#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interface/InteractableTagInterface.h"
#include "Data/ScenarioDataTypes.h"
#include "ScenarioPatientBase.generated.h"

// 6개의 모듈러 메시 컴포넌트를 명확하게 가리키기 위한 가독성용 열거형
UENUM(BlueprintType)
enum class EPatientMeshType : uint8
{
	Torso       UMETA(DisplayName = "Torso"),
	Head        UMETA(DisplayName = "Head"),
	LeftArm     UMETA(DisplayName = "Left Arm"),
	RightArm    UMETA(DisplayName = "Right Arm"),
	LeftLeg     UMETA(DisplayName = "Left Leg"),
	RightLeg    UMETA(DisplayName = "Right Leg")
};

// 델리게이트 파라미터 전달을 위한 약물명, 용량 등의 추가 옵션 데이터 구조체 wrapper
USTRUCT(BlueprintType)
struct FTreatmentAdditionalOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Treatment")
	TMap<FGameplayTag, FString> OptionsMap;
};

// 처치가 완료되었을 때 호스트/클라 UI 및 연출 엔진이 수신할 블루프린트 개방형 전역 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTreatmentAppliedSignature);

// 바이탈 사인 변경시 호출될 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVitalSignChangedSignature, const FVitalSign&, NewVitalSign);

UCLASS()
class SCENARIOCONTENT_API AScenarioPatientBase : public AActor, public IInteractableTagInterface
{
	GENERATED_BODY()

public:
	AScenarioPatientBase();

	// ======================================================================
	// 요구사항 1. 6개의 독립형 스켈레탈 메시 컴포넌트 인프라 구축
	// ======================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patient|Components")
	USceneComponent* RootSceneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patient|Components")
	USkeletalMeshComponent* TorsoMesh; // 몸통 (메인 부모 바디 mesh)

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patient|Components")
	USkeletalMeshComponent* HeadMesh;  // 머리

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patient|Components")
	USkeletalMeshComponent* LeftArmMesh; // 왼팔

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patient|Components")
	USkeletalMeshComponent* RightArmMesh; // 오른팔

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patient|Components")
	USkeletalMeshComponent* LeftLegMesh; // 왼다리

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Patient|Components")
	USkeletalMeshComponent* RightLegMesh; // 오른다리

	// ======================================================================
	// 요구사항 2. 호스트 및 클라이언트 전체 전파용 모프 타겟 RPC 시스템
	// ======================================================================
	/** 외부(도구/컨트롤러)에서 환자의 모프 타겟 조작을 요청할 때 가동하는 런타임 입구 함수 */
	UPROPERTY(ReplicatedUsing = OnRep_InitialPartState, BlueprintReadOnly, Category = "Patient|Treatment")
	FPatientPartState InitialPartState;

	UFUNCTION()
	void OnRep_InitialPartState();

	void InitializePartState(FPatientPartState PartState);

	UFUNCTION(BlueprintCallable, Category = "Patient|Visuals")
	void RequestSetMorphTarget(EPatientMeshType MeshType, FName MorphTargetName, float Value);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Patient|Lifecycle")
	void ActivatePatient();
	virtual void ActivatePatient_Implementation();

	/** 환자 본인에게 영구 누적 적용된 처치(술기) 태그 컨테이너 (레이트 조이너 유저도 리플리케이션 자동 동기화) */
	UPROPERTY(ReplicatedUsing = OnRep_AppliedTreatments, BlueprintReadOnly, Category = "Patient|Treatment")
	FGameplayTagContainer AppliedTreatments;

	UFUNCTION()
	void OnRep_AppliedTreatments();

	UPROPERTY(BlueprintAssignable, Category = "Patient|Events")
	FOnTreatmentAppliedSignature OnTreatmentApplied;

