#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ScenarioPlayerAppearance.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UScenarioPlayerAppearance : public UInterface
{
	GENERATED_BODY()
};

class SCENARIOCONTENT_API IScenarioPlayerAppearance
{
	GENERATED_BODY()

public:
	/** 1. 폰의 초기 신원 정보(인덱스, 고유 색상)를 설정하는 함수 (바뀔 일 없음) */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Scenario|Appearance")
	void UpdatePawnIdentity(int32 InUserIndex, const FLinearColor& InUserColor);

	/** 2. 폰의 이름을 실시간으로 변경하는 함수 (자주 바뀔 수 있음) */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Scenario|Appearance")
	void UpdatePawnName(const FString& InUserName);

	/** 3. 폰의 머리위치 컴포넌트 리턴하는 함수 */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Scenario|Appearance")
	USceneComponent* GetHeadPivotComponent();
};