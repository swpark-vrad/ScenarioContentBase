#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DataTransferPlayerController.generated.h"

class UByteTransferComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTransitionStarted);

UCLASS()
class SCENARIOCONTENT_API ADataTransferPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ADataTransferPlayerController();

protected:
    virtual void BeginPlay() override;

public:
    // 대용량 데이터 전송 및 조립을 전담할 컴포넌트입니다
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UByteTransferComponent* TransferComponent;

    // 클라이언트가 맵 로딩을 마치고 준비되었음을 서버에 보고하는 서버 RPC 함수입니다
    UFUNCTION(Server, Reliable)
    void Server_NotifyReadyForSync();

    // 서버 트래블 직전 신호를 받기 위한 클라이언트 RPC 함수입니다
    UFUNCTION(Client, Reliable)
    void Client_NotifyTransitionStarted();

    // 블루프린트 위젯에서 이벤트를 바인딩할 델리게이트 변수입니다
    UPROPERTY(BlueprintAssignable, Category = "DataTransfer|Events")
    FOnTransitionStarted OnTransitionStarted;
};