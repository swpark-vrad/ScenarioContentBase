// Fill out your copyright notice in the Description page of Project Settings.


#include "DataTransfer/ByteTransferComponent.h"
#include "DataTransfer/DataTransferGameMode.h"
#include "DataTransfer/DataTransferGameState.h"
#include "Global/ScenarioGameInstanceBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UByteTransferComponent::UByteTransferComponent()
{
    SetIsReplicatedByDefault(true);
    MissingImageIndex = -1;
    TotalFilesCount = 0;
    CurrentCompletedFileCount = 0;
}

void UByteTransferComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 데이터 전달 도중 끊긴 버퍼들을 명시적으로 청소하여 메모리 누수 원천 차단
    ActiveServerTransfers.Empty();
    ReceivedBuffers.Empty();
    MissingImageKeys.Empty();

    Super::EndPlay(EndPlayReason);
}

void UByteTransferComponent::NotifyReadyToServer()
{
    if (GetNetMode() == NM_Client || GetOwnerRole() < ROLE_Authority)
    {
        Server_ReadyToSync();
    }
}

void UByteTransferComponent::Server_ReadyToSync_Implementation()
{
    ADataTransferGameMode* MyGM = Cast<ADataTransferGameMode>(GetWorld()->GetAuthGameMode());
    if (MyGM)
    {
        MyGM->RegisterClientComponent(this);
    }
}

FString UByteTransferComponent::GetClientCacheFilePath(const FName& ScenarioID, FName ImageKey) const
{
    return FPaths::ProjectSavedDir() / TEXT("CustomScenarios") / ScenarioID.ToString() / (ImageKey.ToString() + TEXT(".data"));
}

void UByteTransferComponent::Client_InitializeSync_Implementation(const FName& ScenarioID, const TArray<FResultImageInfo>& ImageInfos)
{
    CurrentActiveScenarioID = ScenarioID;
    MissingImageKeys.Empty();
    MissingImageIndex = -1;

    for (const FResultImageInfo& Info : ImageInfos)
    {
        FName Key = FName(*Info.GetUniqueKey());
        FString LocalPath = GetClientCacheFilePath(ScenarioID, Key);
        TArray<uint8> DummyBuffer;

        if (FFileHelper::LoadFileToArray(DummyBuffer, *LocalPath))
        {
            UE_LOG(LogTemp, Log, TEXT("Cache Hit: [%s] 이미지가 로컬 캐시에 존재하므로 다운로드를 패스합니다."), *Key.ToString());
            OnTransferComplete.Broadcast(Info.Category, Info.TestName, Info.DetailItem, DummyBuffer);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Cache Miss: [%s] 이미지가 로컬에 없어 전송 큐에 등록합니다."), *Key.ToString());
            MissingImageKeys.Add(Key);
        }
    }

    // 전송 상태 측정을 위한 카운트 변수 초기화
    TotalFilesCount = MissingImageKeys.Num();
    CurrentCompletedFileCount = 0;

    if (MissingImageKeys.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Client: 모든 이미지가 이미 캐싱되어 있습니다. 동기화 즉시 통과."));

        UScenarioGameInstanceBase* MyGI = Cast<UScenarioGameInstanceBase>(GetWorld()->GetGameInstance());
        if (MyGI)
        {
            MyGI->LoadScenarioToMemory();
        }

        // [추가] 받아야 할 파일이 없으므로 즉시 100퍼센트 도달 상태를 전파합니다
        Server_ReportProgress(1, 1);

        OnSyncComplete.Broadcast();
        Server_NotifyClientSyncComplete();
    }
    else
    {
        // [추가] 다운로드 루프 진입 직전 GameState 테이블에 0퍼센트 진행도로 초기 등록합니다
        Server_ReportProgress(0, TotalFilesCount);

        MissingImageIndex = 0;
        ProcessNextMissingImage();
    }
}

void UByteTransferComponent::ProcessNextMissingImage()
{
    if (MissingImageKeys.IsValidIndex(MissingImageIndex))
    {
        FName NextKey = MissingImageKeys[MissingImageIndex];
        Server_RequestImageChunks(NextKey);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Client: 지정된 시나리오의 모든 분할 전송 다운로드 및 캐시 갱신이 완료되었습니다."));

        UScenarioGameInstanceBase* MyGI = Cast<UScenarioGameInstanceBase>(GetWorld()->GetGameInstance());
        if (MyGI)
        {
            MyGI->LoadScenarioToMemory();
        }

        OnSyncComplete.Broadcast();
        Server_NotifyClientSyncComplete();
    }
}

void UByteTransferComponent::Server_RequestImageChunks_Implementation(FName ImageKey)
{
    if (ActiveServerTransfers.Contains(ImageKey)) return;

    UScenarioGameInstanceBase* MyGI = Cast<UScenarioGameInstanceBase>(GetWorld()->GetGameInstance());
    if (!MyGI) return;

    TArray<uint8> ServerBytes;
    if (MyGI->GetActiveImageData(ImageKey, ServerBytes))
    {
        FServerChunkState& State = ActiveServerTransfers.Add(ImageKey);
        State.Bytes = ServerBytes;
        State.CurrentChunk = 0;

        State.TotalChunks = FMath::CeilToInt((float)ServerBytes.Num() / 32768);

        SendChunkInternal(ImageKey);
    }
}

