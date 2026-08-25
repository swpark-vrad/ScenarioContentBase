// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Data/ScenarioDataTypes.h"
#include "Data/TestResultData.h"
#include "ScenarioGameInstanceBase.generated.h"

class UScenarioSaveGame;

// 블루프린트 위젯 비동기 바인딩용 단일 매개변수 델리게이트
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnTextureLoadComplete, UTexture2D*, LoadedTexture);

UCLASS()
class SCENARIOCONTENT_API UScenarioGameInstanceBase : public UGameInstance
{
    GENERATED_BODY()

public:
    // ======================================================================
    // 1. 핵심 시나리오 제어 및 수명주기 (Core Lifecycle)
    // ======================================================================

    /** 현재 시나리오 고유 ID 전역 설정 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|Scenario")
    void SetCurrentScenarioID(FName InScenarioID);

    /** 현재 실행 중인 시나리오 ID 반환 */
    UFUNCTION(BlueprintPure, Category = "GameInstance|Scenario")
    FName GetCurrentScenarioID() const { return CurrentScenarioID; }

    /** 선택된 시나리오 파일들을 영구 저장소에서 메모리로 로드 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|Scenario")
    bool LoadScenarioToMemory();

    /** 시나리오 ID 분석을 통한 내장(Built-In) 프리셋 여부 판별 */
    UFUNCTION(BlueprintPure, Category = "GameInstance|Scenario")
    bool IsBuiltInScenario(const FName& ScenarioID) const;

    /** ID 코드 형태를 기반으로 빌트인/커스텀 경로를 자동 판별하여 세이브 데이터를 로드합니다 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|Scenario")
    class UScenarioSaveGame* LoadScenarioData(FName ScenarioID);

    // 컨텐츠 진행모드
    UPROPERTY(BlueprintReadWrite, Category = "GameInstance|Scenario")
    EScenarioMode SelectedScenarioMode = EScenarioMode::Practice;

    // ======================================================================
    // 2. 의료 영상 이미지 데이터 조회 시스템 (Scenario Image Query)
    // ======================================================================

    /** 메모리에 로드된 데이터에서 중복되지 않은 모든 의료 영상 카테고리 목록 반환 */
    UFUNCTION(BlueprintPure, Category = "GameInstance|ResultImage")
    TArray<FName> GetAvailableCategories() const;

    /** 특정 카테고리에 속한 모든 검사이름 목록 반환 */
    UFUNCTION(BlueprintPure, Category = "GameInstance|ResultImage")
    TArray<FName> GetTestNamesByCategory(FName Category) const;

    /** 카테고리와 검사이름이 일치하는 고유 세부항목 배열 반환 */
    UFUNCTION(BlueprintCallable, Category = "Scenario|ResultImage")
    TArray<FName> GetDetailItemsByTest(FName Category, FName TestName) const;

    /** 특정 카테고리의 검사명에 해당하는 모든 이미지 키 배열 반환 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|ResultImage")
    TArray<FName> GetImageKeysForTest(FName Category, FName TestName) const;

    /** 특정 카테고리의 검사명에 해당하는 이미지 총 개수 반환 */
    UFUNCTION(BlueprintPure, Category = "GameInstance|ResultImage")
    int32 GetImageCountForTest(FName Category, FName TestName) const;

    /** 현재 활성화된 시나리오의 이미지 메타데이터 전체 목록 반환 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|ResultImage")
    TArray<FResultImageInfo> GetActiveScenarioImageInfos() const;

    /** 원본 이미지 바이너리 데이터 인출 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|ResultImage")
    bool GetActiveImageData(FName ImageKey, TArray<uint8>& OutBytes) const;

    // ======================================================================
    // 3. 비동기 텍스처 스트리밍 및 그래픽 처리 (Async Streaming & Processing)
    // ======================================================================

    /** 특정 이미지 키에 해당하는 원본 파일을 백그라운드 스레드에서 비동기 로드 */
    UFUNCTION(BlueprintCallable, Category = "Scenario|Async")
    void LoadTextureAsync(FName ImageKey, FOnTextureLoadComplete OnComplete);

