// Fill out your copyright notice in the Description page of Project Settings.

#include "Global/ScenarioGameInstanceBase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Data/TestResultData.h"
#include "Data/ScenarioSaveGame.h" // 세이브게임 클래스 헤더 인클루드
#include "Kismet/GameplayStatics.h" // 세이브 파일 인출용 스태틱 함수 헤더
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#endif

void UScenarioGameInstanceBase::DumpActiveScenarioImages()
{
    UE_LOG(LogTemp, Log, TEXT("=== 현재 적재된 시나리오 이미지 맵 디버깅 시작 ==="));
    UE_LOG(LogTemp, Log, TEXT("현재 선택된 시나리오 ID: %s"), *CurrentScenarioID.ToString());
    UE_LOG(LogTemp, Log, TEXT("총 적재된 이미지 파일 개수: %d"), ActiveScenarioImageMap.Num());

    for (const TPair<FName, FByteDataBuffer>& Pair : ActiveScenarioImageMap)
    {
        FName ImageKey = Pair.Key;
        const FByteDataBuffer& Buffer = Pair.Value;
        UE_LOG(LogTemp, Log, TEXT("이미지 키: %s, 데이터 크기: %d 바이트"), *ImageKey.ToString(), Buffer.Bytes.Num());
    }
    UE_LOG(LogTemp, Log, TEXT("=== 현재 적재된 시나리오 이미지 맵 디버깅 종료 ==="));
}

void UScenarioGameInstanceBase::ShutdownSessionAsHost()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem)
    {
        IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UScenarioGameInstanceBase::OnDestroySessionComplete);
            SessionInterface->DestroySession(NAME_GameSession);
            return;
        }
    }
    MoveToHostLobby();
}

void UScenarioGameInstanceBase::LeaveSessionAsClient(APlayerController* RequestingPC)
{
    APlayerController* TargetPC = IsValid(RequestingPC) ? RequestingPC : GetFirstLocalPlayerController(GetWorld());
    if (TargetPC)
    {
        if (!ClientLobbyLevel.IsNull())
        {
            FString MapPath = ClientLobbyLevel.ToSoftObjectPath().GetLongPackageName();
            TargetPC->ClientTravel(MapPath, ETravelType::TRAVEL_Absolute);
        }
        else
        {
            UE_LOG(LogNet, Error, TEXT("LeaveSessionAsClient: ClientLobbyLevel 에셋이 지정되지 않았습니다."));
        }
    }
}

void UScenarioGameInstanceBase::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
    if (Subsystem)
    {
        IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
        if (SessionInterface.IsValid())
        {
            SessionInterface->OnDestroySessionCompleteDelegates.RemoveAll(this);
        }
    }
    MoveToHostLobby();
}

void UScenarioGameInstanceBase::MoveToHostLobby()
{
    APlayerController* PC = GetFirstLocalPlayerController();
    if (PC && !HostLobbyLevel.IsNull())
    {
        FString MapPath = HostLobbyLevel.ToSoftObjectPath().GetLongPackageName();
        PC->ClientTravel(MapPath, ETravelType::TRAVEL_Absolute);
    }
}

bool UScenarioGameInstanceBase::IsBuiltInScenario(const FName& ScenarioID) const
{
    FString IDStr = ScenarioID.ToString();
    if (IDStr.Len() == 5)
    {
        if (FChar::IsAlpha(IDStr[0]))
        {
            for (int32 i = 1; i < IDStr.Len(); ++i)
            {
                if (!FChar::IsDigit(IDStr[i])) return false;
            }
            return true;
        }
    }
    return false;
}

