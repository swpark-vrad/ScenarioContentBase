#include "Actor/ScenarioPatientBase.h"
#include "Global/ScenarioGameStateBase.h"
#include "Global/ScenarioGameplayTags.h"
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

	RefreshPatientVisuals();

}

void AScenarioPatientBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 실습 중 변경될 누적 처치 내역 태그들을 멀티플레이 네트워크 복제 목록에 가동
	DOREPLIFETIME(AScenarioPatientBase, AppliedTreatments); 
	DOREPLIFETIME(AScenarioPatientBase, InitialPartState);

	DOREPLIFETIME(AScenarioPatientBase, VitalSign);
	DOREPLIFETIME(AScenarioPatientBase, DisplayVitalSign);
}

FGameplayTag AScenarioPatientBase::GetUniqueIDTag_Implementation() const
{
	const FScenarioGameplayTags& GameplayTags = FScenarioGameplayTags::Get();
	return GameplayTags.Object_Patient;
}

FGameplayTagContainer AScenarioPatientBase::GetStateTags_Implementation() const
{
	// 환자에게 누적된 처치 태그 컨테이너를 구역(IZ)에 그대로 반환합니다.
	return AppliedTreatments;
}

void AScenarioPatientBase::InitializePartState(FPatientPartState PartState)
{
	if (!HasAuthority()) return;

	// 서버 권한 단에서 부상 상태 구조체를 원본에 적재합니다.
	InitialPartState = PartState;

	// 서버(호스트) 플레이어 화면의 비주얼 조립을 위해 즉시 실행합니다.
	ApplyInitialPartState(InitialPartState);
}

