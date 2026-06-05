#include "Global/ScenarioGameStateBase.h"
#include "Global/ScenarioGameInstanceBase.h"
#include "Actor/ScenarioPhaseBase.h"
#include "Actor/ScenarioEntryBase.h"
#include "Data/ScenarioDataTypes.h"
#include "Data/ScenarioSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"

AScenarioGameStateBase::AScenarioGameStateBase()
{
    ActiveScenarioID = NAME_None;
    StartDateTime = FDateTime::MinValue();
    ProgressTime = 0;
    CurrentPhaseName = NAME_None;
    CurrentPhaseTotalTime = 0;
    CurrentPhaseRemainingTime = 0;
    bIsStarted = false;
    bIsPaused = false;
}

void AScenarioGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AScenarioGameStateBase, ActiveScenarioID);
    DOREPLIFETIME(AScenarioGameStateBase, StartDateTime);
    DOREPLIFETIME(AScenarioGameStateBase, ProgressTime);
    DOREPLIFETIME(AScenarioGameStateBase, CurrentPhaseName);
    DOREPLIFETIME(AScenarioGameStateBase, CurrentPhaseTotalTime);
    DOREPLIFETIME(AScenarioGameStateBase, CurrentPhaseRemainingTime);
    DOREPLIFETIME(AScenarioGameStateBase, CurrentEntryDatas);

    // 늦게 진입한 유저를 위해 시작 변수와 일시정지 변수를 함께 복제 목록에 등록합니다
    DOREPLIFETIME(AScenarioGameStateBase, bIsStarted);
    DOREPLIFETIME(AScenarioGameStateBase, bIsPaused);
}

void AScenarioGameStateBase::OnPlayerIdentityReady(APlayerState* PlayerState)
{
    if (IsValid(PlayerState) && OnUserAdded.IsBound())
    {
        OnUserAdded.Broadcast(PlayerState);
    }
}

void AScenarioGameStateBase::RemovePlayerState(APlayerState* PlayerState)
{
    if (IsValid(PlayerState) && OnUserRemoved.IsBound())
    {
        OnUserRemoved.Broadcast(PlayerState);
    }
    Super::RemovePlayerState(PlayerState);
}

void AScenarioGameStateBase::OnRep_ActiveScenarioID()
{
    UScenarioGameInstanceBase* MyGI = Cast<UScenarioGameInstanceBase>(GetGameInstance());
    if (MyGI && !ActiveScenarioID.IsNone())
    {
        MyGI->SetCurrentScenarioID(ActiveScenarioID);
        MyGI->LoadScenarioToMemory();
        if (OnScenarioDataReady.IsBound())
        {
            OnScenarioDataReady.Broadcast();
        }
    }
}

void AScenarioGameStateBase::StartScenarioClock()
{
    if (!HasAuthority()) return;

    StartDateTime = FDateTime::Now();
    ProgressTime = 0;

    bIsPaused = false;
    OnRep_bIsPaused();

    if (OnClockUpdated.IsBound())
    {
        OnClockUpdated.Broadcast(ProgressTime);
    }

    GetWorldTimerManager().SetTimer(ClockTimerHandle, this, &AScenarioGameStateBase::UpdateScenarioClock, 1.0f, true);
}

void AScenarioGameStateBase::StopScenarioClock()
{
    if (!HasAuthority()) return;

    bIsStarted = false;
    bIsPaused = false;
    OnRep_bIsStarted();
    OnRep_bIsPaused();

    if (GetWorldTimerManager().IsTimerActive(ClockTimerHandle))
    {
        GetWorldTimerManager().ClearTimer(ClockTimerHandle);
    }
}

