#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ScenarioDataTypes.generated.h"

// =================================================================
// 1. 센서와 통제탑 간의 통신 데이터 (Payload)
// =================================================================
USTRUCT(BlueprintType)
struct FInteractionPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	FGameplayTag UniqueID;		// 특정 엔트리르 가리킬 태그

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	FGameplayTag InteractionTag; // 어떤 행동인가? (예: Action.Clean)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	FGameplayTag ToolTag;        // 무슨 도구인가? (예: Item.Gauze)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	AActor* ToolActor;           // 상호작용한 도구 액터의 실제 레퍼런스

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	TMap<FGameplayTag, FString> AdditionalData; // 약물명, 용량 등 추가 정보 딕셔너리
};

// =================================================================
// 2. BP_IZ 동적 스폰 세팅 데이터
// =================================================================
USTRUCT(BlueprintType)
struct FZoneData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FName TargetSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FVector BoxExtent = FVector(10.f, 10.f, 10.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FTransform RelativeOffset;

	// 메모리 최적화를 위한 Soft Object Pointer 적용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	TSoftObjectPtr<class UStaticMesh> HighlightMesh = nullptr;
};

// =================================================================
// 3. 데이터 테이블 관리용 구조체 (FTableRowBase 상속 필수)
// =================================================================
USTRUCT(BlueprintType)
struct FZoneSpawnRow : public FTableRowBase
{
	GENERATED_BODY()

	// 스폰할 BP_IZ의 서브클래스 (메모리 최적화를 위해 Soft Class Pointer 적용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	TSoftClassPtr<class AActor> ZoneClass;

	// 스폰된 클래스에 주입할 세팅 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FZoneData ZoneData;
};

// UI 표시용 엔트리 데이터 구조체
USTRUCT(BlueprintType)
struct FScenarioEntryUIData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|UI")
	FName EntryName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|UI")
	bool bIsMandatory = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|UI")
	bool bIsCompleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|UI")
	int32 CompletionTime = -1;
};

USTRUCT(BlueprintType)
struct FPhaseHistoryData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|History")
	TArray<FScenarioEntryUIData> Entries;
};

// =================================================================
// 1. 마스터 데이터테이블 규격 (엔트리의 모든 속성을 고정 가동하는 마스터 템플릿)
// (RowName을 EntryName 고유 키로 공용 사용)
// =================================================================
USTRUCT(BlueprintType)
struct FScenarioEntryTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MasterData")
	FGameplayTag EntryID;

	// 실제 월드에 스폰될 C++ 또는 블루프린트 엔트리 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MasterData")
	TSubclassOf<class AScenarioEntryBase> EntryClass;

	// [신규] 상호작용 발생 시 전달된 페이로드 태그와 비교할 목표 태그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MasterData")
	FGameplayTag TargetInteractionTag;

	// 이 처치 행동의 목표 수행 횟수 기본값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MasterData")
	int32 TargetExecutionCount = 1;
};

// =================================================================
// 2. 시나리오 데이터 직렬화용 세이브 구조체 세트
// =================================================================
USTRUCT(BlueprintType)
struct FEntrySaveData
{
	GENERATED_BODY()

	// 마스터 데이터테이블의 RowName (곧 처치 행동의 고유 ID 키)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	FName EntryRowName;

	// 이번 페이즈 배치 상태에서 해당 처치가 필수인지 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveData")
	bool bIsMandatory = true;
};

USTRUCT(BlueprintType)
struct FPhaseSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	FName PhaseName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	float TimeLimit = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	TArray<FEntrySaveData> Entries;

	// 성공했을 때 전환될 다음 페이즈 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	FName NextSuccessPhaseName;

	// 시간 초과 등 실패 조건이 발동했을 때 전환될 다음 페이즈 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	FName NextFailurePhaseName;
};

USTRUCT(BlueprintType)
struct FScenarioSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	FName ScenarioID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	TArray<FPhaseSaveData> Phases;
};