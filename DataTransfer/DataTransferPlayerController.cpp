#include "DataTransfer/DataTransferPlayerController.h"
#include "DataTransfer/DataTransferGameMode.h"
#include "DataTransfer/ByteTransferComponent.h"

ADataTransferPlayerController::ADataTransferPlayerController()
{
    // 오직 데이터 전송 레벨에서만 가동될 전송 컴포넌트를 독점적으로 생성 및 부착합니다
    TransferComponent = CreateDefaultSubobject<UByteTransferComponent>(TEXT("TransferComponent"));
}

void ADataTransferPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 클라이언트일 때에만 데이터 요청
    if (!HasAuthority())
    {
        Server_NotifyReadyForSync();
    }
}

void ADataTransferPlayerController::Server_NotifyReadyForSync_Implementation()
{
    // 서버 환경에서 전송용 전용 게임모드를 검색합니다
    ADataTransferGameMode* MyGM = Cast<ADataTransferGameMode>(GetWorld()->GetAuthGameMode());
    if (MyGM && TransferComponent)
    {
        // 검증이 완료되면 현재 접속한 클라이언트의 컴포넌트 포인터를 게임모드 대기열에 등록합니다
        MyGM->RegisterClientComponent(TransferComponent);
    }
}

void ADataTransferPlayerController::Client_NotifyTransitionStarted_Implementation()
{
    // 각 클라이언트 기기에서 이벤트가 방송됩니다
    if (OnTransitionStarted.IsBound())
    {
        OnTransitionStarted.Broadcast();
    }
}