void AScenarioGameStateBase::OnRep_CurrentPhaseRemainingTime()
{
    if (OnPhaseTimeUpdated.IsBound())
    {
        float RemainingRatio = 1.0f;

        if (CurrentPhaseTotalTime > 0)
        {
            // 남은 초를 총 제한시간 초로 나누어 0.0 ~ 1.0 사이의 float 비율을 산출합니다.
            RemainingRatio = static_cast<float>(CurrentPhaseRemainingTime) / static_cast<float>(CurrentPhaseTotalTime);
        }
        else if (CurrentPhaseTotalTime == -1)
        {
            // 무제한 페이즈일 경우 UI 연출 분기를 위해 비율도 -1.0을 리턴합니다.
            RemainingRatio = -1.0f;
        }

        // 시간 데이터와 가공된 비율 데이터를 결합하여 전역 브로드캐스트 수행
        OnPhaseTimeUpdated.Broadcast(CurrentPhaseRemainingTime, RemainingRatio);
    }
}

FText AScenarioGameStateBase::GetFormattedPhaseTimeText() const
{
    // 페이즈가 가동 전이거나 제한시간이 없는 무제한 상태일 경우 예외 처리
    if (CurrentPhaseTotalTime <= 0 || CurrentPhaseRemainingTime < 0)
    {
        return FText::FromString(TEXT("--:-- / --:--"));
    }

    // 좌측에 표시할 현재 페이즈의 남은 시간 파싱
    int32 RemainingMin = CurrentPhaseRemainingTime / 60;
    int32 RemainingSec = CurrentPhaseRemainingTime % 60;

    // 우측에 표시할 현재 페이즈의 총 제한 시간 파싱
    int32 TotalMin = CurrentPhaseTotalTime / 60;
    int32 TotalSec = CurrentPhaseTotalTime % 60;

    // 두 데이터를 MM:SS / MM:SS 형태로 결합하여 반환
    FString CombinedStr = FString::Printf(TEXT("%02d:%02d / %02d:%02d"), RemainingMin, RemainingSec, TotalMin, TotalSec);
    return FText::FromString(CombinedStr);
}

void AScenarioGameStateBase::ProcessInteractionPayload(const FInteractionPayload& Payload)
{
    // 서버 권한을 가지고 있고, 현재 가동 중인 페이즈가 명확히 존재할 때만 로직을 실행합니다.
    if (!HasAuthority() || !CurrentPhase) return;

    // 현재 페이즈가 관리하는 월드의 활성화된 엔트리 액터 목록을 순회합니다.
    for (AScenarioEntryBase* Entry : CurrentPhase->ActiveEntries)
    {
        if (!Entry) continue;

        // 페이로드에 담겨온 UniqueID와 엔트리 액터의 고유 EntryID가 일치하는지 라우팅 검사를 합니다.
        if (Entry->EntryID == Payload.UniqueID)
        {
            // 아직 미완료 상태이고, 해당 엔트리의 고유 성공 조건(오버라이드 함수)을 통과하면 완료 처리합니다.
            if (!Entry->bIsCompleted && Entry->CheckSuccessCondition(Payload))
            {
                Entry->CompleteEntry();
            }

            // 고유 ID 타깃팅 구조이므로 해당 미션을 찾아서 검사했다면 루프를 즉시 탈출하여 연산을 아낍니다.
            break;
        }
    }
}

void AScenarioGameStateBase::UpdateScenarioClock()
{
    if (!HasAuthority()) return;

    ProgressTime++;
    if (OnClockUpdated.IsBound())
    {
        OnClockUpdated.Broadcast(ProgressTime);
    }

    // 페이즈가 가동 중이고 제한시간이 설정된 상태(0보다 큼)인 경우 매초 차감
    if (bIsStarted && !bIsPaused && CurrentPhase && CurrentPhaseRemainingTime > 0)
    {
        CurrentPhaseRemainingTime--;
        OnRep_CurrentPhaseRemainingTime();

        // 0초에 도달하는 순간 즉시 시간 초과 실패 판정 호출
        if (CurrentPhaseRemainingTime == 0)
        {
            HandlePhaseCompleted(CurrentPhase, false);
        }
    }
}

void AScenarioGameStateBase::OnRep_ProgressTime()
{
    if (OnClockUpdated.IsBound())
    {
        OnClockUpdated.Broadcast(ProgressTime);
    }
}

