#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/ScenarioSaveTypes.h"
#include "ScenarioBuilderSaveGame.generated.h"

UCLASS()
class SCENARIOCONTENT_API UScenarioBuilderSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	// 자신이 저장된 실제 파일 이름 (예: "Scenario_1234abcd...")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Save")
	FString SaveSlotName;

	// 시나리오의 기초 정보 및 메타 데이터 (환자 정보, 시작 페이즈 이름 등)
	// 시나리오의 기본정보만 사용하고 페이즈 데이터는 하단의 SavedNode를 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Save")
	FScenarioSaveData BaseScenarioData;

	// 시작, 종료노드는 ScenarioSaveData에 포함되지 않기 때문에 별도로 저장
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	FVector2D StartNodePosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|SaveData")
	FVector2D EndNodePosition = FVector2D::ZeroVector;

	// 전체 페이즈 노드들 (Key: PhaseID, Value: 노드 알맹이 데이터 + 위젯 위치)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Save")
	TMap<FName, FPhaseNodeSaveData> SavedNodes;
};