void UByteTransferComponent::SendChunkInternal(FName ImageKey)
{
    FServerChunkState* State = ActiveServerTransfers.Find(ImageKey);
    if (!State) return;

    int32 ChunkSize = 32768;
    int32 TotalSize = State->Bytes.Num();
    int32 i = State->CurrentChunk;

    int32 StartIndex = i * ChunkSize;
    int32 EndIndex = FMath::Min(StartIndex + ChunkSize, TotalSize);
    int32 CurrentChunkSize = EndIndex - StartIndex;

    TArray<uint8> ChunkData;
    ChunkData.SetNumUninitialized(CurrentChunkSize);
    FMemory::Memcpy(ChunkData.GetData(), State->Bytes.GetData() + StartIndex, CurrentChunkSize);

    Client_ReceiveChunk(ImageKey, ChunkData, i, State->TotalChunks);
}

void UByteTransferComponent::Server_AckAndRequestNextChunk_Implementation(FName ImageKey, int32 AcknowledgedIndex)
{
    FServerChunkState* State = ActiveServerTransfers.Find(ImageKey);
    if (!State) return;

    if (State->CurrentChunk == AcknowledgedIndex)
    {
        State->CurrentChunk++;

        if (State->CurrentChunk < State->TotalChunks)
        {
            SendChunkInternal(ImageKey);
        }
        else
        {
            ActiveServerTransfers.Remove(ImageKey);
        }
    }
}

void UByteTransferComponent::Client_ReceiveChunk_Implementation(FName ImageKey, const TArray<uint8>& ChunkData, int32 CurrentIndex, int32 TotalCount)
{
    FByteDataBuffer& BufferStruct = ReceivedBuffers.FindOrAdd(ImageKey);
    BufferStruct.Bytes.Append(ChunkData);

    if (CurrentIndex == TotalCount - 1)
    {
        FString SavePath = GetClientCacheFilePath(CurrentActiveScenarioID, ImageKey);
        if (FFileHelper::SaveArrayToFile(BufferStruct.Bytes, *SavePath))
        {
            UE_LOG(LogTemp, Log, TEXT("Cache Written: 전송받은 [%s] 데이터를 로컬에 영구 캐싱했습니다."), *ImageKey.ToString());

            // [추가] 파일 작성이 완수되었으므로 카운트를 누적하고 서버 GameState 진행도를 최신화합니다
            CurrentCompletedFileCount++;
            Server_ReportProgress(CurrentCompletedFileCount, TotalFilesCount);
        }

        FString CleanFileName = ImageKey.ToString();
        FString CategoryStr;
        FString RestStr;
        if (CleanFileName.Split(TEXT("_"), &CategoryStr, &RestStr))
        {
            FString TestNameStr;
            FString DetailItemStr; // 변수명을 명확하게 세부항목 문자열로 변경합니다

            // 맨 우측 마지막 언더바(_) 분리 규칙을 활용하여 숫자가 아닌 세부항목 이름을 문자열로 추출합니다
            if (RestStr.Split(TEXT("_"), &TestNameStr, &DetailItemStr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
            {
                FName Category = FName(*CategoryStr);
                FName TestName = FName(*TestNameStr);
                FName DetailItem = FName(*DetailItemStr); // 갱신된 규칙에 맞추어 FName 구조체로 원형 그대로 캐스팅합니다

                // 수정된 대리자 규칙에 부합하도록 FName 인자 조합을 브로드캐스트합니다
                OnTransferComplete.Broadcast(Category, TestName, DetailItem, BufferStruct.Bytes);
            }
        }

        ReceivedBuffers.Remove(ImageKey);

        Server_AckAndRequestNextChunk(ImageKey, CurrentIndex);

        MissingImageIndex++;
        ProcessNextMissingImage();
    }
    else
    {
        Server_AckAndRequestNextChunk(ImageKey, CurrentIndex);
    }
}

void UByteTransferComponent::Server_ReportProgress_Implementation(int32 Current, int32 Total)
{
    ADataTransferGameState* MyGS = GetWorld()->GetGameState<ADataTransferGameState>();
    ADataTransferGameMode* MyGM = Cast<ADataTransferGameMode>(GetWorld()->GetAuthGameMode());

    // 게임스테이트와 게임모드가 모두 온전한 권한을 가질 때만 실행합니다
    if (MyGS && MyGM)
    {
        // 내가 서버 대기열의 몇 번째 인덱스에 등록된 객체인지 번호를 획득합니다
        int32 FoundIndex = MyGM->GetClientIndex(this);

        if (FoundIndex != INDEX_NONE)
        {
            // 획득한 번호로 User0, User1 문자열을 생성하여 던집니다
            FString CustomUserName = FString::Printf(TEXT("User%d"), FoundIndex);
            MyGS->UpdatePlayerProgress(CustomUserName, Current, Total);
        }
    }
}

void UByteTransferComponent::Server_NotifyClientSyncComplete_Implementation()
{
    ADataTransferGameMode* MyGM = Cast<ADataTransferGameMode>(GetWorld()->GetAuthGameMode());
    if (MyGM)
    {
        MyGM->NotifyClientSyncFinished(this);
    }
}

float UByteTransferComponent::GetLocalProgressPercentage() const
{
    if (TotalFilesCount > 0)
    {
        // 프로그레스 바의 퍼센트 핀에 바로 꽂을 수 있도록 0.0에서 1.0 사이의 실수를 반환합니다
        return (float)CurrentCompletedFileCount / (float)TotalFilesCount;
    }

    // 받아야 할 파일이 0개라면 이미 로딩이 완료된 상태이므로 100퍼센트를 반환합니다
    return 1.0f;
}