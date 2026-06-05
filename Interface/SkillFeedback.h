#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SkillFeedback.generated.h"

// 이 클래스는 언리얼 엔진의 리플렉션 시스템을 위해 존재하며 직접 수정하지 않습니다.
UINTERFACE(MinimalAPI, BlueprintType)
class USkillFeedback : public UInterface
{
	GENERATED_BODY()
};

/*
 * 통제탑(Phase/Entry)이 상호작용의 성공 및 실패 여부를
 * 타깃(환자/기기)과 도구에게 전달하여 자율적인 연출을 지시하는 인터페이스
 */
class SCENARIOCONTENT_API ISkillFeedback
{
	GENERATED_BODY()

public:
	/*
	 * 상호작용(술기)이 성공적으로 완료되었을 때 호출됩니다.
	 * BlueprintNativeEvent로 선언되어 블루프린트에서 Event 노드로 구현 가능합니다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Simulator|Feedback")
	void OnSkillSuccessfullyApplied(FGameplayTag ActionTag, FName TargetSocket);

	/*
	 * 상호작용(술기)이 실패했을 때 호출됩니다. (잘못된 도구 사용, 위치 오류 등)
	 * 오답 피드백(경고음, 빨간색 하이라이트 등)을 연출할 때 사용합니다.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Simulator|Feedback")
	void OnSkillFailed(FGameplayTag ActionTag, FName TargetSocket);
};