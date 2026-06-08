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
}

bool AInteractionZoneBase::CheckAndBuildPayload_Implementation(AActor* OtherActor, FInteractionPayload& OutPayload)
{
	return false;
}

// RepNotify: 서버가 데이터를 넣고 스폰하면 클라이언트에서 자동 실행됨
void AInteractionZoneBase::OnRep_ZoneData()
{
	if (CollisionBox)
	{
		CollisionBox->SetBoxExtent(ZoneData.BoxExtent);
	}

	if (HighlightMeshComp && !ZoneData.HighlightMesh.IsNull())
	{
		// Soft Pointer 동기 로드 적용 (하이라이트 메시 정도는 가벼워서 Synchronous 로드도 무방함)
		UStaticMesh* LoadedMesh = ZoneData.HighlightMesh.LoadSynchronous();
		HighlightMeshComp->SetStaticMesh(LoadedMesh);
	}
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
	if (OtherActor->GetClass()->ImplementsInterface(UInteractableTagInterface::StaticClass()))
	{
		// 2차 검사: 인터페이스 함수를 안전하게 호출하여 고유 ID 태그 획득
		FGameplayTag ActorUniqueID = IInteractableTagInterface::Execute_GetUniqueIDTag(OtherActor);

		// 필터 지정 변수가 채워져 있을 때만 태그 대조 필터링 수행 (비어있으면 예외 없이 통과)
		if (!ZoneData.FilterTags.IsEmpty())
		{
			// 컨테이너 내부에 해당 도구의 고유 ID가 일치하는지 검사
			// 하위 태그도 확인하기 위해 HasTag 사용 (ex.Object.Hand.L)
			if (!ZoneData.FilterTags.HasTag(ActorUniqueID))
			{
				// 일치하지 않는 엉뚱한 도구이므로 C++ 단에서 즉시 가드 필터 탈락 처리
				return false;
			}
		}
		return true; // 필터를 무사히 통과함
	}

	return false; // 인터페이스조차 없는 일반 액터는 원천 차단
}