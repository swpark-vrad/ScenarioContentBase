#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "InteractableTagInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractableTagInterface : public UInterface
{
	GENERATED_BODY()
};

/** 시뮬레이터 내의 모든 도구(가위, 청진기 등) 및 폰(상하차 핸드 등)이 공용으로 장착할 태그 통신 규격 */
class SCENARIOCONTENT_API IInteractableTagInterface
{
	GENERATED_BODY()

public:
	/** 도구나 폰의 고유 식별 명칭 태그를 반환 (예: Item.Tool.Scissors, Character.Player.LeftHand) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Scenario|Interface")
	FGameplayTag GetUniqueIDTag() const;

	/** 도구나 폰의 현재 실시간 상태 태그를 반환 (예: InputState.Grab, InputState.Trigger, InputState.None) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Scenario|Interface")
	FGameplayTagContainer GetStateTags() const;
};