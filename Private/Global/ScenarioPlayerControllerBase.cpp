// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/ScenarioPlayerControllerBase.h"
#include "Global/ScenarioPlayerStateBase.h"
#include "Global/ScenarioGameInstanceBase.h"
#include "Global/ScenarioGameStateBase.h"
#include "Actor/ScenarioEntryBase.h"
#include "Actor/ScenarioPhaseBase.h"
#include "GameFramework/GameStateBase.h"
#include "LogSystem/ScenarioLogSubsystem.h"


AScenarioPlayerControllerBase::AScenarioPlayerControllerBase()
{
}


void AScenarioPlayerControllerBase::RequestStartScenario()
{
    // 로컬 클라이언트(UI)가 호출하면 서버 RPC를 가동합니다.
    Server_RequestStartScenario();
}

bool AScenarioPlayerControllerBase::Server_RequestStartScenario_Validate()
{
    // 정식 가동 시에는 이 컨트롤러를 쥔 유저가 진짜 방장(호스트)인지 검증하는 로직을 넣을 수 있습니다.
    return true;
}

void AScenarioPlayerControllerBase::Server_RequestStartScenario_Implementation()
{
    // 서버 권한 진입 완료
    AScenarioGameStateBase* GS = GetWorld()->GetGameState<AScenarioGameStateBase>();
    if (GS)
    {
        // 안전하게 서버 측 게임스테이트를 통해 환경 조립을 시작합니다.
        GS->StartScenario();
    }
}

void AScenarioPlayerControllerBase::Server_RequestCompleteEntry_Implementation(FGameplayTag EntryID)
{
    if (!HasAuthority()) return;

    UWorld* World = GetWorld();
    if (!World) return;

    AScenarioGameStateBase* GS = World->GetGameState<AScenarioGameStateBase>();
    if (GS)
    {
        // PlayerController가 직접 내부 배열을 훑지 않고 GameState의 공용 함수를 경유하도록 캡슐화 단계를 정립합니다.
        GS->RequestCompleteEntryByID(EntryID);
    }
}

void AScenarioPlayerControllerBase::RequestPauseScenario()
{
    Server_RequestPauseScenario();
}

bool AScenarioPlayerControllerBase::Server_RequestPauseScenario_Validate()
{
    // 방장 권한을 검증하거나 일시정지 가능한 상태인지 체크하는 로직 배치 가능
    return true;
}

void AScenarioPlayerControllerBase::Server_RequestPauseScenario_Implementation()
{
    AScenarioGameStateBase* GS = GetWorld()->GetGameState<AScenarioGameStateBase>();
    if (GS)
    {
        // 서버 권한 검증을 통과한 후 GameState의 일시정지 엔진 가동
        GS->PauseScenario();
    }
}

void AScenarioPlayerControllerBase::RequestResumeScenario()
{
    Server_RequestResumeScenario();
}


bool AScenarioPlayerControllerBase::Server_RequestResumeScenario_Validate()
{
    return true;
}

void AScenarioPlayerControllerBase::Server_RequestResumeScenario_Implementation()
{
    AScenarioGameStateBase* GS = GetWorld()->GetGameState<AScenarioGameStateBase>();
    if (GS)
    {
        // 서버 권한 검증을 통과한 후 GameState의 재개 엔진 가동
        GS->ResumeScenario();
    }
}

void AScenarioPlayerControllerBase::RequestSwitchPauseScenario()
{
    Server_RequestSwitchPauseScenario_Implementation();
}

bool AScenarioPlayerControllerBase::Server_RequestSwitchPauseScenario_Validate()
{
    return true;
}

void AScenarioPlayerControllerBase::Server_RequestSwitchPauseScenario_Implementation()
{
    AScenarioGameStateBase* GS = GetWorld()->GetGameState<AScenarioGameStateBase>();
    if (GS->bIsPaused)
    {

        GS->ResumeScenario();
    }
    else
    {
        GS->PauseScenario();
    }
}

