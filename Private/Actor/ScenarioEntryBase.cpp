#include "Actor/ScenarioEntryBase.h"
#include "Actor/ScenarioPhaseBase.h"
#include "Actor/InteractionZoneBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

AScenarioEntryBase::AScenarioEntryBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AScenarioEntryBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AScenarioEntryBase, bIsActive); // 활성화 상태 동기화
	DOREPLIFETIME(AScenarioEntryBase, CurrentExecutionCount);
	DOREPLIFETIME(AScenarioEntryBase, bIsCompleted);
}

FString AScenarioEntryBase::GetActualName_Implementation() const
{
	return EntryName.ToString();
}

void AScenarioEntryBase::OnRep_IsCompleted() {}

void AScenarioEntryBase::InitializeEntry(AScenarioPhaseBase* InOwnerPhase, AInteractionZoneBase* TargetZone)
{
	if (!HasAuthority()) return;

	OwnerPhase = InOwnerPhase;

	if (TargetZone)
	{
		TargetZone->OnInteractionTriggered.AddDynamic(this, &AScenarioEntryBase::ProcessPayload);
	}
}

// =====================================
// 생명주기 관리 (Start / End)
// =====================================
void AScenarioEntryBase::StartEntry()
{
	if (!HasAuthority() || bIsCompleted) return;

	bIsActive = true;
	ReceiveStartEntry(); // BP 이벤트 호출
}

void AScenarioEntryBase::EndEntry()
{
	if (!HasAuthority()) return;

	bIsActive = false;
	ReceiveEndEntry(); // BP 이벤트 호출
}

// C++ 기본 구현부 (BP에서 비워둬도 에러 안 나게)
void AScenarioEntryBase::ReceiveStartEntry_Implementation() {}
void AScenarioEntryBase::ReceiveEndEntry_Implementation() {}
// =====================================

void AScenarioEntryBase::ProcessPayload(const FInteractionPayload& Payload)
{
	// [핵심 변경] 활성화(bIsActive) 상태가 아니거나 이미 끝났으면 심사하지 않고 무시함
	if (!HasAuthority() || !bIsActive || bIsCompleted) return;

	if (CheckSuccessCondition(Payload))
	{
		CompleteEntry();
	}
}

bool AScenarioEntryBase::CheckSuccessCondition_Implementation(const FInteractionPayload& Payload) const
{
	return Payload.InteractionTag.MatchesTag(TargetInteractionTag);
}

void AScenarioEntryBase::CompleteEntry()
{
	if (HasAuthority() && !bIsCompleted)
	{
		CurrentExecutionCount++;

		if (CurrentExecutionCount >= TargetExecutionCount)
		{
			bIsCompleted = true;

			// 엔트리가 완료되었으므로 더 이상 체크하지 않도록 비활성화
			EndEntry();

			// 클라이언트 동기화 및 페이즈의 성공 조건 판단 리스너 가동
			OnRep_IsCompleted();
			OnEntryCompleted.Broadcast(this);
		}
	}
}