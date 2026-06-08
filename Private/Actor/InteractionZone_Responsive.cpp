#include "Actor/InteractionZone_Responsive.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Global/ScenarioGameplayTags.h" // 프로젝트 네이티브 태그 헤더 인클루드

AInteractionZone_Responsive::AInteractionZone_Responsive()
{
	TrackInterval = 0.1f; // VR 퍼포먼스 드롭이 없는 10Hz 분석 주기로 안전 장전
	ActiveTrackingPlayer = nullptr;
	CurrentPlayerOverlapCount = 0;
}

void AInteractionZone_Responsive::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && CollisionBox)
	{
		// 반응형 감시 사양에 맞춰 오버랩 진입/이탈 이벤트를 쌍으로 동적 맵핑
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AInteractionZone_Responsive::OnBoxOverlapBegin);
		CollisionBox->OnComponentEndOverlap.AddDynamic(this, &AInteractionZone_Responsive::OnBoxOverlapEnd);
	}
}

void AInteractionZone_Responsive::OnBoxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	// 유효성 검사
	if (!IsValidInteractableActor(OtherActor)) return;

	// 진입한 도구의 실질적 주인 유저 식별
	APlayerState* EnteringPlayer = GetPlayerStateFromActor(OtherActor);
	if (!EnteringPlayer) return;

	// [선점형 소유권 락 연산]
	if (ActiveTrackingPlayer == nullptr)
	{
		// 구역이 비어있으므로 첫 진입 유저가 소유권 선점 확보 락 작동
		ActiveTrackingPlayer = EnteringPlayer;
		CurrentPlayerOverlapCount = 1;
		PreviousInputTags.Reset(); // 이전 버퍼를 비워 진입 즉시 누르고 있는 상태인 경우 Pressed 유도

		// 정밀 인풋 추적 타이머 시동
		GetWorldTimerManager().SetTimer(ResponsiveTrackTimerHandle, this, &AInteractionZone_Responsive::OnResponsiveTrackTick, TrackInterval, true);
		UE_LOG(LogTemp, Log, TEXT("Zone_Responsive: 유저 [%s]가 구역 소유권을 선점 점유했습니다."), *ActiveTrackingPlayer->GetPlayerName());
	}
	else if (ActiveTrackingPlayer == EnteringPlayer)
	{
		// 동일 유저가 다른 손(양손 교체) 또는 가위 등의 다중 도구를 추가로 넣은 경우 락을 유지하고 카운트만 증가
		CurrentPlayerOverlapCount++;
	}
	else
	{
		// [타인 간섭 예외처리] 다른 제3의 유저가 침범한 경우 연산을 철저하게 무시하여 방어
		UE_LOG(LogTemp, Warning, TEXT("Zone_Responsive: [선점 차단] 유저 [%s]가 작업 중이므로 타인 [%s]의 접근을 무시합니다."),
			*ActiveTrackingPlayer->GetPlayerName(), *EnteringPlayer->GetPlayerName());
	}
}

