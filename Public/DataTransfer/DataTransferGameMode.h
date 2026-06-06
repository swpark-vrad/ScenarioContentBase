#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DataTransferGameMode.generated.h"

class UByteTransferComponent;

// 레벨 블루프린트 등 외부에서 호출을 인지할 수 있도록 멀티캐스트 델리게이트를 선언합니다
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllClientsSyncComplete);

UCLASS()
class SCENARIOCONTENT_API ADataTransferGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADataTransferGameMode();

    // 플레이어 컨트롤러로부터 요청을 받아 대기열 배열에 등록하는 함수입니다
    void RegisterClientComponent(UByteTransferComponent* ClientComp);

    // 개별 클라이언트가 다운로드를 완수했음을 보고할 때 호출되는 함수입니다
    void NotifyClientSyncFinished(UByteTransferComponent* ClientComp);

    // 모든 인원의 다운로드가 끝나면 발동할 블루프린트 연동용 이벤트를 노출합니다
    UPROPERTY(BlueprintAssignable, Category = "DataTransfer|Network")
    FOnAllClientsSyncComplete OnAllClientsSyncComplete;

    // 에디터 디테일 패널에서 지정할 호스트 PC용 폰 클래스입니다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classes")
    TSubclassOf<APawn> PCPawnClass;

    // 에디터 디테일 패널에서 지정할 클라이언트 VR용 폰 클래스입니다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classes")
    TSubclassOf<APawn> VRPawnClass;

    // 호스트 위젯의 버튼을 눌렀을 때 호출할 블루프린트 개방형 함수입니다
    UFUNCTION(BlueprintCallable, Category = "DataTransfer|Network")
    void StartScenarioTravel(const FString& MapURL);

    // 컴포넌트가 대기열의 몇 번째 칸에 가동 중인지 인덱스를 반환하는 함수입니다
    int32 GetClientIndex(UByteTransferComponent* ClientComp) const;

private:
    // 배열 인덱스를 기반으로 다음 순서의 클라이언트 동기화를 순차 가동합니다
    void ExecuteNextSync();

    UPROPERTY()
    TArray<UByteTransferComponent*> SyncQueue;

    int32 CurrentSyncIndex;
    bool bIsSyncActive;

protected:
    // 엔진의 기본 폰 결정 로직을 가로채기 위한 핵심 오버라이드 함수 선언입니다
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};