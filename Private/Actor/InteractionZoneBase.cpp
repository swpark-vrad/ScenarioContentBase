#include "Actor/InteractionZoneBase.h"
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

	// 서버인 경우, 오버랩 이벤트 바인딩 및 본인 스스로 OnRep 함수 1회 호출하여 세팅 적용
	if (HasAuthority())
	{
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AInteractionZoneBase::OnBoxOverlap);
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

// 충돌 감지 로직 (서버에서만 실행됨)
void AInteractionZoneBase::OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 1. 방어 코드 및 서버 권한 확인 (C++의 통제 영역)
	if (!HasAuthority() || !OtherActor || OtherActor == this) return;

	FInteractionPayload AssembledPayload;

	// 2. 블루프린트에 로직 위임 (BP_IZ_프로젝트에서 구현한 노드가 실행됨)
	if (CheckAndBuildPayload(OtherActor, AssembledPayload))
	{
		// 3. 블루프린트가 성공(true)을 반환했다면, C++가 넘겨받아 후처리 진행
		OnInteractionTriggered.Broadcast(AssembledPayload);

		// 중복 충돌 방지를 위해 즉시 콜리전 끄기
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}