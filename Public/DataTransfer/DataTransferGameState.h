#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DataTransferGameState.generated.h"

UENUM(BlueprintType)
enum class EClientSyncState : uint8
{
    Waiting     UMETA(DisplayName = "Waiting"),   // 대기 중
    Syncing     UMETA(DisplayName = "Syncing"),   // 진행 중
    Completed   UMETA(DisplayName = "Completed")  // 완료됨
};

// 개별 플레이어의 진행 상태를 담을 구조체입니다
USTRUCT(BlueprintType)
struct FClientProgressInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Progress")
    FString PlayerName = TEXT("");

    UPROPERTY(BlueprintReadOnly, Category = "Progress")
    int32 CurrentCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Progress")
    int32 TotalCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Progress")
    float Percentage = 0.0f;

    // 위젯이 참조할 핵심 상태 변수를 추가합니다
    UPROPERTY(BlueprintReadOnly, Category = "Progress")
    EClientSyncState SyncState = EClientSyncState::Waiting;
};

UCLASS()
class SCENARIOCONTENT_API ADataTransferGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ADataTransferGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 모든 클라이언트가 가치 공유하여 위젯에서 읽어갈 핵심 진행도 배열 변수입니다
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Progress")
    TArray<FClientProgressInfo> ClientProgressArray;

    // 모든 접속자의 동기화가 완벽히 끝났을 때만 true가 되는 네트워크 복제 변수입니다
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Progress")
    bool bIsAllClientsSynced;

    // 클라이언트가 서버에 최초 등록될 때 호출할 초기화 함수입니다
    void InitializePlayerProgress(const FString& PlayerName);

    // 클라이언트의 상태 열거형만 강제로 변경할 때 사용할 함수입니다
    void UpdatePlayerState(const FString& PlayerName, EClientSyncState NewState);

    // 컴포넌트가 다운로드 숫자를 보고할 때 호출할 갱신 함수입니다
    void UpdatePlayerProgress(const FString& PlayerName, int32 Current, int32 Total);

    // 위젯에서 현재 총 접속자 명수를 반환받을 블루프린트 노드 함수입니다
    UFUNCTION(BlueprintPure, Category = "Progress")
    int32 GetTotalConnectionCount() const;

    // 위젯에서 다운로드가 100퍼센트 끝난 누적 인원수를 반환받을 블루프린트 노드 함수입니다
    UFUNCTION(BlueprintPure, Category = "Progress")
    int32 GetFullySyncedConnectionCount() const;
};