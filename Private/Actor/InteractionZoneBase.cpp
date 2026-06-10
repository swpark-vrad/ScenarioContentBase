#include "Actor/InteractionZoneBase.h"
#include "Interface/InteractableTagInterface.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AInteractionZoneBase::AInteractionZoneBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // 멀티플레이 스폰 동기화를 위해 필수

	// 1. 콜리전 박스 세팅 (루트)
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	// 기본적으로 겹침(Overlap)만 허용
	CollisionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	// 2. 하이라이트 메시 세팅
	HighlightMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HighlightMeshComp"));
	HighlightMeshComp->SetupAttachment(CollisionBox);
	HighlightMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 메시는 충돌 연산 제외
	HighlightMeshComp->SetVisibility(false);	// 비저블 비활성화
}

void AInteractionZoneBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		OnRep_ZoneData();
	}
}

// 변수 리플리케이션 등록
void AInteractionZoneBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AInteractionZoneBase, ZoneData); 
	DOREPLIFETIME(AInteractionZoneBase, bIsShutdown);
}

void AInteractionZoneBase::DeactivateAndShutdown()
{
	if (!HasAuthority()) return;

	// 셧다운 상태 변수를 변경하여 접속 중이거나 늦게 들어올 모든 클라이언트에 패킷을 송신합니다.
	bIsShutdown = true;
	OnRep_bIsShutdown(); // 서버 호스트 본인의 로컬 환경에도 즉시 반영합니다.
}

void AInteractionZoneBase::SetPatientActor(AActor* InPatientActor)
{
	CachedPatientActor = InPatientActor;
}

void AInteractionZoneBase::OnRep_bIsShutdown()
{
	if (bIsShutdown)
	{
		// 컴포넌트의 콜리전을 완전히 비활성화하여 레이트 조이너의 오버랩을 차단합니다.
		if (CollisionBox)
		{
			CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CollisionBox->SetHiddenInGame(true);
		}

		// 하이라이트 메쉬의 가시성을 꺼서 시각적으로도 완벽히 정리합니다.
		if (HighlightMeshComp)
		{
			HighlightMeshComp->SetVisibility(false);
		}
	}
}

// RepNotify: 서버가 데이터를 넣고 스폰하면 클라이언트에서 자동 실행됨
void AInteractionZoneBase::OnRep_ZoneData()
{
	if (CollisionBox)
	{
		CollisionBox->SetBoxExtent(ZoneData.BoxExtent);
	}

}

bool AInteractionZoneBase::CheckAndBuildPayload_Implementation(AActor* OtherActor, FInteractionPayload& OutPayload)
{
	// 공통 필터 함수를 호출하여 가위나 도구 액터의 유효성을 1차 검사합니다.
	if (!IsValidInteractableActor(OtherActor)) return false;

	OutPayload.TargetActor = OtherActor;
	OutPayload.InteractionTags.Reset();

	// 도구 액터가 장착한 인터페이스를 통해 실시간 상태 태그 리스트를 수집합니다.
	if (OtherActor && OtherActor->Implements<UInteractableTagInterface>())
	{
		FGameplayTag IDTag = IInteractableTagInterface::Execute_GetUniqueIDTag(OtherActor);
		OutPayload.UniqueID = IDTag;

		FGameplayTagContainer ActorStateTags = IInteractableTagInterface::Execute_GetStateTags(OtherActor);
		OutPayload.InteractionTags.AppendTags(ActorStateTags);
	}

	// 구역 본인이 데이터 테이블로부터 주입받은 목적 태그를 함께 포장합니다.
	OutPayload.InteractionTags.AppendTags(ZoneData.TargetInteractionTags);

	return true;
}

void AInteractionZoneBase::BroadcastInteractionTriggered(const FInteractionPayload& Payload)
{
	if (OnInteractionTriggered.IsBound())
	{
		OnInteractionTriggered.Broadcast(Payload);
	}
}

bool AInteractionZoneBase::IsValidInteractableActor(AActor* OtherActor) const
{
	if (!OtherActor || OtherActor == this) return false;

	// 1차 검사: 들어온 액터가 상호작용 태그 인터페이스를 장착하고 있는지 확인
	if (OtherActor->Implements<UInteractableTagInterface>())
	{
		// 2차 검사: 인터페이스 함수로 참조한 ID 태그로 충돌 예외처리
		FGameplayTag ActorUniqueID = IInteractableTagInterface::Execute_GetUniqueIDTag(OtherActor);

		if (!ZoneData.CollisionFilterTags.IsEmpty() && !ActorUniqueID.MatchesAny(ZoneData.CollisionFilterTags))
		{
			return false;
		}

		// 3차 검사 : 사전 필요 처치가 있을 경우 필터링
		if (!ZoneData.RequiredStateTags.IsEmpty())
		{
			// 캐싱한 환자가 유효한지, 인터페이스를 구현했는지 확인
			if (CachedPatientActor != nullptr && CachedPatientActor->Implements<UInteractableTagInterface>())
			{
				// Execute_ 매크로는 대상이 네이티브든 블루프린트든 상관없이 인터페이스 함수를 최상위 속도로 강제 가동합니다.
				FGameplayTagContainer PatientTags = IInteractableTagInterface::Execute_GetStateTags(CachedPatientActor);

				if (!PatientTags.HasAll(ZoneData.RequiredStateTags))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	return false; // 인터페이스조차 없는 일반 액터는 원천 차단
}

void AInteractionZoneBase::Multicast_SetActivateHint_Implementation(bool bActivate)
{
	if (!HasAuthority()) return;

	if (!bIsShutdown)
	{
		HighlightMeshComp->SetVisibility(bActivate);
	}
}