void AScenarioGameStateBase::HandleEntryCompletedFromWorld(AScenarioEntryBase* CompletedEntry)
{
    if (!CompletedEntry) return;

    // 엔트리가 쏜 신호를 받아 GameState가 내부에서 안전하게 UI 배열 상태를 업데이트합니다.
    UpdateEntryUIState(CompletedEntry->EntryName, true);
    // 체크되었다면 체크시 호출될 델리게이트 언바인딩
    CompletedEntry->OnEntryCompleted.RemoveDynamic(this, &AScenarioGameStateBase::HandleEntryCompletedFromWorld);
}

void AScenarioGameStateBase::NotifyDataReadyToLocalClients()
{
    if (OnScenarioDataReady.IsBound())
    {
        OnScenarioDataReady.Broadcast();
    }
}

// ==========================================
// 글로벌 존 및 페이즈 관리 구현부
// ==========================================

void AScenarioGameStateBase::OnRep_bIsStarted()
{
    if (OnScenarioStarted.IsBound())
    {
        OnScenarioStarted.Broadcast(bIsStarted);
    }
}

void AScenarioGameStateBase::OnRep_bIsPaused()
{
    if (OnScenarioPaused.IsBound())
    {
        OnScenarioPaused.Broadcast(bIsPaused);
    }
}

void AScenarioGameStateBase::OnRep_CurrentPhaseName()
{
    // 페이즈 변수가 복제되어 도달하면 모니터 및 호스트 UI에 시작 알림을 브로드캐스트합니다.
    if (OnPhaseStarted.IsBound())
    {
        OnPhaseStarted.Broadcast(CurrentPhaseName);
    }
}

