#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/** 시뮬레이터 전역에서 공용으로 사용할 네이티브 태그 저장소 */
struct SCENARIOCONTENT_API FScenarioGameplayTags
{
public:
	static const FScenarioGameplayTags& Get() { return Tags; }

	// 이니 파일 없이 코드에서 태그를 직접 주입하는 핵심 함수
	static void InitializeNativeTags();

	// 기본적인 태그 루트
	FGameplayTag EntryID;
	FGameplayTag Interaction;
	FGameplayTag Object;
	FGameplayTag Zone;
	FGameplayTag Treatment;

	FGameplayTag Object_Patient;

	FGameplayTag Object_Hand;
	FGameplayTag Object_Hand_R;
	FGameplayTag Object_Hand_L;

	// 입력 상태 확인용 태그
	FGameplayTag InputState;
	FGameplayTag InputState_Grab;
	FGameplayTag InputState_Trigger;

private:
	static FScenarioGameplayTags Tags;
};