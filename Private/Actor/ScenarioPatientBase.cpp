#include "Actor/ScenarioPatientBase.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"

AScenarioPatientBase::AScenarioPatientBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // 멀티플레이 환경을 위해 필수 활성화

	// 몸통 메시를 루트로 설정
	TorsoMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TorsoMesh"));
	SetRootComponent(TorsoMesh);

	// 나머지 머리, 팔, 다리 부위들은 루트가 된 TorsoMesh 하위에 그대로 부착합니다.
	HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(TorsoMesh);

	LeftArmMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LeftArmMesh"));
	LeftArmMesh->SetupAttachment(TorsoMesh);

	RightArmMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RightArmMesh"));
	RightArmMesh->SetupAttachment(TorsoMesh);

	LeftLegMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LeftLegMesh"));
	LeftLegMesh->SetupAttachment(TorsoMesh);

	RightLegMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RightLegMesh"));
	RightLegMesh->SetupAttachment(TorsoMesh);
}

// 에디터 편집, 레벨 배치, 블루프린트 오픈 시점에 실행되는 뷰포트 동기화 본문입니다.
void AScenarioPatientBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 컴포넌트 에셋 적재가 끝난 시점이므로 에디터 프리뷰 환경에서도 즉각 리더 포즈 추종이 활성화됩니다.
	if (TorsoMesh)
	{
		if (HeadMesh)
		{
			HeadMesh->SetLeaderPoseComponent(TorsoMesh);
		}
		if (LeftArmMesh)
		{
			LeftArmMesh->SetLeaderPoseComponent(TorsoMesh);
		}
		if (RightArmMesh)
		{
			RightArmMesh->SetLeaderPoseComponent(TorsoMesh);
		}
		if (LeftLegMesh)
		{
			LeftLegMesh->SetLeaderPoseComponent(TorsoMesh);
		}
		if (RightLegMesh)
		{
			RightLegMesh->SetLeaderPoseComponent(TorsoMesh);
		}

		UE_LOG(LogTemp, Log, TEXT("ScenarioPatientBase: Editor viewport modular pose synchronization established via OnConstruction."));
	}
}

void AScenarioPatientBase::BeginPlay()
{
	Super::BeginPlay();

}

void AScenarioPatientBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 실습 중 변경될 누적 처치 내역 태그들을 멀티플레이 네트워크 복제 목록에 가동
	DOREPLIFETIME(AScenarioPatientBase, AppliedTreatments);
}

void AScenarioPatientBase::RequestSetMorphTarget(EPatientMeshType MeshType, FName MorphTargetName, float Value)
{
	// 이 함수를 호출한 주체가 서버(호스트 방장) 권한을 쥐고 있다면 즉시 멀티캐스트 전파 개시
	if (HasAuthority())
	{
		Multicast_SetMorphTarget(MeshType, MorphTargetName, Value);
	}
	else
	{
		// 렌더링 소유권이 없는 원격 클라이언트 VR 기기에서 시도한 경우 안전하게 서버 RPC 우회로 개방
		Server_SetMorphTarget(MeshType, MorphTargetName, Value);
	}
}

void AScenarioPatientBase::Server_SetMorphTarget_Implementation(EPatientMeshType MeshType, FName MorphTargetName, float Value)
{
	// 서버단 검증 통과 후 전체 접속자 PC로 실시간 외형 변형 방송 가동
	Multicast_SetMorphTarget(MeshType, MorphTargetName, Value);
}

void AScenarioPatientBase::Multicast_SetMorphTarget_Implementation(EPatientMeshType MeshType, FName MorphTargetName, float Value)
{
	// 이 내부 연산은 호스트PC 및 원격 클라이언트 컴퓨터들의 로컬 뷰포트 스레드에서 실시간 동시 처리됨
	if (USkeletalMeshComponent* TargetMesh = GetMeshComponentByType(MeshType))
	{
		TargetMesh->SetMorphTarget(MorphTargetName, Value);
		UE_LOG(LogTemp, Log, TEXT("Patient: [%d]번 파트 메시의 모프타겟 '%s' 수치가 %.2f 로 전체 동기화되었습니다."),
			static_cast<int32>(MeshType), *MorphTargetName.ToString(), Value);
	}
}

USkeletalMeshComponent* AScenarioPatientBase::GetMeshComponentByType(EPatientMeshType MeshType) const
{
	switch (MeshType)
	{
	case EPatientMeshType::Torso:    return TorsoMesh;
	case EPatientMeshType::Head:     return HeadMesh;
	case EPatientMeshType::LeftArm:  return LeftArmMesh;
	case EPatientMeshType::RightArm: return RightArmMesh;
	case EPatientMeshType::LeftLeg:  return LeftLegMesh;
	case EPatientMeshType::RightLeg: return RightLegMesh;
	default: return nullptr;
	}
}

void AScenarioPatientBase::ApplyTreatment(FGameplayTag TreatmentTag, const FTreatmentAdditionalOptions& AdditionalOptions)
{
	// 데이터 오염 및 중복 패킷 변조를 막기 위해 철저히 서버 권한을 가졌을 때에만 실무 데이터 갱신을 허용
	if (!HasAuthority() || !TreatmentTag.IsValid()) return;

	// 1. 중복 처치 히스토리 스택 누적 방지 (기획 의도에 따라 중복 태그 누적이 필요한 경우 판정 제거 가능)
	if (!AppliedTreatments.HasTagExact(TreatmentTag))
	{
		AppliedTreatments.AddTag(TreatmentTag);
	}

	// 2. 새로운 술기 적용이 전파 완료 되었으므로, 환자를 관찰 중인 호스트 화면 및 서버 리스너 위젯을 갱신하도록 방송
	if (OnTreatmentApplied.IsBound())
	{
		OnTreatmentApplied.Broadcast(TreatmentTag, AdditionalOptions);
	}

	UE_LOG(LogTemp, Log, TEXT("Patient: 환자 본체에 새로운 처치 상태 데이터가 영구 적재되었습니다. (태그 명칭: %s)"), *TreatmentTag.ToString());
}