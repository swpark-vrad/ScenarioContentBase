#include "Actor/ScenarioPhaseBase.h"
#include "Actor/ScenarioEntryBase.h"
#include "Actor/InteractionZoneBase.h"
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

	if (AGameStateBase* GS = GetWorld()->GetGameState())
	{
		PhaseStartTime = GS->GetServerWorldTimeSeconds();
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
void AScenarioPhaseBase::ReceiveEndPhase_Implementation(bool bSuccess) {}

void AScenarioPhaseBase::OnPhaseTimeout()
{
	if (!HasAuthority() || bIsPhaseCompleted) return;
	EndPhase(false);
}

void AScenarioPhaseBase::HandleEntryCompleted(AScenarioEntryBase* CompletedEntry)
{
	if (!HasAuthority() || bIsPhaseCompleted) return;
	CheckPhaseCompletion();
}

void AScenarioPhaseBase::CheckPhaseCompletion()
{
	if (!HasAuthority() || bIsPhaseCompleted) return;

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
		EndPhase(true);
	}
}

void AScenarioPhaseBase::EndPhase(bool bSuccess)
{
	if (bIsPhaseCompleted) return;

	bIsPhaseCompleted = true;
	bIsPhaseSuccess = bSuccess;

	GetWorldTimerManager().ClearTimer(PhaseTimerHandle);

	// 1. 아직 진행 중이던 모든 엔트리들을 강제 비활성화(체크 중지)
	for (AScenarioEntryBase* Entry : ActiveEntries)
	{
		if (Entry && Entry->bIsActive)
		{
			Entry->EndEntry();
		}
	}

	// 2. 블루프린트 종료 이벤트 호출 (액터 정리, UI 연출 등)
	ReceiveEndPhase(bIsPhaseSuccess);

	OnRep_IsPhaseCompleted();
	OnPhaseCompleted.Broadcast(this, bIsPhaseSuccess);
}