void AScenarioGameStateBase::BuildGlobalScenarioEnvironment()
{
    if (!HasAuthority()) return;

    UScenarioGameInstanceBase* GI = Cast<UScenarioGameInstanceBase>(GetGameInstance());
    if (!GI || !EntryMasterTable || !DefaultPhaseClass)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildEnvironment: 필수 인프라 에셋이 누락되었습니다."));
        return;
    }

    ActiveScenarioID = GI->GetCurrentScenarioID();
    if (ActiveScenarioID.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("BuildEnvironment: GameInstance의 CurrentScenarioID가 비어있습니다."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("========================================================================="));
    UE_LOG(LogTemp, Log, TEXT("▶ [START] 시나리오 월드 생성 및 데이터 주입 시작 (ID: %s)"), *ActiveScenarioID.ToString());
    UE_LOG(LogTemp, Log, TEXT("========================================================================="));

    UScenarioSaveGame* SaveGame = GI->LoadScenarioData(ActiveScenarioID);
    if (!SaveGame)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildEnvironment: %s 세이브 파일을 읽어오지 못했습니다."), *ActiveScenarioID.ToString());
        return;
    }

    const FScenarioSaveData& ScenarioData = SaveGame->ScenarioData;
    if (ScenarioData.Phases.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("BuildEnvironment: 세이브 파일에 배치된 페이즈가 없습니다."));
        return;
    }

    // 환경만 구축하는 단계이므로 시작 플래그(bIsStarted) 조작 및 StartPhase 호출을 전면 제거합니다.
    for (const FPhaseSaveData& PhaseData : ScenarioData.Phases)
    {
        FActorSpawnParameters PhaseSpawnParams;
        PhaseSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AScenarioPhaseBase* NewPhase = GetWorld()->SpawnActor<AScenarioPhaseBase>(DefaultPhaseClass, FVector::ZeroVector, FRotator::ZeroRotator, PhaseSpawnParams);
        if (!NewPhase)
        {
            UE_LOG(LogTemp, Error, TEXT("BuildEnvironment: 페이즈 액터 스폰 실패: %s"), *PhaseData.PhaseName.ToString());
            continue;
        }

        NewPhase->PhaseName = PhaseData.PhaseName;
        NewPhase->TimeLimit = PhaseData.TimeLimit;
        NewPhase->NextSuccessPhaseName = PhaseData.NextSuccessPhaseName;
        NewPhase->NextFailurePhaseName = PhaseData.NextFailurePhaseName;

        UE_LOG(LogTemp, Log, TEXT("[Phase 생성] 명칭: %s (제한시간: %.1f초) | [성공시 ➡️ %s] [실패시 ➡️ %s]"),
            *PhaseData.PhaseName.ToString(),
            PhaseData.TimeLimit,
            PhaseData.NextSuccessPhaseName.IsNone() ? TEXT("시나리오 종료") : *PhaseData.NextSuccessPhaseName.ToString(),
            PhaseData.NextFailurePhaseName.IsNone() ? TEXT("시나리오 종료") : *PhaseData.NextFailurePhaseName.ToString());

        for (const FEntrySaveData& SaveEntry : PhaseData.Entries)
        {
            if (SaveEntry.EntryRowName.IsNone()) continue;

            FScenarioEntryTableRow* RowData = EntryMasterTable->FindRow<FScenarioEntryTableRow>(SaveEntry.EntryRowName, TEXT("GameState_EntrySetup"));
            if (!RowData || !RowData->EntryClass)
            {
                UE_LOG(LogTemp, Warning, TEXT("  ㄴ ❌ [Master 테이블 조회 실패] 행 이름 '%s' 데이터가 마스터 테이블에 없습니다."), *SaveEntry.EntryRowName.ToString());
                continue;
            }

            FActorSpawnParameters EntrySpawnParams;
            EntrySpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            AScenarioEntryBase* NewEntry = GetWorld()->SpawnActor<AScenarioEntryBase>(RowData->EntryClass, FVector::ZeroVector, FRotator::ZeroRotator, EntrySpawnParams);
            if (!NewEntry)
            {
                UE_LOG(LogTemp, Error, TEXT("  ㄴ ❌ [스폰 실패] 엔트리 액터 인스턴스 생성 실패: %s"), *SaveEntry.EntryRowName.ToString());
                continue;
            }

            NewEntry->EntryID = RowData->EntryID;
            NewEntry->EntryName = SaveEntry.EntryRowName;
            NewEntry->bIsMandatory = SaveEntry.bIsMandatory;
            NewEntry->TargetExecutionCount = RowData->TargetExecutionCount;
            NewEntry->TargetInteractionTag = RowData->TargetInteractionTag;

            // Zone 관련 로직을 제거하고, 기존 함수 호환성을 위해 nullptr을 대입합니다.
            NewEntry->InitializeEntry(NewPhase, nullptr);

            NewPhase->ActiveEntries.Add(NewEntry);

            // 로그 출력문에서 구역 표시 정보를 지우고 간소화합니다.
            UE_LOG(LogTemp, Log, TEXT("  ㄴ 🟢 [Entry 추가 완료] 명칭: %s | 필수: %s | 목표 횟수: %d회"),
                *SaveEntry.EntryRowName.ToString(),
                SaveEntry.bIsMandatory ? TEXT("필수(Mandatory)") : TEXT("옵션(Optional)"),
                RowData->TargetExecutionCount);
        }

        ScenarioPhases.Add(NewPhase);
    }

    UE_LOG(LogTemp, Log, TEXT("========================================================================="));
    UE_LOG(LogTemp, Log, TEXT("▶ [SUCCESS] 모든 가상 환경 조립 완료. 사용자의 시작 명령을 대기합니다."));
    UE_LOG(LogTemp, Log, TEXT("========================================================================="));
}

