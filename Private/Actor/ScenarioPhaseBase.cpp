#include "Actor/ScenarioPhaseBase.h"
#include "Actor/ScenarioEntryBase.h"
#include "Actor/InteractionZoneBase.h"
#include "Actor/ScenarioPatientBase.h"
#include "Global/ScenarioGameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

AScenarioPhaseBase::AScenarioPhaseBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AScenarioPhaseBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AScenarioPhaseBase, bIsPhaseCompleted);
	DOREPLIFETIME(AScenarioPhaseBase, bIsPhaseSuccess);
	DOREPLIFETIME(AScenarioPhaseBase, ActiveEntries);
}

void AScenarioPhaseBase::OnRep_IsPhaseCompleted() {}

void AScenarioPhaseBase::StartPhase()
{
	if (!HasAuthority()) return;

	PhaseState = EPhaseState::Active;

	if (AScenarioGameStateBase* GS = Cast<AScenarioGameStateBase>(GetWorld()->GetGameState()))
	{
		PhaseStartTime = GS->GetServerWorldTimeSeconds();
		// 페이즈 시작시 환자 VS 수정치 적용
		if (GS->GetSpawnedPatient())
		{
			GS->GetSpawnedPatient()->ApplyVitalModifier(VitalModifier);
		}
	}

	// 1. 블루프린트 시작 이벤트 호출 (환자 스폰 등 처리)
	ReceiveStartPhase();

	// 2. 내가 관리하는 모든 엔트리들의 체크(심사)를 활성화
	for (AScenarioEntryBase* Entry : ActiveEntries)
	{
		if (Entry)
		{
			Entry->StartEntry();
		}
	}

	// 3. 타임아웃 세팅
	if (TimeLimit > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PhaseTimerHandle, this, &AScenarioPhaseBase::OnPhaseTimeout, TimeLimit, false);
	}
}

// C++ 기본 구현부
void AScenarioPhaseBase::ReceiveStartPhase_Implementation() {}
void AScenarioPhaseBase::ReceiveEndPhase_Implementation(EPhaseState EndCondition) {}

void AScenarioPhaseBase::OnPhaseTimeout()
{
	if (!HasAuthority() || bIsPhaseCompleted) return;
	EndPhase(EPhaseState::Timeover);
}

void AScenarioPhaseBase::HandleEntryCompleted(AScenarioEntryBase* CompletedEntry)
{
	if (!HasAuthority() || bIsPhaseCompleted) return;
	CheckPhaseCompletion();
}

void AScenarioPhaseBase::CheckPhaseCompletion()
{
	if (!HasAuthority()) return;

	bool bAllMandatoryCompleted = true;

	for (AScenarioEntryBase* Entry : ActiveEntries)
	{
		if (Entry && Entry->bIsMandatory && !Entry->bIsCompleted)
		{
			bAllMandatoryCompleted = false;
			break;
		}
	}

	if (bAllMandatoryCompleted)
	{
		EndPhase(EPhaseState::Completed);
	}
}

void AScenarioPhaseBase::EndPhase(EPhaseState EndCondition)
{
	// 이미 최종 완료 처리된 상태라면 중복 호출 차단
	if (PhaseState == EPhaseState::Completed) return;

	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

	// 전달받은 종료 조건 상태를 그대로 반영
	PhaseState = EndCondition;

	// 완전히 세션이 종료되는 상태(성공 혹은 완전실패)일 때만 하위 엔트리들을 닫아줍니다.
	// Overtime(유예) 상태일 때는 이 루프를 건너뛰어 엔트리 상호작용 채널을 살려둡니다.
	if (PhaseState == EPhaseState::Completed)
	{
		for (AScenarioEntryBase* Entry : ActiveEntries)
		{
			if (Entry)
			{
				Entry->EndEntry();
			}
		}
	}

	// 블루프린트 연출 및 전파 가동
	ReceiveEndPhase(EndCondition);
	OnRep_IsPhaseCompleted();
	OnPhaseCompleted.Broadcast(this, EndCondition);
}

void AScenarioPhaseBase::DeactivateEntries()
{
	for (AScenarioEntryBase* Entry : ActiveEntries)
	{
		if (Entry && Entry->bIsActive)
		{
			Entry->EndEntry();
		}
	}
}
