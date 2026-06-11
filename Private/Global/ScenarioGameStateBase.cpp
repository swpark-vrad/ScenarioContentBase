#include "Global/ScenarioGameStateBase.h"
#include "Global/ScenarioGameInstanceBase.h"
#include "Actor/ScenarioPhaseBase.h"
#include "Actor/ScenarioEntryBase.h"
#include "Actor/InteractionZoneBase.h"
#include "Actor/ScenarioPatientBase.h"
#include "Data/ScenarioDataTypes.h"
#include "Data/ScenarioSaveTypes.h"
#include "Data/ScenarioUITypes.h"
#include "Data/ScenarioSaveGame.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

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

    DOREPLIFETIME(AScenarioGameStateBase, SpawnedPatient);
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
            if (!Entry->bIsCompleted && Entry->CheckTargetInteraction(Payload))
            {
                Entry->CompleteEntry();
            }

            // 고유 ID 타깃팅 구조이므로 해당 미션을 찾아서 검사했다면 루프를 즉시 탈출하여 연산을 아낍니다.
            break;
        }
    }
}

void AScenarioGameStateBase::RequestCompleteEntryByID(FGameplayTag EntryID)
{
    if (!HasAuthority() || !CurrentPhase) return;

    // 현재 페이즈가 관리하는 실시간 활성 엔트리 목록을 안전하게 내부 검색
    for (AScenarioEntryBase* Entry : CurrentPhase->ActiveEntries)
    {
        if (Entry && Entry->EntryID.MatchesTagExact(EntryID))
        {
            if (!Entry->bIsCompleted)
            {
                // 엔트리 완료 시 내부 순정 규칙에 의해 OnEntryCompleted 델리게이트가 호출되며,
                // 이에 연동된 HandleEntryCompletedFromWorld 및 UpdateEntryUIState가 차례로 자동 실행됩니다.
                Entry->ForceToCompleteEntry();
                UE_LOG(LogTemp, Log, TEXT("ScenarioGS: 외부 요청에 의해 엔트리 [%s] 완료 공정을 수행했습니다."), *EntryID.ToString());
            }
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

void AScenarioGameStateBase::OnRep_SpawnedPatient()
{
    if (SpawnedPatient && OnPatientSpawned.IsBound())
    {
        OnPatientSpawned.Broadcast(SpawnedPatient);
    }
}

void AScenarioGameStateBase::HandleEntryCompletedFromWorld(AScenarioEntryBase* CompletedEntry)
{
    if (!CompletedEntry) return;

    // 1. GameState 내부에서 UI용 레플리케이션 데이터를 최우선으로 최신화합니다.
    UpdateEntryUIState(CompletedEntry->EntryID, true);
    CompletedEntry->OnEntryCompleted.RemoveDynamic(this, &AScenarioGameStateBase::HandleEntryCompletedFromWorld);

    // 2. 구조 개선: UI 상태 동기화가 완결된 확실한 시점에 현재 페이즈의 핸들러 함수를 순차적으로 직접 찔러줍니다.
    // 이를 통해 UI가 갱신되기도 전에 페이즈가 먼저 종료되어 데이터가 꼬이는 레이스 컨디션을 원천 차단합니다.
    if (CurrentPhase)
    {
        CurrentPhase->HandleEntryCompleted(CompletedEntry);
    }
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
    if (!GI || !EntryMasterTable || !ZoneMasterTable || !DefaultPhaseClass || !DefaultPatientClass)
    {
        UE_LOG(LogTemp, Error, TEXT("BuildEnvironment: 필수 인프라 에셋 데이터 테이블 또는 환자 클래스가 누락되었습니다."));
        return;
    }

    ActiveScenarioID = GI->GetCurrentScenarioID();
    if (ActiveScenarioID.IsNone()) return;

    UScenarioSaveGame* SaveGame = GI->LoadScenarioData(ActiveScenarioID);
    if (!SaveGame) return;

    const FScenarioSaveData& ScenarioData = SaveGame->ScenarioData;
    if (ScenarioData.Phases.Num() == 0) return;

    // 지정된 클래스를 기반으로 월드 원점에 환자 생성
    FActorSpawnParameters PatientSpawnParams;
    PatientSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // 환자 안보이는 위치에 스폰
    FVector PatientSpawnLoaction = FVector(0.0f, 0.0f, -10000.0f);
    //FVector PatientSpawnLoaction = FVector(0.0f, 0.0f, 100.0f); // 개발용
    SpawnedPatient = GetWorld()->SpawnActor<AScenarioPatientBase>(DefaultPatientClass, PatientSpawnLoaction, FRotator::ZeroRotator, PatientSpawnParams);

    USkeletalMeshComponent* PatientMesh = nullptr;
    if (SpawnedPatient)
    {
        PatientMesh = SpawnedPatient->TorsoMesh; // 메인 부모 바디로 설정된 TorsoMesh 사용
        // 환자 부위별 상태 적용
        SpawnedPatient->InitializePartState(ScenarioData.PatientPartState);

        UE_LOG(LogTemp, Log, TEXT("GameState: 환자 베이스 액터 동적 스폰 및 데이터 바인딩 완료."));

        OnRep_SpawnedPatient();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GameState: 환자 베이스 액터 스폰에 실패했습니다. 구역 부착을 진행할 수 없습니다."));
        return;
    }

    // 1. 페이즈 생성 루프 시작
    for (const FPhaseSaveData& PhaseData : ScenarioData.Phases)
    {
        FActorSpawnParameters PhaseSpawnParams;
        PhaseSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AScenarioPhaseBase* NewPhase = GetWorld()->SpawnActor<AScenarioPhaseBase>(DefaultPhaseClass, FVector::ZeroVector, FRotator::ZeroRotator, PhaseSpawnParams);
        if (!NewPhase) continue;

        NewPhase->PhaseName = PhaseData.PhaseName;
        NewPhase->TimeLimit = PhaseData.TimeLimit;
        NewPhase->NextSuccessPhaseName = PhaseData.NextSuccessPhaseName;
        NewPhase->NextFailurePhaseName = PhaseData.NextFailurePhaseName;

        // 2. 엔트리 생성 루프 시작
        for (const FEntrySaveData& SaveEntry : PhaseData.Entries)
        {
            if (SaveEntry.EntryRowName.IsNone()) continue;

            FScenarioEntryTableRow* RowData = EntryMasterTable->FindRow<FScenarioEntryTableRow>(SaveEntry.EntryRowName, TEXT("GameState_EntrySetup"));
            if (!RowData || !RowData->EntryClass) continue;

            FActorSpawnParameters EntrySpawnParams;
            EntrySpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            AScenarioEntryBase* NewEntry = GetWorld()->SpawnActor<AScenarioEntryBase>(RowData->EntryClass, FVector::ZeroVector, FRotator::ZeroRotator, EntrySpawnParams);
            if (!NewEntry) continue;

            NewEntry->EntryID = RowData->EntryID;
            NewEntry->EntryName = SaveEntry.EntryRowName;
            NewEntry->bIsMandatory = SaveEntry.bIsMandatory;
            NewEntry->TargetExecutionCount = RowData->TargetExecutionCount;
            NewEntry->TargetInteractionTag = RowData->TargetInteractionTag;

            // 구역 데이터 검색 전 엔트리의 기본 페이즈 오너 연결을 최우선 수행합니다.
            NewEntry->InitializeEntry(NewPhase, nullptr);

            // 3. 신규 FZoneDataWrapper 규격 기반 하이브리드 공간 조립 공정
            FZoneSpawnRow* ZoneRow = ZoneMasterTable->FindRow<FZoneSpawnRow>(SaveEntry.EntryRowName, TEXT("GameState_ZoneSetup"));

            if (ZoneRow)
            {
                for (const FZoneDataWrapper& Wrapper : ZoneRow->ZoneDatas)
                {
                    TSoftClassPtr<AInteractionZoneBase> SoftClassToSpawn = Wrapper.ZoneClass;

                    if (!SoftClassToSpawn.IsNull())
                    {
                        UClass* LoadedZoneClass = SoftClassToSpawn.LoadSynchronous();
                        if (LoadedZoneClass)
                        {
                            FTransform InitialTransform = FTransform::Identity;
                            USceneComponent* AttachTargetComponent = nullptr;

                            // Wrapper 내부에 이관된 AnchorType에 맞게 분기 연산 실행
                            switch (Wrapper.AnchorType)
                            {
                            case EZoneAnchorType::Patient:
                            {
                                /*if (PatientMesh && !Wrapper.ZoneData.TargetSocket.IsNone())
                                {
                                    InitialTransform = PatientMesh->GetSocketTransform(Wrapper.ZoneData.TargetSocket);
                                }*/
                                if (PatientMesh)
                                {
                                    // 소켓 트랜스폼에 상대 오프셋을 먼저 곱해 최종 월드 트랜스폼을 미리 계산합니다.
                                    FTransform BaseTransform = Wrapper.ZoneData.TargetSocket.IsNone() ? PatientMesh->GetComponentTransform() : PatientMesh->GetSocketTransform(Wrapper.ZoneData.TargetSocket);
                                    InitialTransform = Wrapper.ZoneData.RelativeOffset * BaseTransform;
                                }
                                AttachTargetComponent = PatientMesh;
                                break;
                            }
                            case EZoneAnchorType::StaticWorld:
                            {
                                InitialTransform = Wrapper.ZoneData.RelativeOffset;
                                break;
                            }
                            case EZoneAnchorType::AttachedObject:
                            {
                                if (!Wrapper.AnchorObjectTag.IsNone())
                                {
                                    TArray<AActor*> AllActors;
                                    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);

                                    for (AActor* Actor : AllActors)
                                    {
                                        if (Actor && Actor->ActorHasTag(Wrapper.AnchorObjectTag))
                                        {
                                            AttachTargetComponent = Actor->GetRootComponent();

                                            if (!Wrapper.ZoneData.TargetSocket.IsNone() && AttachTargetComponent)
                                            {
                                                InitialTransform = AttachTargetComponent->GetSocketTransform(Wrapper.ZoneData.TargetSocket);
                                            }
                                            else
                                            {
                                                InitialTransform = Actor->GetActorTransform();
                                            }
                                            break;
                                        }
                                    }
                                }
                                break;
                            }
                            }
                            // 스폰 스케일은 1로 고정
                            InitialTransform.SetScale3D(FVector(1.0f));

                            // 지연 스폰 가동 후 고유 설정 정보 주입
                            AInteractionZoneBase* SpawnedZone = GetWorld()->SpawnActorDeferred<AInteractionZoneBase>(
                                LoadedZoneClass, InitialTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
                            );

                            if (SpawnedZone)
                            {
                                SpawnedZone->ZoneID = Wrapper.ZoneID;
                                SpawnedZone->ZoneData = Wrapper.ZoneData;
                                SpawnedZone->FinishSpawning(InitialTransform);

                                // 환자 인터페이스 캐싱
                                if (SpawnedPatient)
                                {
                                    SpawnedZone->SetPatientActor(SpawnedPatient);
                                }

                                // 힌트 활성화를 위한 캐싱
                                FGameplayTag TargetTag = SpawnedZone->ZoneData.HintTargetTag;

                                // 유효한 태그가 세팅되어 있다면 GameState의 전역 맵 장부에 주소를 쏙 집어넣습니다.
                                if (TargetTag.IsValid())
                                {
                                    ActiveHintRegistry.FindOrAdd(TargetTag).InteractionZoneActors.AddUnique(SpawnedZone);
                                }

                                SpawnedZone->FinishSpawning(FTransform::Identity);

                                if (AttachTargetComponent)
                                {
                                    /*FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
                                    SpawnedZone->AttachToComponent(AttachTargetComponent, AttachRules, Wrapper.ZoneData.TargetSocket);
                                    SpawnedZone->SetActorRelativeTransform(Wrapper.ZoneData.RelativeOffset);*/
                                    // InitialTransform으로 이미 계산했기 때문에 KeepWorld로 어태치
                                    FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
                                    SpawnedZone->AttachToComponent(AttachTargetComponent, AttachRules, Wrapper.ZoneData.TargetSocket);
                                }
                                else if (Wrapper.AnchorType == EZoneAnchorType::StaticWorld)
                                {
                                    SpawnedZone->SetActorTransform(Wrapper.ZoneData.RelativeOffset);
                                }

                                // 생성 완료된 구역 객체를 해당 엔트리의 신호 채널(ProcessPayload)에 즉시 누적 바인딩합니다.
                                NewEntry->InitializeEntry(NewPhase, SpawnedZone);

                                // 다중 구역 관리용 유니크 키 조합 생성 (EntryID + ZoneID)
                                FName UniqueZoneKey = FName(*(RowData->EntryID.ToString() + TEXT("_") + Wrapper.ZoneID.ToString() + TEXT("_Zone")));
                                GlobalActiveZones.Add(UniqueZoneKey, SpawnedZone);
                                NewPhase->ActiveZones.Add(UniqueZoneKey, SpawnedZone);

                                UE_LOG(LogTemp, Log, TEXT("GameState: [구역 조립 완료] 엔트리 행: %s, 구역 ID: %s, 앵커: %d"),
                                    *SaveEntry.EntryRowName.ToString(), *Wrapper.ZoneID.ToString(), (int32)Wrapper.AnchorType);
                            }
                        }
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("GameState: 구역 테이블에서 [%s] 키에 매칭되는 행 데이터를 찾지 못했습니다."), *SaveEntry.EntryRowName.ToString());
            }

            NewPhase->ActiveEntries.Add(NewEntry);
        }

        ScenarioPhases.Add(NewPhase);
    }


    UE_LOG(LogTemp, Log, TEXT("========================================================================="));
    UE_LOG(LogTemp, Log, TEXT("GameState: 최신 메타데이터 구조 기반 다목적 가상 시뮬레이터 환경 조립 성공."));
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
        if (Entry && Entry->EntryID.IsValid())
        {
            ActiveEntryMap.Add(Entry->EntryID, Entry);

            // GS의 UI 갱신 함수 바인딩
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

void AScenarioGameStateBase::ReceiveGrabSignal(FGameplayTag GrabbedTag)
{
    if (!HasAuthority()) return;
    // 전달 받은 태그가 추가되어있는지 확인
    if (GrabbedTag.IsValid() && ActiveHintRegistry.Contains(GrabbedTag))
    {
        // 있다면 등록된 IZ 모두 순회하며 힌트 활성화
        for (AInteractionZoneBase* NewZone : ActiveHintRegistry[GrabbedTag].InteractionZoneActors)
        {
            if (IsValid(NewZone))
            {
                NewZone->Multicast_SetActivateHint(true);
            }
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
            UIData.EntryID = Entry->EntryID;
            UIData.EntryName = Entry->EntryName;
            UIData.bIsMandatory = Entry->bIsMandatory;
            UIData.bIsCompleted = false;
            UIData.CompletionTime = 0;

            CurrentEntryDatas.Add(UIData);
        }
    }

    OnRep_CurrentEntryDatas();
}

void AScenarioGameStateBase::UpdateEntryUIState(FGameplayTag EntryID, bool bCompleted)
{
    if (!HasAuthority()) return;

    bool bIsChanged = false;

    for (FScenarioEntryUIData& UIData : CurrentEntryDatas)
    {
        if (UIData.EntryID.MatchesTagExact(EntryID))
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

void AScenarioGameStateBase::ActivatePatient_Implementation(USceneComponent* InParentComponent)
{
    // 스폰되어 대기 중인 환자 액터가 정상적으로 존재하는지 검증
    if (!SpawnedPatient)
    {
        UE_LOG(LogTemp, Warning, TEXT("ActivatePatient: 가상 환경에 스폰된 환자(SpawnedPatient)가 존재하지 않습니다."));
        return;
    }

    // 환자를 부착시킬 침대나 특정 구역의 부모 컴포넌트 유효성 검증
    if (!InParentComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("ActivatePatient: 파라미터로 전달된 InParentComponent가 nullptr입니다. 부착을 취소합니다."));
        return;
    }

    // 부착 규칙 수립: 부모 컴포넌트의 위치와 회전에 강제로 일치시킴
    FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget, // 위치 스냅
        EAttachmentRule::SnapToTarget, // 회전 스냅
        EAttachmentRule::KeepWorld,    // 메시 왜곡 방지를 위해 스케일은 월드 값 유지
        false
    );

    // 총괄 관리자인 GameState가 명령을 내려 환자 액터 본체를 부모 컴포넌트에 하위 종속시킵니다.
    SpawnedPatient->AttachToComponent(InParentComponent, AttachRules);
    SpawnedPatient->ActivatePatient();

    UE_LOG(LogTemp, Log, TEXT("GameState: 환자를 지정된 컴포넌트에 배치하고 시나리오 내 활성화를 완료했습니다."));
}

bool AScenarioGameStateBase::GetTreatmentVisualData(FGameplayTag TreatmentTag, FTreatmentVisuals& OutVisualData) const
{
    if (!TreatmentVisualTable || !TreatmentTag.IsValid()) return false;

    TArray<FTreatmentVisuals*> AllRows;
    TreatmentVisualTable->GetAllRows(TEXT("FindVisualContext"), AllRows);

    // 테이블 내의 모든 행을 순회하며 ObjectID 태그가 일치하는 데이터를 검색합니다.
    for (FTreatmentVisuals* Row : AllRows)
    {
        if (Row && Row->ObjectID == TreatmentTag)
        {
            OutVisualData = *Row;
            return true;
        }
    }

    return false;
}
