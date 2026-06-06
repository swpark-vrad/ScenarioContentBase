// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/TestResultData.h"
#include "ByteTransferComponent.generated.h"

USTRUCT()
struct FServerChunkState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<uint8> Bytes;

    int32 CurrentChunk = 0;
    int32 TotalChunks = 0;
};

// 4개의 파라미터를 지원하도록 멀티캐스트 델리게이트 매크로를 변경합니다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnByteTransferComplete, FName, Category, FName, TestName, FName, DetailItem, const TArray<uint8>&, Bytes);

// 모든 시나리오 이미지의 동기화가 완전히 끝났음을 알리는 델리게이트입니다
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnByteTransferSyncComplete);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SCENARIOCONTENT_API UByteTransferComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UByteTransferComponent();

    UPROPERTY(BlueprintAssignable, Category = "Networking|Transfer")
    FOnByteTransferComplete OnTransferComplete;

    // 블루프린트 위젯이나 컨트롤러에서 구독할 전체 완료 델리게이트입니다
    UPROPERTY(BlueprintAssignable, Category = "Networking|Transfer")
    FOnByteTransferSyncComplete OnSyncComplete;

    UFUNCTION(BlueprintCallable, Category = "Networking|Transfer")
    void NotifyReadyToServer();

    UFUNCTION(Client, Reliable)
    void Client_InitializeSync(const FName& ScenarioID, const TArray<FResultImageInfo>& ImageInfos);

    UFUNCTION(Server, Reliable)
    void Server_RequestImageChunks(FName ImageKey);

    UFUNCTION(Server, Reliable)
    void Server_AckAndRequestNextChunk(FName ImageKey, int32 AcknowledgedIndex);

    UFUNCTION(Server, Reliable)
    void Server_ReportProgress(int32 Current, int32 Total);

    // 블루프린트 위젯에서 언제든 소수점 퍼센티지를 바로 가져갈 수 있도록 함수를 노출합니다
    UFUNCTION(BlueprintPure, Category = "Networking|Transfer")
    float GetLocalProgressPercentage() const;

protected:
    // 수명주기 종료 시 메모리 강제 해제를 위한 오버라이드 추가
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION(Server, Reliable)
    void Server_ReadyToSync();

    void SendChunkInternal(FName ImageKey);

    UFUNCTION(Client, Reliable)
    void Client_ReceiveChunk(FName ImageKey, const TArray<uint8>& ChunkData, int32 CurrentIndex, int32 TotalCount);

    UFUNCTION(Server, Reliable)
    void Server_NotifyClientSyncComplete();

    void ProcessNextMissingImage();

    FString GetClientCacheFilePath(const FName& ScenarioID, FName ImageKey) const;

    FName CurrentActiveScenarioID;

    UPROPERTY()
    TArray<FName> MissingImageKeys;
    int32 MissingImageIndex;

    UPROPERTY()
    TMap<FName, FServerChunkState> ActiveServerTransfers;

    UPROPERTY()
    TMap<FName, FByteDataBuffer> ReceivedBuffers;

    // 전체 다운로드해야 할 파일 개수입니다
    int32 TotalFilesCount;

    // 현재 다운로드가 완료된 누적 파일 개수입니다
    int32 CurrentCompletedFileCount;
};