#include "DataTransfer/DataTransferGameMode.h"
#include "DataTransfer/ByteTransferComponent.h"
#include "DataTransfer/DataTransferGameState.h"
#include "DataTransfer/DataTransferPlayerController.h"
#include "Global/ScenarioGameInstanceBase.h"

ADataTransferGameMode::ADataTransferGameMode()
{
    CurrentSyncIndex = 0;
    bIsSyncActive = false;

    // 서버 트래블 시 접속된 유저들을 탈락시키지 않고 함께 이동시키기 위해 필수 활성화합니다
    bUseSeamlessTravel = true;

    PlayerControllerClass = ADataTransferPlayerController::StaticClass();
    GameStateClass = ADataTransferGameState::StaticClass();
}

void ADataTransferGameMode::RegisterClientComponent(UByteTransferComponent* ClientComp)
{
    if (!ClientComp) return;

    int32 AssignedIndex = SyncQueue.Add(ClientComp);

    ADataTransferGameState* MyGS = GetGameState<ADataTransferGameState>();
    if (MyGS)
    {
        MyGS->bIsAllClientsSynced = false;

        // 대기열에 추가된 현재 순번을 기반으로 User0, User1 형태의 이름을 강제 부여합니다
        
        FString CustomUserName = FString::Printf(TEXT("User%d"), AssignedIndex);

        // 위젯 목록 테이블에 등록합니다
        MyGS->InitializePlayerProgress(CustomUserName);
    }

    if (!bIsSyncActive)
    {
        CurrentSyncIndex = AssignedIndex;
        ExecuteNextSync();
    }
}

void ADataTransferGameMode::StartScenarioTravel(const FString& MapURL)
{
    // 1. 현재 게임 월드에 접속해 있는 모든 플레이어 컨트롤러를 순회합니다
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ADataTransferPlayerController* PC = Cast<ADataTransferPlayerController>(It->Get());
        if (PC)
        {
            // 각 클라이언트 기기에게 화면에 띄울 문구를 원격 전송합니다
            PC->Client_NotifyTransitionStarted();
        }
    }

    // 2. 클라이언트들에게 패킷이 완전히 송신될 수 있도록 문구 전송 바로 다음 줄에서 트래블을 트리거합니다
    GetWorld()->ServerTravel(MapURL);
}

int32 ADataTransferGameMode::GetClientIndex(UByteTransferComponent* ClientComp) const
{
    return SyncQueue.IndexOfByKey(ClientComp);
}

void ADataTransferGameMode::ExecuteNextSync()
{
    ADataTransferGameState* MyGS = GetGameState<ADataTransferGameState>();

    if (SyncQueue.IsValidIndex(CurrentSyncIndex))
    {
        bIsSyncActive = true;

        if (MyGS)
        {
            // 현재 전송 중인 대기열 인덱스 번호로 UserX 이름을 빌드하여 진행 중 상태로 바꿉니다
            FString CustomUserName = FString::Printf(TEXT("User%d"), CurrentSyncIndex);
            MyGS->UpdatePlayerState(CustomUserName, EClientSyncState::Syncing);
        }

        UScenarioGameInstanceBase* MyGI = Cast<UScenarioGameInstanceBase>(GetGameInstance());
        if (MyGI)
        {
            SyncQueue[CurrentSyncIndex]->Client_InitializeSync(MyGI->GetCurrentScenarioID(), MyGI->GetActiveScenarioImageInfos());
        }
    }
    else
    {
        bIsSyncActive = false;

        if (MyGS)
        {
            MyGS->bIsAllClientsSynced = true;
        }

        if (OnAllClientsSyncComplete.IsBound())
        {
            OnAllClientsSyncComplete.Broadcast();
        }
    }
}

UClass* ADataTransferGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    APlayerController* PC = Cast<APlayerController>(InController);
    if (PC)
    {
        // GetLocalPlayer()가 유효하다면 이 게임 프로세스(호스트 서버)에 물리적으로 직접 연결된 로컬 마스터 유저입니다
        if (PC->GetLocalPlayer() != nullptr)
        {
            return PCPawnClass ? *PCPawnClass : Super::GetDefaultPawnClassForController_Implementation(InController);
        }
        else
        {
            // GetLocalPlayer()가 nullptr라면 네트워크 소켓 소통을 통해 외부에서 접속한 원격 클라이언트(퀘스트 유저)입니다
            return VRPawnClass ? *VRPawnClass : Super::GetDefaultPawnClassForController_Implementation(InController);
        }
    }

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ADataTransferGameMode::NotifyClientSyncFinished(UByteTransferComponent* ClientComp)
{
    ADataTransferGameState* MyGS = GetGameState<ADataTransferGameState>();
    if (MyGS && ClientComp)
    {
        // 종료를 보고한 컴포넌트의 고유 인덱스를 역추적하여 해당 유저를 완료 처리합니다
        int32 FoundIndex = GetClientIndex(ClientComp);
        if (FoundIndex != INDEX_NONE)
        {
            FString CustomUserName = FString::Printf(TEXT("User%d"), FoundIndex);
            MyGS->UpdatePlayerState(CustomUserName, EClientSyncState::Completed);
        }
    }

    bIsSyncActive = false;
    CurrentSyncIndex++;
    ExecuteNextSync();
}