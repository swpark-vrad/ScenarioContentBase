#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/ScenarioDataTypes.h"
#include "ScenarioSaveGame.generated.h"

UCLASS()
class SCENARIOCONTENT_API UScenarioSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 파일 하나당 단 하나의 시나리오 통짜 데이터를 들고 있게 만듭니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Scenario|Save")
	FScenarioSaveData ScenarioData;
};