UScenarioSaveGame* UScenarioGameInstanceBase::LoadScenarioData(FName ScenarioID)
{
    if (ScenarioID.IsNone()) return nullptr;

    // 자체 식별 패턴 함수를 통해 빌트인 에셋 경로와 커스텀 경로를 자동 판별합니다
    if (IsBuiltInScenario(ScenarioID))
    {
        FString FullPath = FPaths::ProjectContentDir() / TEXT("AdditionalAsset") / (ScenarioID.ToString() + TEXT(".sav"));
        TArray<uint8> RawData;

        // Content 폴더 내의 고정 시나리오 파일을 바이너리로 로드 후 역직렬화 진행
        if (FFileHelper::LoadFileToArray(RawData, *FullPath))
        {
            return Cast<UScenarioSaveGame>(UGameplayStatics::LoadGameFromMemory(RawData));
        }
        return nullptr;
    }

    // 커스텀 시나리오 파일은 플랫폼 규격 세이브 보관소(Saved/SaveGames)에서 인출
    return Cast<UScenarioSaveGame>(UGameplayStatics::LoadGameFromSlot(ScenarioID.ToString(), 0));
}

TArray<FName> UScenarioGameInstanceBase::GetAvailableBloodCategories() const
{
    TArray<FName> OutCategories;
    ActiveBloodTestMap.GetKeys(OutCategories);
    return OutCategories;
}

TArray<FBloodTestRow> UScenarioGameInstanceBase::GetBloodTestRowsByCategory(FName Category) const
{
    if (const FBloodTestCategory* Found = ActiveBloodTestMap.Find(Category))
    {
        return Found->Rows;
    }
    return TArray<FBloodTestRow>();
}

FString UScenarioGameInstanceBase::GetScenarioDirectoryPath(const FName& ScenarioID) const
{
    return FPaths::ProjectSavedDir() / TEXT("CustomScenarios") / ScenarioID.ToString();
}

FString UScenarioGameInstanceBase::GetBuiltInDirectoryPath(const FName& ScenarioID) const
{
    return FPaths::ProjectContentDir() / TEXT("AdditionalAsset") / ScenarioID.ToString();
}

bool UScenarioGameInstanceBase::ParseBloodTestJson(const FString& JsonString)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    TArray<TSharedPtr<FJsonValue>> JsonArray;

    if (FJsonSerializer::Deserialize(Reader, JsonArray))
    {
        for (const auto& Value : JsonArray)
        {
            TSharedPtr<FJsonObject> CategoryObject = Value->AsObject();
            if (!CategoryObject.IsValid()) continue;

            FBloodTestCategory NewCategory;
            FString CategoryNameStr;
            if (CategoryObject->TryGetStringField(TEXT("Category"), CategoryNameStr))
            {
                NewCategory.Category = FName(*CategoryNameStr);
            }

            const TArray<TSharedPtr<FJsonValue>>* RowsArray;
            if (CategoryObject->TryGetArrayField(TEXT("Rows"), RowsArray))
            {
                for (const auto& RowValue : *RowsArray)
                {
                    TSharedPtr<FJsonObject> RowObject = RowValue->AsObject();
                    if (!RowObject.IsValid()) continue;

                    FBloodTestRow NewRow;
                    if (FJsonObjectConverter::JsonObjectToUStruct(RowObject.ToSharedRef(), FBloodTestRow::StaticStruct(), &NewRow))
                    {
                        NewRow.UpdateNormalStatus();
                        NewCategory.Rows.Add(NewRow);
                    }
                }
            }

            if (!NewCategory.Category.IsNone())
            {
                ActiveBloodTestMap.Add(NewCategory.Category, NewCategory);
            }
        }
        return true;
    }
    return false;
}

bool UScenarioGameInstanceBase::OpenFileDialogAndLoadImages(FName Category, FName TestName, TArray<FResultImage>& OutLoadedImages)
{
    OutLoadedImages.Empty();

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) return false;

    const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
    FString DialogTitle = TEXT("시나리오 이미지 파일 다중 선택");
    FString DefaultPath = FPaths::ProjectDir();
    FString DefaultFile = TEXT("");
    FString FileTypes = TEXT("Image Files|*.png;*.jpg;*.jpeg|All Files|*.*");
    uint32 Flags = EFileDialogFlags::Multiple;
    TArray<FString> OutFileNames;

    bool bOpened = DesktopPlatform->OpenFileDialog(ParentWindowWindowHandle, DialogTitle, DefaultPath, DefaultFile, FileTypes, Flags, OutFileNames);

    if (bOpened && OutFileNames.Num() > 0)
    {
        for (const FString& FilePath : OutFileNames)
        {
            TArray<uint8> FileBytes;
            if (FFileHelper::LoadFileToArray(FileBytes, *FilePath))
            {
                FString OriginalFileName = FPaths::GetBaseFilename(FilePath);
                FResultImage NewImage;
                NewImage.Category = Category;
                NewImage.TestName = TestName;
                NewImage.DetailItem = FName(*OriginalFileName);
                NewImage.Bytes = FileBytes;
                OutLoadedImages.Add(NewImage);
            }
        }
        return true;
    }
    return false;