void AInteractionZone_Responsive::OnBoxOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority() || !OtherActor) return;

	APlayerState* LeavingPlayer = GetPlayerStateFromActor(OtherActor);
	// 타인이 들어왔다가 손을 빼는 행위는 현재 가동 중인 소유권 상태에 영향이 없으므로 안전하게 스킵합니다.
	if (!LeavingPlayer || LeavingPlayer != ActiveTrackingPlayer) return;

	// 정당한 권한을 쥐고 있던 선점 유저의 도구 한 개가 탈출했으므로 카운터 차감
	CurrentPlayerOverlapCount--;

	// 선점 유저의 모든 손과 도구가 구역 레이어 바깥으로 완전히 빠져나간 최종 이탈 완결 프레임
	if (CurrentPlayerOverlapCount <= 0)
	{
		// [예외 방지 강제 해제 조치] 유저가 버튼을 꾹 누른 상태에서 손을 바깥으로 빼버린 경우 강제 Released 분기 실행
		FInteractionPayload ForceReleasePayload;
		CheckAndBuildPayload(OtherActor, ForceReleasePayload);

		const FScenarioGameplayTags& GameplayTags = FScenarioGameplayTags::Get();

		if (PreviousInputTags.HasTagExact(GameplayTags.InputState_Grab))
		{
			OnGrabReleased.Broadcast(ForceReleasePayload);
		}
		if (PreviousInputTags.HasTagExact(GameplayTags.InputState_Trigger))
		{
			OnTriggerReleased.Broadcast(ForceReleasePayload);
		}

		// =================================================================
		// [소유권 승계(Handover) 알고리즘] 현재 구역 내부에 이미 진입해서 숨죽이고 대기 중인 타인이 있는지 검사
		// =================================================================
		TArray<AActor*> OverlappingActors;
		CollisionBox->GetOverlappingActors(OverlappingActors);

		APlayerState* NewOwner = nullptr;
		int32 NewOwnerCount = 0;

		for (AActor* Actor : OverlappingActors)
		{
			if (Actor && Actor != this)
			{
				APlayerState* OverlappedPS = GetPlayerStateFromActor(Actor);
				// 방금 나간 주인(유저 A)이 아닌 제3의 대기자가 존재하는지 식별
				if (OverlappedPS && OverlappedPS != ActiveTrackingPlayer)
				{
					FInteractionPayload DirectPayload;
					if (CheckAndBuildPayload(Actor, DirectPayload))
					{
						if (NewOwner == nullptr)
						{
							NewOwner = OverlappedPS;
						}
						// 해당 승계 대상 유저가 현재 구역에 걸치고 있는 총 도구/핸들 개수 합산 계산
						if (OverlappedPS == NewOwner)
						{
							NewOwnerCount++;
						}
					}
				}
			}
		}

		// 대기 중인 후임 유저가 발견된 경우 (소유권 실시간 이양)
		if (NewOwner != nullptr)
		{
			ActiveTrackingPlayer = NewOwner;
			CurrentPlayerOverlapCount = NewOwnerCount;
			PreviousInputTags.Reset(); // 이전 잔상 버퍼를 초기화하여 승계 유저의 Pressed 이벤트가 다음 주기 즉시 매끄럽게 발동되도록 유도

			UE_LOG(LogTemp, Log, TEXT("Zone_Responsive: 주권자 이탈에 따라 소유권이 대기 중이던 유저 [%s]에게 승계(Handover)되었습니다. (잔여 도구 개수: %d)"),
				*ActiveTrackingPlayer->GetPlayerName(), CurrentPlayerOverlapCount);
		}
		else
		{
			// 구역 내에 진짜 아무도 남지 않은 경우에만 비로소 타이머를 완전히 끄고 락 해제 단행
			GetWorldTimerManager().ClearTimer(ResponsiveTrackTimerHandle);

			ActiveTrackingPlayer = nullptr;
			CurrentPlayerOverlapCount = 0;
			PreviousInputTags.Reset();
		}
	}
}

void AInteractionZone_Responsive::OnResponsiveTrackTick()
{
	if (!HasAuthority() || !ActiveTrackingPlayer)
	{
		GetWorldTimerManager().ClearTimer(ResponsiveTrackTimerHandle);
		return;
	}

	// 구역 박스 볼륨 내부에 현재 오버랩되어 걸려있는 동적 액터 목록 인출
	TArray<AActor*> OverlappingActors;
	CollisionBox->GetOverlappingActors(OverlappingActors);

	AActor* ValidTrackingActor = nullptr;
	FInteractionPayload CurrentPayload;

	// 겹쳐진 액터 더미 중, 정당한 권한을 가진 유저의 유효 도구 액터 하나를 추적 인출
	for (AActor* Actor : OverlappingActors)
	{
		if (Actor && GetPlayerStateFromActor(Actor) == ActiveTrackingPlayer)
		{
			// 블루프린트단 쿼리 함수를 찔러 현재 프레임의 실시간 인풋 태그 적재 상태(Payload)를 동적 인출
			if (CheckAndBuildPayload(Actor, CurrentPayload))
			{
				ValidTrackingActor = Actor;
				break;
			}
		}
	}

	// 영역 내에 있으나 유효 데이터를 검출해 내지 못한 프레임은 예외 통과
	if (!ValidTrackingActor) return;

	// 사전 정의된 글로벌 네이티브 태그 클래스 인스턴스 참조 인출
	const FScenarioGameplayTags& GameplayTags = FScenarioGameplayTags::Get();

	// 블루프린트 프레임워크 구조가 Payload.InteractionTag 또는 데이터 딕셔너리에 실어 보낸 실시간 버튼 인풋 적재 판단
	bool bIsGrabbing = CurrentPayload.InteractionTags.HasTag(GameplayTags.InputState_Grab) || CurrentPayload.AdditionalData.Contains(GameplayTags.InputState_Grab);
	bool bIsTriggering = CurrentPayload.InteractionTags.HasTag(GameplayTags.InputState_Trigger) || CurrentPayload.AdditionalData.Contains(GameplayTags.InputState_Trigger);

	// =================================================================
	// 상태 천이(Edge Detection) 알고리즘을 통한 6대 반응형 순간 정밀 분기
	// =================================================================

	// [그랩 제어 분기]
	if (!PreviousInputTags.HasTagExact(GameplayTags.InputState_Grab) && bIsGrabbing)
	{
		OnGrabPressed.Broadcast(CurrentPayload);
	}
	else if (PreviousInputTags.HasTagExact(GameplayTags.InputState_Grab) && bIsGrabbing)
	{
		OnGrabHeld.Broadcast(CurrentPayload);
	}
	else if (PreviousInputTags.HasTagExact(GameplayTags.InputState_Grab) && !bIsGrabbing)
	{
		OnGrabReleased.Broadcast(CurrentPayload);
	}

	// [트리거 제어 분기]
	if (!PreviousInputTags.HasTagExact(GameplayTags.InputState_Trigger) && bIsTriggering)
	{
		OnTriggerPressed.Broadcast(CurrentPayload);
	}
	else if (PreviousInputTags.HasTagExact(GameplayTags.InputState_Trigger) && bIsTriggering)
	{
		OnTriggerHeld.Broadcast(CurrentPayload);
	}
	else if (PreviousInputTags.HasTagExact(GameplayTags.InputState_Trigger) && !bIsTriggering)
	{
		OnTriggerReleased.Broadcast(CurrentPayload);
	}

	// 이번 주기의 대조 판정이 완결되었으므로 다음 프레임 비교를 위해 이전 버퍼 데이터를 교체 백업합니다.
	PreviousInputTags.Reset();
	if (bIsGrabbing) PreviousInputTags.AddTag(GameplayTags.InputState_Grab);
	if (bIsTriggering) PreviousInputTags.AddTag(GameplayTags.InputState_Trigger);
}