	/** 서버 권한(Authority) 단에서 처치 성공 정보를 주입하고 전역 알림 이벤트를 가동시키는 함수 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Patient|Treatment")
	void ApplyTreatment(FGameplayTag TreatmentTag, const FTreatmentAdditionalOptions& AdditionalOptions);

	// 처치시 비주얼메시 추가
	UFUNCTION(BlueprintCallable, Category = "Patient|Visual")
	bool AddTreatmentVisuals(FGameplayTag VisualID, UStaticMeshComponent*& OutMeshComp);

	UFUNCTION(BlueprintPure, Category = "Patient|Treatment")
	bool CheckTreatment(FGameplayTag TreatmentTag);

	// ======================================================================
	// VitalSign
	// ======================================================================
	UPROPERTY(BlueprintAssignable, Category = "Patient|Events")
	FOnVitalSignChangedSignature OnVitalSignChanged;

	/** 통짜 바이탈사인 정보 직접 수정 함수 (서버 권한 전용) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Patient|Vital")
	void SetVitalSign(const FVitalSign& NewVitalSign);

	/** 특정 항목 수정: 심박수 변경 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Patient|Vital")
	void SetHeartRate(int32 NewHR);

	/** 특정 항목 수정: 호흡수 변경 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Patient|Vital")
	void SetRespiratoryRate(int32 NewRR);

	/** 특정 항목 수정: 산소포화도 변경 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Patient|Vital")
	void SetSPO2(int32 NewSPO2);

	/** 특정 항목 수정: 혈압 정보 변경 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Patient|Vital")
	void SetBloodPressure(int32 NewMinBP, int32 NewMaxBP, bool bIsABP);

	/** 특정 항목 수정: 체온 변경 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Patient|Vital")
	void SetBodyTemperature(float NewBT);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// 블루프린트 에디터 뷰포트에서 실시간으로 포즈 동기화를 처리하기 위해 OnConstruction을 추가합니다.
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual FGameplayTag GetUniqueIDTag_Implementation() const override;
	virtual FGameplayTagContainer GetStateTags_Implementation() const override;

	/** 원격 클라이언트 기기에서 다이렉트로 호출을 시도했을 때 소유권 예외 처리용 서버 RPC */
	UFUNCTION(Server, Reliable)
	void Server_SetMorphTarget(EPatientMeshType MeshType, FName MorphTargetName, float Value);

	/** 서버 권한 확인 후 모든 소켓 커넥션(호스트+원격 클라)으로 패킷을 동시 방송하는 멀티캐스트 RPC */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetMorphTarget(EPatientMeshType MeshType, FName MorphTargetName, float Value);

	UFUNCTION(BlueprintCallable)
	void Local_SetMorphTarget(EPatientMeshType MeshType, FName MorphTargetName, float Value);

	// 레이트 조이너 및 데이터 변동 시 현재 태그 컨테이너를 기반으로 외형을 강제 동기화하는 핵심 함수입니다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Patient|Visuals")
	void RefreshPatientVisuals();
	virtual void RefreshPatientVisuals_Implementation();

	UPROPERTY()
	TMap<FGameplayTag, UStaticMeshComponent*> SpawnedVisualComponents;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Patient|Treatment")
	void ApplyInitialPartState(const FPatientPartState& PartState);

	UPROPERTY(ReplicatedUsing = OnRep_VitalSign, BlueprintReadOnly, Category = "Patient|Vital")
	FVitalSign VitalSign;

	UFUNCTION()
	void OnRep_VitalSign();

	// UI에서 참조할 랜덤값 적용 VitalSign
	UPROPERTY(ReplicatedUsing = OnRep_DisplayVitalSign, BlueprintReadOnly, Category = "Patient|Vital")
	FVitalSign DisplayVitalSign;

	UFUNCTION()
	void OnRep_DisplayVitalSign();


	void RefreshRandomValue();

	/** 서버에서 주기적으로 랜덤 보정값을 갱신하는 함수 */
	void UpdateDisplayVitalSigns();

	/** 바이탈사인 변동 타이머 핸들 */
	FTimerHandle DisplayVitalTimerHandle;

private:
	/** 내부 헬퍼: 열거형 분기를 통해 타겟팅된 메시 컴포넌트의 주소 포인터를 반환 */
	USkeletalMeshComponent* GetMeshComponentByType(EPatientMeshType MeshType) const;

	// VitalSign 랜덤값
	int32 RandomHR;
	int32 RandomRR;
	int32 RandomSPO2;

};