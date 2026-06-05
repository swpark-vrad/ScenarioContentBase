#include "Global/ScenarioGameplayTags.h"
#include "GameplayTagsManager.h"

FScenarioGameplayTags FScenarioGameplayTags::Tags;

void FScenarioGameplayTags::InitializeNativeTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// AddNativeGameplayTag를 통해 엔진 구동 시점에 태그를 직접 강제 등록합니다.
	// 두 번째 인자는 에디터에서 보일 개발자용 힌트 주석입니다.
	Tags.EntryID = Manager.AddNativeGameplayTag(FName("EntryID"), TEXT("엔트리ID 루트"));
	Tags.Interaction = Manager.AddNativeGameplayTag(FName("Interaction"), TEXT("행동 루트"));
	Tags.Object = Manager.AddNativeGameplayTag(FName("Object"), TEXT("도구 루트"));
}