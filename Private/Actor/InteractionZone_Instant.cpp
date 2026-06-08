#include "Actor/InteractionZone_Instant.h"
#include "Components/BoxComponent.h"

AInteractionZone_Instant::AInteractionZone_Instant()
{
	CooldownDuration = 0.5f;
	LastTriggerTime = -10.0f; // 초기 상태에서 즉시 실행 가능하도록 여유 값으로 장전
}

void AInteractionZone_Instant::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && CollisionBox)
	{
		// 즉시 판정형에 맞는 단일 진입 이벤트만 동적 바인딩 구성
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AInteractionZone_Instant::OnBoxOverlap);
	}
}

void AInteractionZone_Instant::OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	// C++ 네이티브 초고속 인터페이스 및 필터링 검사 (불합격 시 블루프린트 VM을 깨우지 않고 탈출)
	if (!IsValidInteractableActor(OtherActor)) return;

	// 쿨타임 검사
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < LastTriggerTime + CooldownDuration) return;

	FInteractionPayload AssembledPayload;
	// 최종 블루프린트 심사단 통과
	if (CheckAndBuildPayload(OtherActor, AssembledPayload))
	{
		LastTriggerTime = CurrentTime;

		// 공식 전역 엔트리 타워로 상호작용 성공 패킷 발송
		BroadcastInteractionTriggered(AssembledPayload);

		// 진짜 일회성 전용 엔트리라면 충돌 처리 직후 물리엔진 레벨에서 차단
		if (ZoneData.bIsOneShot && CollisionBox)
		{
			// 콜리전을 완전히 꺼버림으로써 엔진 내부 오버랩 바인딩 목록에서 소멸시킵니다. (두 번 다시 체크 안 함)
			CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CollisionBox->OnComponentBeginOverlap.RemoveDynamic(this, &AInteractionZone_Instant::OnBoxOverlap);

			UE_LOG(LogTemp, Log, TEXT("Zone_Instant: [One-Shot 완결] 일회성 구역 미션이 완수되어 물리 콜리전 및 바인딩이 영구 비활성화되었습니다."));
		}
	}
}