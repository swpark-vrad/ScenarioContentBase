#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Global/ScenarioGameplayTags.h"
#include "ScenarioSaveTypes.generated.h"


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
	FPatientPartState PatientPartState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	TArray<FPhaseSaveData> Phases;
};