void AScenarioPatientBase::OnRep_InitialPartState()
{
	ApplyInitialPartState(InitialPartState);
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

bool AScenarioPatientBase::AddTreatmentVisuals(FGameplayTag VisualID, UStaticMeshComponent*& OutMeshComp)
{
	// 초기화 작업을 통해 실패 시 안전하게 nullptr을 가지도록 방어합니다.
	OutMeshComp = nullptr;

	if (!VisualID.IsValid() || SpawnedVisualComponents.Contains(VisualID)) return false;

	AScenarioGameStateBase* GS = GetWorld()->GetGameState<AScenarioGameStateBase>();
	if (!GS) return false;

	FTreatmentVisuals VisualData;
	if (GS->GetTreatmentVisualData(VisualID, VisualData))
	{
		UStaticMesh* LoadedMesh = VisualData.VisualMesh.LoadSynchronous();
		USkeletalMeshComponent* BaseSkelMesh = FindComponentByClass<USkeletalMeshComponent>();

		if (LoadedMesh && BaseSkelMesh)
		{
			UStaticMeshComponent* NewMeshComp = NewObject<UStaticMeshComponent>(this);
			if (NewMeshComp)
			{
				NewMeshComp->SetStaticMesh(LoadedMesh);
				NewMeshComp->RegisterComponent();

				FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
				NewMeshComp->AttachToComponent(BaseSkelMesh, AttachRules, VisualData.TargetSocketName);
				NewMeshComp->SetRelativeTransform(VisualData.RelativeOffset);

				NewMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

				SpawnedVisualComponents.Add(VisualID, NewMeshComp);

				// 생성된 컴포넌트 주소를 출력 파라미터에 할당하고 최종 성공을 리턴합니다.
				OutMeshComp = NewMeshComp;
				return true;
			}
		}
	}

	return false;
}

bool AScenarioPatientBase::CheckTreatment(FGameplayTag TreatmentTag)
{
	// 상위 태그도 체크할 수 있도록 HasTag 사용
	return AppliedTreatments.HasTag(TreatmentTag);
}

void AScenarioPatientBase::Server_SetMorphTarget_Implementation(EPatientMeshType MeshType, FName MorphTargetName, float Value)
{
	// 서버단 검증 통과 후 전체 접속자 PC로 실시간 외형 변형 방송 가동
	Multicast_SetMorphTarget(MeshType, MorphTargetName, Value);
}

void AScenarioPatientBase::Multicast_SetMorphTarget_Implementation(EPatientMeshType MeshType, FName MorphTargetName, float Value)
{
	Local_SetMorphTarget(MeshType,MorphTargetName,Value);
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

void AScenarioPatientBase::ActivatePatient_Implementation()
{
	// 모든 클라이언트 및 서버 공통: C++ 기본 활성화 로그 및 디폴트 처리
	UE_LOG(LogTemp, Log, TEXT("ScenarioPatientBase: Patient [%s] is now officially activated."), *GetName());

	// 서버에서만 1초에 한번씩 바이탈사인 랜덤값 계산
	if (HasAuthority())
	{
		// 초기 값 장전
		UpdateDisplayVitalSigns();
		GetWorldTimerManager().SetTimer(DisplayVitalTimerHandle, this, &AScenarioPatientBase::RefreshRandomValue, 1.0f, true);
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

	OnRep_AppliedTreatments();

	UE_LOG(LogTemp, Log, TEXT("Patient: 환자 본체에 새로운 처치 상태 데이터가 영구 적재되었습니다. (태그 명칭: %s)"), *TreatmentTag.ToString());
}

void AScenarioPatientBase::OnRep_AppliedTreatments()
{
	// 1. 전달받은 최신 태그 상태를 기반으로 모든 클라이언트의 환자 메쉬/모프 외형을 전수 갱신합니다.
	RefreshPatientVisuals();

	if (OnTreatmentApplied.IsBound())
	{
		OnTreatmentApplied.Broadcast();
	}

}

void AScenarioPatientBase::RefreshPatientVisuals_Implementation()
{
	// 부모 단에서는 공통 처리할 디폴트 에셋 로직이 없으므로 비워둡니다.
}

void AScenarioPatientBase::Local_SetMorphTarget(EPatientMeshType MeshType, FName MorphTargetName, float Value)
{
	// 기존 내부 내장 헬퍼 함수를 통해 열거형에 해당하는 메시 컴포넌트 포인터를 안전하게 획득합니다.
	if (USkeletalMeshComponent* TargetMesh = GetMeshComponentByType(MeshType))
	{
		// RPC 패킷을 보내지 않고, 오직 이 함수가 실행 중인 현재 PC 화면의 모프 타겟만 변경합니다.
		TargetMesh->SetMorphTarget(MorphTargetName, Value);

		UE_LOG(LogTemp, Log, TEXT("Patient: 로컬 연출 처리 - [%d]번 파트 메시의 모프타겟 '%s' 수치가 %.2f 로 변경되었습니다."),
			static_cast<int32>(MeshType), *MorphTargetName.ToString(), Value);
	}
}

void AScenarioPatientBase::OnRep_VitalSign()
{
	// 복제 데이터가 수신되면 리스너(모니터 등)들에게 변경을 알립니다.
	if (OnVitalSignChanged.IsBound())
	{
		OnVitalSignChanged.Broadcast(VitalSign);
	}
}

void AScenarioPatientBase::OnRep_DisplayVitalSign()
{
	// 3. 서버에서 값이 바뀌어 패킷이 날아오면, 클라이언트 기기에서도 이 OnRep이 완벽하게 동시 작동합니다.
	if (OnVitalSignChanged.IsBound())
	{
		// 원본 데이터가 아닌, 난수가 더해져서 도달한 최신 디스플레이 구조체를 위젯 중계자(WA)에게 던집니다.
		OnVitalSignChanged.Broadcast(DisplayVitalSign);
	}
}

void AScenarioPatientBase::RefreshRandomValue()
{
	RandomHR = FMath::RandRange(-2, 2);
	RandomRR = FMath::RandRange(-2, 2);
	RandomSPO2 = FMath::RandRange(-2, 2);

	UpdateDisplayVitalSigns();
}

void AScenarioPatientBase::UpdateDisplayVitalSigns()
{
	if (!HasAuthority()) return;

	// 1. 기준치(VitalSign) 데이터를 기반으로 매초 새로운 난수를 더해 'DisplayVitalSign'의 값을 실제로 변조합니다.
	DisplayVitalSign.IsABP = VitalSign.IsABP;
	DisplayVitalSign.MinBP = VitalSign.MinBP;
	DisplayVitalSign.MaxBP = VitalSign.MaxBP;
	DisplayVitalSign.BT = VitalSign.BT;

	DisplayVitalSign.HR = VitalSign.HR + RandomHR;
	DisplayVitalSign.RR = VitalSign.RR + RandomRR;
	DisplayVitalSign.SPO2 = FMath::Clamp(VitalSign.SPO2 + RandomSPO2, 0, 100);

	// 2. 서버(호스트) 본인 화면의 위젯 동기화를 위해 수동 호출합니다.
	OnRep_DisplayVitalSign();
}

void AScenarioPatientBase::SetVitalSign(const FVitalSign& NewVitalSign)
{
	if (!HasAuthority()) return;

	VitalSign = NewVitalSign;

	// 마스터 값이 바뀌면 즉시 디스플레이 값도 동기화하여 반응성을 높입니다.
	UpdateDisplayVitalSigns();
}

void AScenarioPatientBase::SetHeartRate(int32 NewHR)
{
	if (!HasAuthority()) return;

	VitalSign.HR = NewHR;
	UpdateDisplayVitalSigns();
}

void AScenarioPatientBase::SetRespiratoryRate(int32 NewRR)
{
	if (!HasAuthority()) return;

	VitalSign.RR = NewRR;
	UpdateDisplayVitalSigns();
}

void AScenarioPatientBase::SetSPO2(int32 NewSPO2)
{
	if (!HasAuthority()) return;

	VitalSign.SPO2 = NewSPO2;
	UpdateDisplayVitalSigns();
}

void AScenarioPatientBase::SetBloodPressure(int32 NewMinBP, int32 NewMaxBP, bool bIsABP)
{
	if (!HasAuthority()) return;

	VitalSign.MinBP = NewMinBP;
	VitalSign.MaxBP = NewMaxBP;
	VitalSign.IsABP = bIsABP;
	UpdateDisplayVitalSigns();
}

void AScenarioPatientBase::SetBodyTemperature(float NewBT)
{
	if (!HasAuthority()) return;

	VitalSign.BT = NewBT;
	UpdateDisplayVitalSigns();
}