void AScenarioGameStateBase::StartScenario_Implementation()
{
    if (!HasAuthority() || bIsStarted || ScenarioPhases.Num() == 0) return;

    // 사용자가 시작을 누른 정식 구동 시점이므로 복제 상태를 true로 전환하여 전파합니다
    bIsStarted = true;
    bIsPaused = false;
    OnRep_bIsStarted();

    // 시나리오 타이머 시계를 작동시킵니다
    StartScenarioClock();

    // 미리 준비되어 대기 중이던 샌드박스 풀의 최상단 0번째 페이즈 이름을 찔러 실습을 개시합니다
    if (ScenarioPhases[0])
    {
        StartPhaseByName(ScenarioPhases[0]->PhaseName);
    }
}

void AScenarioGameStateBase::StartPhaseByName(FName PhaseName)
{
    if (!HasAuthority() || PhaseName.IsNone()) return;

    AScenarioPhaseBase* FoundPhase = nullptr;
    for (AScenarioPhaseBase* Phase : ScenarioPhases)
    {
        if (Phase && Phase->PhaseName == PhaseName)
        {
            FoundPhase = Phase;
            break;
        }
    }

    if (!FoundPhase)
    {
        UE_LOG(LogTemp, Error, TEXT("StartPhaseByName: 해당하는 페이즈 노드를 찾지 못함: %s"), *PhaseName.ToString());
        return;
    }

    CurrentPhaseName = PhaseName;
    CurrentPhase = FoundPhase;

    if (CurrentPhase->TimeLimit > 0.0f)
    {
        // 총 시간과 남은 시간을 동일하게 제한시간 초 단위 정수로 장전합니다.
        CurrentPhaseTotalTime = FMath::RoundToInt(CurrentPhase->TimeLimit);
        CurrentPhaseRemainingTime = CurrentPhaseTotalTime;
    }
    else
    {
        // 제한시간이 없는 무제한 페이즈인 경우 예외 값 설정
        CurrentPhaseTotalTime = -1;
        CurrentPhaseRemainingTime = -1;
    }

    CurrentPhase->OnPhaseCompleted.AddDynamic(this, &AScenarioGameStateBase::HandlePhaseCompleted);

    ActiveEntryMap.Empty();
    for (AScenarioEntryBase* Entry : CurrentPhase->ActiveEntries)
    {
        if (Entry && !Entry->EntryID.IsValid())
        {
            // 엔트리가 가진 고유 GameplayTag를 Key로 삼아 맵에 캐싱합니다.
            ActiveEntryMap.Add(Entry->EntryID, Entry);
            Entry->OnEntryCompleted.AddDynamic(this, &AScenarioGameStateBase::HandleEntryCompletedFromWorld);
        }
    }

    // UI에 복사해 줄 엔트리 배열 데이터를 최신화합니다.
    InitializePhaseEntryDatas();

    // 서버 호스트 본인의 로컬 위젯 화면들을 갱신하기 위해 콜백들을 수동 트리거합니다.
    OnRep_CurrentPhaseName();

    CurrentPhase->StartPhase();
}

void AScenarioGameStateBase::HandlePhaseCompleted(AScenarioPhaseBase* CompletedPhase, bool bIsSuccess)
{
    if (!HasAuthority() || !CompletedPhase) return;

    // 기존 페이즈의 엔트리들 체크 델리게이트 언바인딩
    for (AScenarioEntryBase* Entry : CompletedPhase->ActiveEntries)
    {
        if (Entry)
        {
            Entry->OnEntryCompleted.RemoveDynamic(this, &AScenarioGameStateBase::HandleEntryCompletedFromWorld);
        }
    }

    if (bIsSuccess)
    {
        CompletedPhase->OnPhaseCompleted.RemoveDynamic(this, &AScenarioGameStateBase::HandlePhaseCompleted);

        FPhaseHistoryData HistoryData;
        HistoryData.Entries = CurrentEntryDatas;
        PhaseHistoryMap.Add(CompletedPhase->PhaseName, HistoryData);

        FName NextPhaseName = CompletedPhase->NextSuccessPhaseName;

        if (NextPhaseName == FName("End"))
        {
            StopScenarioClock();
            UE_LOG(LogTemp, Log, TEXT("시나리오 최종 성공 완료 - 명시적 종료 단계를 시작합니다."));
        }
        else if (!NextPhaseName.IsNone())
        {
            StartPhaseByName(NextPhaseName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("데이터 오류: %s 페이즈 성공 후 다음 목적지가 설정되지 않았습니다."), *CompletedPhase->PhaseName.ToString());
        }
    }
    else
    {
        FName NextPhaseName = CompletedPhase->NextFailurePhaseName;

        if (!NextPhaseName.IsNone())
        {
            CompletedPhase->OnPhaseCompleted.RemoveDynamic(this, &AScenarioGameStateBase::HandlePhaseCompleted);

            FPhaseHistoryData HistoryData;
            HistoryData.Entries = CurrentEntryDatas;
            PhaseHistoryMap.Add(CompletedPhase->PhaseName, HistoryData);

            if (NextPhaseName == FName("End"))
            {
                StopScenarioClock();
                UE_LOG(LogTemp, Log, TEXT("시나리오 최종 실패 종료 - 명시적 종료 단계를 시작합니다."));
            }
            else
            {
                StartPhaseByName(NextPhaseName);
            }
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("실패 분기 미설정: 현재 페이즈(%s)에 대기하며 실습을 계속 진행합니다."), *CompletedPhase->PhaseName.ToString());
        }
    }
}

