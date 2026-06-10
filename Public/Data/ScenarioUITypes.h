#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Global/ScenarioGameplayTags.h"
#include "ScenarioUITypes.generated.h"


// UI 표시용 엔트리 데이터 구조체
USTRUCT(BlueprintType)
struct FScenarioEntryUIData
{
	GENERATED_BODY()

	// UI 레이어에서 고유 식별 및 RPC 요청에 사용할 EntryID 태그 멤버를 추가합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|UI")
	FGameplayTag EntryID;

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