void AScenarioPlayerControllerBase::Client_ForceMoveToClientLobby_Implementation()
{
    // 이 안의 로직은 오직 클라이언트 컴퓨터에서만 실행됩니다.
    UScenarioGameInstanceBase* MyGI = Cast<UScenarioGameInstanceBase>(GetGameInstance());
    if (IsValid(MyGI))
    {
        MyGI->LeaveSessionAsClient(this);
    }
}

void AScenarioPlayerControllerBase::Server_RequestShutdownSession_Implementation()
{
    if (!HasAuthority()) return;

    // GameState 유효성 검사
    AGameStateBase* GS = GetWorld()->GetGameState();
    if (!IsValid(GS)) return;

    // 엔진 내장 플레이어 컨트롤러 전용 반복자를 사용합니다.
    for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        // Iterator->Get()을 통해 APlayerController 포인터를 안전하게 꺼냅니다.
        AScenarioPlayerControllerBase* ScenarioPC = Cast<AScenarioPlayerControllerBase>(Iterator->Get());

        // 원격 클라이언트 컨트롤러만 골라내어 클라이언트 로비로 추방합니다.
        if (IsValid(ScenarioPC) && !ScenarioPC->IsLocalPlayerController())
        {
            ScenarioPC->Client_ForceMoveToClientLobby();
        }
    }

    // 호스트 본인도 자체 게임인스턴스를 통해 호스트 로비로 이동하며 세션을 종료합니다.
    UScenarioGameInstanceBase* MyGI = Cast<UScenarioGameInstanceBase>(GetGameInstance());
    if (IsValid(MyGI))
    {
        MyGI->ShutdownSessionAsHost();
    }
}

bool AScenarioPlayerControllerBase::Server_RequestLog_Validate(const FString& LogInstigator, const FString& LogMessage)
{
    // 입력 값에 대한 검증이 필요 없다면 통과
    return true;
}

void AScenarioPlayerControllerBase::Server_RequestLog_Implementation(const FString& LogInstigator, const FString& LogMessage)
{
    FString FinalInstigator = LogInstigator;

    // 블루프린트에서 Instigator를 비워두고 호출한 경우 본인의 동기화 이름(UserName)으로 예외 보정
    if (FinalInstigator.IsEmpty())
    {
        FinalInstigator = TEXT("Unknown_Player");
        if (AScenarioPlayerStateBase* ScenarioPS = GetPlayerState<AScenarioPlayerStateBase>())
        {
            if (!ScenarioPS->UserName.IsEmpty()) FinalInstigator = ScenarioPS->UserName;
        }
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UScenarioLogSubsystem* LoggingSubsystem = GI->GetSubsystem<UScenarioLogSubsystem>())
        {
            // 서버 측 장부에 가공 및 저장 후 모니터 UI 복제 배열에 원스톱 탑재
            FString Formatted = LoggingSubsystem->AddLog(FinalInstigator, LogMessage);
            if (AScenarioGameStateBase* GS = GetWorld()->GetGameState<AScenarioGameStateBase>())
            {
                GS->AddDisplayLog(Formatted);
            }
        }
    }
}

void AScenarioPlayerControllerBase::Server_RequestChangeName_Implementation(int32 TargetUserIndex, const FString& NewName)
{
    // Server RPC 내부이므로 이 안의 로직은 무조건 서버 컴퓨터에서 실행됩니다.
    AGameStateBase* GS = GetWorld()->GetGameState();
    if (!IsValid(GS)) return;

    // GameState가 상시 관리하는 PlayerArray를 순회하며 타겟 유저를 찾습니다.
    for (APlayerState* PS : GS->PlayerArray)
    {
        AScenarioPlayerStateBase* ScenarioPS = Cast<AScenarioPlayerStateBase>(PS);
        if (IsValid(ScenarioPS) && ScenarioPS->UserIndex == TargetUserIndex)
        {
            // 타겟을 찾았으므로 서버 권한으로 이름을 변경합니다.
            ScenarioPS->SetUserName(NewName);
            break;
        }
    }
}