    /** 로우 바이너리 바이트 배열을 컴포넌트용 CPU 텍스처 객체로 파싱 */
    UTexture2D* CreateTextureFromBytes(const TArray<uint8>& RawData);

    // 로드하고 맵에 저장된 메모리 해제
    UFUNCTION(BlueprintCallable, Category = "Scenario|Memory")
    void ClearScenarioMemoryCache();

    // ======================================================================
    // 4. 혈액검사 수치 데이터 인터페이스 (Blood Test System)
    // ======================================================================

    /** 혈액검사 데이터에서 중복되지 않은 메인 카테고리 목록 반환 */
    UFUNCTION(BlueprintPure, Category = "GameInstance|BloodTest")
    TArray<FName> GetAvailableBloodCategories() const;

    /** 특정 혈액검사 카테고리에 속하는 세부 표 행 데이터 배열 반환 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|BloodTest")
    TArray<FBloodTestRow> GetBloodTestRowsByCategory(FName Category) const;

    // ======================================================================
    // 5. 호스트 전용 시나리오 빌더 툴 아키텍처 (Scenario Builder Tools)
    // ======================================================================

    /** OS 탐색기를 열어 다중 이미지를 로드하고 구조체 배열로 가공 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|Builder")
    bool OpenFileDialogAndLoadImages(FName Category, FName TestName, TArray<FResultImage>& OutLoadedImages);

    /** 로드된 다수의 이미지 파일들을 플랫폼 영구 저장소에 저장 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|Scenario")
    bool SaveScenarioToPersistentStorage(FName ScenarioID, const TArray<FResultImage>& Images);

    // ======================================================================
    // 6. 유틸리티 및 진단 시스템 (Utilities & Diagnostics)
    // ======================================================================

    /** 파라미터 조합을 통해 고유 매핑 해시 키(ImageKey) 생성 */
    UFUNCTION(BlueprintPure, Category = "GameInstance|Utility")
    FORCEINLINE FName CreateImageKey(FName Category, FName TestName, FName DetailItem) const
    {
        return FName(*(Category.ToString() + TEXT("_") + TestName.ToString() + TEXT("_") + DetailItem.ToString()));
    }

    /** 활성화된 시나리오의 이미지 스트리밍 상태 디버그 출력 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|Debug")
    void DumpActiveScenarioImages();

    /** 호스트 전용 세션 파괴 및 이동 요청 */
    void ShutdownSessionAsHost();

    /** 클라이언트 전용 세션 이탈 및 VR 로비 이동 */
    UFUNCTION(BlueprintCallable, Category = "GameInstance|Session")
    void LeaveSessionAsClient(APlayerController* RequestingPC);

protected:
    // ======================================================================
    // 런타임 활성 데이터 캐시 메모리 저장소 (Runtime Active Memory Caching)
    // ======================================================================

    UPROPERTY()
    TMap<FName, FString> ActiveScenarioFilePathMap;

    UPROPERTY()
    TMap<FName, UTexture2D*> TextureCacheMap;

    UPROPERTY()
    TMap<FName, FBloodTestCategory> ActiveBloodTestMap;

    void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

    void MoveToHostLobby();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Settings", meta = (AllowedClasses = "/Script/Engine.World"))
    TSoftObjectPtr<UWorld> HostLobbyLevel;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Settings", meta = (AllowedClasses = "/Script/Engine.World"))
    TSoftObjectPtr<UWorld> ClientLobbyLevel;

private:
    // ======================================================================
    // 내부 헬퍼 및 보안 상태 관리 (Internal Helpers & Hidden States)
    // ======================================================================

    bool ParseBloodTestJson(const FString& JsonString);

    FString GetScenarioDirectoryPath(const FName& ScenarioID) const;

    FString GetBuiltInDirectoryPath(const FName& ScenarioID) const;

    UPROPERTY()
    FName CurrentScenarioID;

    UPROPERTY()
    TMap<FName, FByteDataBuffer> ActiveScenarioImageMap;

    UPROPERTY()
    TArray<FResultImageInfo> ActiveImageInfos;
};