void AScenarioGameStateBase::PauseScenario()
{
    if (!HasAuthority() || !bIsStarted || bIsPaused) return;

    bIsPaused = true;
    GetWorldTimerManager().PauseTimer(ClockTimerHandle);
    OnRep_bIsPaused();
}

void AScenarioGameStateBase::ResumeScenario()
{
    if (!HasAuthority() || !bIsStarted || !bIsPaused) return;

    bIsPaused = false;
    GetWorldTimerManager().UnPauseTimer(ClockTimerHandle);
    OnRep_bIsPaused();
}

// ==========================================
// 통합 UI 배열 및 히스토리 관리
// ==========================================

void AScenarioGameStateBase::InitializePhaseEntryDatas()
{
    if (!HasAuthority() || !CurrentPhase) return;

    CurrentEntryDatas.Empty();

    for (AScenarioEntryBase* Entry : CurrentPhase->ActiveEntries)
    {
        if (Entry)
        {
            FScenarioEntryUIData UIData;
            UIData.EntryName = Entry->EntryName;
            UIData.bIsMandatory = Entry->bIsMandatory;
            UIData.bIsCompleted = false;
            UIData.CompletionTime = 0;

            CurrentEntryDatas.Add(UIData);
        }
    }

    OnRep_CurrentEntryDatas();
}

void AScenarioGameStateBase::UpdateEntryUIState(FName EntryName, bool bCompleted)
{
    if (!HasAuthority()) return;

    bool bIsChanged = false;

    for (FScenarioEntryUIData& UIData : CurrentEntryDatas)
    {
        if (UIData.EntryName == EntryName)
        {
            UIData.bIsCompleted = bCompleted;

            // 완료(true) 상태일 때만 총 시간에서 남은 시간을 빼서 경과 시간을 산출합니다.
            int32 CalculatedElapsedTime = 0;
            if (bCompleted && CurrentPhaseTotalTime > 0)
            {
                CalculatedElapsedTime = CurrentPhaseTotalTime - CurrentPhaseRemainingTime;
            }

            // 제한시간이 없는 무제한 페이즈(-1)의 경우 경과 시간은 0으로 기록됩니다.
            UIData.CompletionTime = CalculatedElapsedTime;
            bIsChanged = true;
            break;
        }
    }

    if (bIsChanged)
    {
        OnRep_CurrentEntryDatas();
    }
}

void AScenarioGameStateBase::OnRep_CurrentEntryDatas()
{
    // 엔트리 리스트 배열 데이터가 복제 완료되면 UI 컴포넌트들에게 최종 갱신 명령을 내립니다.
    if (OnEntryDatasUpdated.IsBound())
    {
        OnEntryDatasUpdated.Broadcast();
    }
}