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
	Tags.Zone = Manager.AddNativeGameplayTag(FName("Zone"), TEXT("존 루트"));
	Tags.Treatment = Manager.AddNativeGameplayTag(FName("Treatment"), TEXT("처치 루트"));

	Tags.Object_Patient = Manager.AddNativeGameplayTag(FName("Object.Patient"), TEXT("환자"));

	Tags.Object_Hand = Manager.AddNativeGameplayTag(FName("Object.Hand"), TEXT("손"));
	Tags.Object_Hand_R = Manager.AddNativeGameplayTag(FName("Object.Hand.Right"), TEXT("오른손"));
	Tags.Object_Hand_L = Manager.AddNativeGameplayTag(FName("Object.Hand.Left"), TEXT("왼손"));

	Tags.InputState = Manager.AddNativeGameplayTag(FName("InputState"), TEXT("입력 상태 루트"));
	Tags.InputState_Grab = Manager.AddNativeGameplayTag(FName("InputState.Grab"), TEXT("그랩 상태"));
	Tags.InputState_Trigger = Manager.AddNativeGameplayTag(FName("InputState.Trigger"), TEXT("트리거 상태"));
}