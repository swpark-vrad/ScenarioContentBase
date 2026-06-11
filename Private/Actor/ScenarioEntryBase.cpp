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
		TargetZone->OnInteractionTriggered.AddDynamic(this, &AScenarioEntryBase::BroadcastProcessPayload);
		AssociatedZones.AddUnique(TargetZone);
	}
}

const TArray<class AInteractionZoneBase*>& AScenarioEntryBase::GetAssociatedZones() const
{
	return AssociatedZones;
}

 bool AScenarioEntryBase::FindAssociatedZone(FGameplayTag ZoneID, AInteractionZoneBase*& FindZone) const
{
	if (!ZoneID.IsValid()) return false;

	for (AInteractionZoneBase* Zone : AssociatedZones)
	{
		if (Zone->ZoneID == ZoneID)
		{
			FindZone = Zone;
			return true;
		}
	}
	FindZone = nullptr;
	return false;
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

void AScenarioEntryBase::BroadcastProcessPayload(const FInteractionPayload& Payload)
{
	// [핵심 변경] 활성화(bIsActive) 상태가 아니거나 이미 끝났으면 심사하지 않고 무시함
	if (!HasAuthority()) return;

	ProcessPayload(Payload);

	if (CheckTargetInteraction(Payload))
	{
		CurrentExecutionCount++;
		CompleteEntry();
	}
}

void AScenarioEntryBase::ProcessPayload_Implementation(const FInteractionPayload& Payload)
{
}

bool AScenarioEntryBase::CheckTargetInteraction_Implementation(const FInteractionPayload& Payload) const
{
	return Payload.InteractionTags.HasTag(TargetInteractionTag);
}

void AScenarioEntryBase::CompleteEntry()
{
	if (HasAuthority() && bIsActive && !bIsCompleted)
	{
		bIsCompleted = true;

		// 엔트리가 완료되었으므로 더 이상 체크하지 않도록 비활성화
		EndEntry();

		// 클라이언트 동기화 및 페이즈의 성공 조건 판단 리스너 가동
		OnRep_IsCompleted();
		OnEntryCompleted.Broadcast(this);
	}
}

void AScenarioEntryBase::ForceToCompleteEntry()
{
	// 무조건 서버 권한을 가졌고, 이미 완료된 엔트리가 아닐 때만 강제 변조를 허용합니다.
	if (!HasAuthority() || bIsCompleted) return;

	// 1. 완수 플래그를 즉시 참으로 변경합니다.
	bIsCompleted = true;

	// 2. bIsActive 여부와 상관없이 이 엔트리의 심사 상태를 강제 종료합니다.
	EndEntry();

	// 3. 로컬 호스트 화면 반영 및 상위 페이즈 시스템으로 성공 신호를 즉시 전송합니다.
	OnRep_IsCompleted();
	OnEntryCompleted.Broadcast(this);

	UE_LOG(LogTemp, Warning, TEXT("Entry: [%s] 엔트리가 관리자 요청에 의해 강제 성공 처리되었습니다."), *EntryName.ToString());
}