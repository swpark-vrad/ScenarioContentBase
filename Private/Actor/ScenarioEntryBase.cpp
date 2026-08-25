#include "Actor/ScenarioEntryBase.h"
#include "Actor/ScenarioPhaseBase.h"
#include "Actor/InteractionZoneBase.h"
#include "Actor/ScenarioPatientBase.h"
#include "Global/ScenarioGameStateBase.h"
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
	DOREPLIFETIME(AScenarioEntryBase, TotalExecutionCount);
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

	// 엔트리(미션)가 실무 심사에 들어가는 순간 환자의 바이탈을 연산 규칙에 맞춰 실시간 변조
	if (AScenarioGameStateBase* GS = Cast<AScenarioGameStateBase>(GetWorld()->GetGameState()))
	{
		if (GS->GetSpawnedPatient())
		{
			GS->GetSpawnedPatient()->ApplyVitalModifier(VitalModifier);
		}
	}

	bIsActive = true;
	CurrentExecutionCount = 0;

	// 페이즈 변경시 이미 수행했을 경우 성공처리
	if (CheckAutoCompletionCondition())
	{
		CompleteEntry();
		return;
	}
	
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

bool AScenarioEntryBase::CheckAutoCompletionCondition_Implementation() const
{
	return false;
}

bool AScenarioEntryBase::IsTargetExecutionCountReached() const
{
	// TargetExecutionCount가 설정되지 않았다면(-1 이하), 1회만 수행했어도(> 0) 만족한 것으로 간주
	if (TargetExecutionCount <= 0)
	{
		return CurrentExecutionCount > 0;
	}

	// 목표 횟수가 설정되어 있다면, 그 횟수 이상을 수행했는지 확인
	return CurrentExecutionCount >= TargetExecutionCount;
}

void AScenarioEntryBase::BroadcastProcessPayload(const FInteractionPayload& Payload)
{
	if (!HasAuthority()) return;

	CachedCurrentPayload = Payload;
	ProcessPayload(Payload);

	if (CheckTargetInteraction(Payload))
	{
		// 전체 수행횟수 증가
		TotalExecutionCount++;

		// 활성화되어있다면 현재 수행횟수 증가
		if (bIsActive)
		{
			CurrentExecutionCount++;

			if (IsTargetExecutionCountReached())
			{
				CompleteEntry();
			}
		}
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
		OnEntryCompleted.Broadcast(this, false);
	}
}

void AScenarioEntryBase::ForceToCompleteEntry()
{
	// 무조건 서버 권한을 가졌고, 이미 완료된 엔트리가 아닐 때만 강제 변조를 허용합니다.
	if (!HasAuthority() || bIsCompleted) return;

	// 1. 강제 완료시 수행될 인터랙션을 적용할 함수 호출
	OnManualInteractionTriggered();

	// 2. 완수 플래그를 즉시 참으로 변경합니다.
	bIsCompleted = true;

	// 3. bIsActive 여부와 상관없이 이 엔트리의 심사 상태를 강제 종료합니다.
	EndEntry();

	// 4. 로컬 호스트 화면 반영 및 상위 페이즈 시스템으로 성공 신호를 즉시 전송합니다.
	OnRep_IsCompleted();
	OnEntryCompleted.Broadcast(this, true);

	//UE_LOG(LogTemp, Warning, TEXT("Entry: [%s] 엔트리가 관리자 요청에 의해 강제 성공 처리되었습니다."), *EntryName.ToString());
}

FString AScenarioEntryBase::MakeExecuteMessage_Implementation() const
{
	// 예: "[Entry.Tag.Name] 미션을 완료했습니다." 형태의 기본 문자열 반환
	return FString::Printf(TEXT("%s 미션을 완료했습니다."), *EntryID.ToString());
}


void AScenarioEntryBase::OnManualInteractionTriggered_Implementation()
{
}