#else
    return false;
#endif
}

TArray<FName> UScenarioGameInstanceBase::GetImageKeysForTest(FName Category, FName TestName) const
{
    TArray<FName> MatchedKeys;
    for (const FResultImageInfo& Info : ActiveImageInfos)
    {
        if (Info.Category == Category && Info.TestName == TestName)
        {
            MatchedKeys.Add(FName(*Info.GetUniqueKey()));
        }
    }
    MatchedKeys.Sort([](const FName& A, const FName& B) {
        return A.ToString() < B.ToString();
        });
    return MatchedKeys;
}

int32 UScenarioGameInstanceBase::GetImageCountForTest(FName Category, FName TestName) const
{
    int32 Count = 0;
    for (const FResultImageInfo& Info : ActiveImageInfos)
    {
        if (Info.Category == Category && Info.TestName == TestName) Count++;
    }
    return Count;
}

bool UScenarioGameInstanceBase::SaveScenarioToPersistentStorage(FName ScenarioID, const TArray<FResultImage>& Images)
{
    if (ScenarioID.ToString().IsEmpty() || Images.Num() == 0) return false;
    FString TargetDir = GetScenarioDirectoryPath(ScenarioID);

    for (const FResultImage& Img : Images)
    {
        FString FileName = Img.GetUniqueKey() + TEXT(".data");
        FString FullPath = TargetDir / FileName;
        if (!FFileHelper::SaveArrayToFile(Img.Bytes, *FullPath)) return false;
    }
    return true;
}

void UScenarioGameInstanceBase::SetCurrentScenarioID(FName InScenarioID)
{
    CurrentScenarioID = InScenarioID;
}