APlayerState* AInteractionZone_Responsive::GetPlayerStateFromActor(AActor* InActor) const
{
	if (!InActor) return nullptr;

	// 오너십 체인을 끝까지 상향 추적하여 Pawn 또는 Controller로부터 최상위 고유 PlayerState 주소를 안전 인출
	AActor* CurrentOwner = InActor;
	while (CurrentOwner)
	{
		if (APawn* Pawn = Cast<APawn>(CurrentOwner))
		{
			return Pawn->GetPlayerState();
		}
		if (AController* Controller = Cast<AController>(CurrentOwner))
		{
			return Controller->PlayerState;
		}
		CurrentOwner = CurrentOwner->GetOwner();
	}
	return nullptr;
}

void AInteractionZone_Responsive::BroadcastInteractionTriggered(const FInteractionPayload& Payload)
{
	Super::BroadcastInteractionTriggered(Payload);

	if (ZoneData.bIsSingleUse && CollisionBox)
	{
		// 콜리전을 완전히 꺼버림으로써 엔진 내부 오버랩 바인딩 목록에서 소멸시킵니다. (두 번 다시 체크 안 함)
		DeactivateAndShutdown();

		UE_LOG(LogTemp, Log, TEXT("Zone_Instant: [One-Shot 완결] 일회성 구역 미션이 완수되어 물리 콜리전 및 바인딩이 영구 비활성화되었습니다."));
	}

}

void AInteractionZone_Responsive::DeactivateAndShutdown()
{
	if (!HasAuthority()) return;

	PreviousInputTags.Reset();

	if (CollisionBox)
	{
		// 델리게이트를 먼저 해제하므로, 아래에서 콜리전이 꺼질 때 
		// 물리 엔진이 EndOverlap 신호를 보내도 OnBoxOverlapEnd 함수가 아예 실행되지 않습니다.
		CollisionBox->OnComponentBeginOverlap.RemoveDynamic(this, &AInteractionZone_Responsive::OnBoxOverlapBegin);
		CollisionBox->OnComponentEndOverlap.RemoveDynamic(this, &AInteractionZone_Responsive::OnBoxOverlapEnd);
	}

	// ----------------------------------------------------------------------
	// 이제 안전하게 부모를 호출하여 콜리전 및 메쉬를 비활성화합니다.
	// ----------------------------------------------------------------------
	Super::DeactivateAndShutdown();

	// 2. 나머지 고유 타이머 및 자원 정리
	GetWorldTimerManager().ClearTimer(ResponsiveTrackTimerHandle);

	ActiveTrackingPlayer = nullptr;
	CurrentPlayerOverlapCount = 0;

	UE_LOG(LogTemp, Log, TEXT("Zone_Responsive: [Responsive 오버라이드 완결] 반응형 고유 자원 정리를 완료했습니다."));
}
