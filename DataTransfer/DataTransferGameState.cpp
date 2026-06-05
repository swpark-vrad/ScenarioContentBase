#include "DataTransfer/DataTransferGameState.h"
#include "Net/UnrealNetwork.h"

ADataTransferGameState::ADataTransferGameState()
{
    // 게임 시작 시 초기값은 당연히 false로 채워둡니다
    bIsAllClientsSynced = false;
}

void ADataTransferGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 진행도 배열 데이터가 네트워크를 타고 모든 클라이언트 위젯에 복제되도록 설정합니다
    DOREPLIFETIME(ADataTransferGameState, ClientProgressArray);
    // bool 상태 값도 모든 클라이언트 기기들로 실시간 복제되도록 등록합니다
    DOREPLIFETIME(ADataTransferGameState, bIsAllClientsSynced);
}

void ADataTransferGameState::InitializePlayerProgress(const FString& PlayerName)
{
    // 이미 등록된 플레이어인지 검사합니다
    for (const FClientProgressInfo& Info : ClientProgressArray)
    {
        if (Info.PlayerName == PlayerName) return;
    }

    // 신규 진입 유저를 대기(Waiting) 상태로 목록에 즉시 올립니다
    FClientProgressInfo NewInfo;
    NewInfo.PlayerName = PlayerName;
    NewInfo.SyncState = EClientSyncState::Waiting;
    ClientProgressArray.Add(NewInfo);
}

void ADataTransferGameState::UpdatePlayerState(const FString& PlayerName, EClientSyncState NewState)
{
    for (FClientProgressInfo& Info : ClientProgressArray)
    {
        if (Info.PlayerName == PlayerName)
        {
            Info.SyncState = NewState;
            break;
        }
    }
}

void ADataTransferGameState::UpdatePlayerProgress(const FString& PlayerName, int32 Current, int32 Total)
{
    for (FClientProgressInfo& Info : ClientProgressArray)
    {
        if (Info.PlayerName == PlayerName)
        {
            Info.CurrentCount = Current;
            Info.TotalCount = Total;

            if (Total > 0)
            {
                Info.Percentage = ((float)Current / (float)Total) * 100.0f;
            }
            else
            {
                Info.Percentage = 100.0f;
            }

            // 다운로드가 100퍼센트에 도달했다면 상태를 완료로 자동 전환합니다
            if (Info.Percentage >= 100.0f)
            {
                Info.SyncState = EClientSyncState::Completed;
            }
            break;
        }
    }
}

int32 ADataTransferGameState::GetTotalConnectionCount() const
{
    return ClientProgressArray.Num();
}

int32 ADataTransferGameState::GetFullySyncedConnectionCount() const
{
    int32 CompletedCount = 0;
    for (const FClientProgressInfo& Info : ClientProgressArray)
    {
        if (Info.Percentage >= 100.0f)
        {
            CompletedCount++;
        }
    }
    return CompletedCount;
}