bool UScenarioGameInstanceBase::LoadScenarioToMemory()
{
    if (CurrentScenarioID.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("GameInstance: CurrentScenarioID가 지정되지 않아 메모리 로드를 취소합니다."));
        return false;
    }

    TextureCacheMap.Empty();
    ActiveScenarioFilePathMap.Empty();
    ActiveImageInfos.Empty();

    IFileManager& FileManager = IFileManager::Get();
    TArray<FString> FoundFiles;
    FString TargetDir = GetScenarioDirectoryPath(CurrentScenarioID);
    FileManager.FindFiles(FoundFiles, *TargetDir, TEXT("data"));

    if (FoundFiles.Num() == 0)
    {
        TargetDir = GetBuiltInDirectoryPath(CurrentScenarioID);
        FileManager.FindFiles(FoundFiles, *TargetDir, TEXT("data"));
    }

    for (const FString& File : FoundFiles)
    {
        FString FullPath = TargetDir / File;
        FString CleanFileName = FPaths::GetBaseFilename(File);
        FString CategoryStr;
        FString RestStr;

        if (CleanFileName.Split(TEXT("_"), &CategoryStr, &RestStr))
        {
            FString TestNameStr;
            FString DetailItemStr;
            if (RestStr.Split(TEXT("_"), &TestNameStr, &DetailItemStr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
            {
                FResultImageInfo Info;
                Info.Category = FName(*CategoryStr);
                Info.TestName = FName(*TestNameStr);
                Info.DetailItem = FName(*DetailItemStr);
                ActiveImageInfos.Add(Info);
                ActiveScenarioFilePathMap.Add(FName(*CleanFileName), FullPath);
            }
        }
    }

    ActiveBloodTestMap.Empty();
    FString JsonFilePath = TargetDir / TEXT("BloodTest.json");
    FString JsonString;

    if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
    {
        TargetDir = GetBuiltInDirectoryPath(CurrentScenarioID);
        JsonFilePath = TargetDir / TEXT("BloodTest.json");
        FFileHelper::LoadFileToString(JsonString, *JsonFilePath);
    }

    if (!JsonString.IsEmpty()) ParseBloodTestJson(JsonString);

    return ActiveScenarioFilePathMap.Num() > 0 || ActiveBloodTestMap.Num() > 0;
}

void UScenarioGameInstanceBase::LoadTextureAsync(FName ImageKey, FOnTextureLoadComplete OnComplete)
{
    if (UTexture2D** CachedTexPtr = TextureCacheMap.Find(ImageKey))
    {
        if (*CachedTexPtr && (*CachedTexPtr)->IsValidLowLevel())
        {
            OnComplete.ExecuteIfBound(*CachedTexPtr);
            return;
        }
    }

    const FString* FoundPath = ActiveScenarioFilePathMap.Find(ImageKey);
    if (!FoundPath)
    {
        OnComplete.ExecuteIfBound(nullptr);
        return;
    }

    FString LocalPath = *FoundPath;
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [LocalPath, OnComplete]()
        {
            TArray<uint8> CompressedBuffer;
            if (FFileHelper::LoadFileToArray(CompressedBuffer, *LocalPath))
            {
                IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
                EImageFormat Format = ImageWrapperModule.DetectImageFormat(CompressedBuffer.GetData(), CompressedBuffer.Num());
                TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);

                if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(CompressedBuffer.GetData(), CompressedBuffer.Num()))
                {
                    TArray<uint8> RawRGBA;
                    if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawRGBA))
                    {
                        int32 Width = ImageWrapper->GetWidth();
                        int32 Height = ImageWrapper->GetHeight();

                        AsyncTask(ENamedThreads::GameThread, [Width, Height, MoveRaw = MoveTemp(RawRGBA), OnComplete]()
                            {
                                UTexture2D* NewTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
                                if (NewTexture)
                                {
                                    void* TextureData = NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
                                    FMemory::Memcpy(TextureData, MoveRaw.GetData(), MoveRaw.Num());
                                    NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
                                    NewTexture->UpdateResource();
                                }
                                OnComplete.ExecuteIfBound(NewTexture);
                            });
                        return;
                    }
                }
            }
            AsyncTask(ENamedThreads::GameThread, [OnComplete]() { OnComplete.ExecuteIfBound(nullptr); });
        });
}

UTexture2D* UScenarioGameInstanceBase::CreateTextureFromBytes(const TArray<uint8>& RawData)
{
    if (RawData.Num() == 0) return nullptr;
    return FImageUtils::ImportBufferAsTexture2D(RawData);
}

TArray<FResultImageInfo> UScenarioGameInstanceBase::GetActiveScenarioImageInfos() const
{
    return ActiveImageInfos;
}

bool UScenarioGameInstanceBase::GetActiveImageData(FName ImageKey, TArray<uint8>& OutBytes) const
{
    if (const FString* FoundPath = ActiveScenarioFilePathMap.Find(ImageKey))
    {
        return FFileHelper::LoadFileToArray(OutBytes, **FoundPath);
    }
    return false;
}

TArray<FName> UScenarioGameInstanceBase::GetAvailableCategories() const
{
    TSet<FName> UniqueCategories;
    for (const FResultImageInfo& Info : ActiveImageInfos) UniqueCategories.Add(Info.Category);
    return UniqueCategories.Array();
}

TArray<FName> UScenarioGameInstanceBase::GetTestNamesByCategory(FName Category) const
{
    TSet<FName> UniqueTestNames;
    for (const FResultImageInfo& Info : ActiveImageInfos)
    {
        if (Info.Category == Category) UniqueTestNames.Add(Info.TestName);
    }
    return UniqueTestNames.Array();
}

TArray<FName> UScenarioGameInstanceBase::GetDetailItemsByTest(FName Category, FName TestName) const
{
    TSet<FName> UniqueDetailItems;
    for (const FResultImageInfo& Info : ActiveImageInfos)
    {
        if (Info.Category == Category && Info.TestName == TestName) UniqueDetailItems.Add(Info.DetailItem);
    }
    return UniqueDetailItems.Array();
}