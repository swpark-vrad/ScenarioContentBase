#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Global/ScenarioGameplayTags.h"
#include "ScenarioDataTypes.generated.h"

UENUM(BlueprintType)
enum class EScenarioMode : uint8
{
	Practice    UMETA(DisplayName = "Practice Mode"),
	Evaluation  UMETA(DisplayName = "Evaluation Mode")
};


UENUM(BlueprintType)
enum class EZoneAnchorType : uint8
{
	Patient         UMETA(DisplayName = "Patient Body"),
	StaticWorld     UMETA(DisplayName = "Static World Space"),
	AttachedObject  UMETA(DisplayName = "Attached To Object")
};

// 환자의 신체 부위별 부상 상태를 나타내는 열거형
UENUM(BlueprintType)
enum class EBodyPartState : uint8
{
	Normal          UMETA(DisplayName = "Normal"),          // 정상
	Contusion       UMETA(DisplayName = "Contusion"),       // 타박상
	CloseFracture   UMETA(DisplayName = "Close Fracture"),  // 폐쇄성 골절
	OpenFracture    UMETA(DisplayName = "Open Fracture"),   // 개방성 골절
	Amputation      UMETA(DisplayName = "Amputation")       // 절단
};

// VS 변화 타입
UENUM(BlueprintType)
enum class EVitalModifierOp : uint8
{
	None     UMETA(DisplayName = "Keep Current (유지)"),
	Set      UMETA(DisplayName = "Set Absolute (절대값 고정)"),
	Add      UMETA(DisplayName = "Add/Subtract (상대값 증감)")
};

// 혈액형
UENUM(BlueprintType)
enum class EBloodType : uint8
{
	A_Positive  UMETA(DisplayName = "A형 (RH+)"),
	A_Negative  UMETA(DisplayName = "A형 (RH-)"),
	B_Positive  UMETA(DisplayName = "B형 (RH+)"),
	B_Negative  UMETA(DisplayName = "B형 (RH-)"),
	O_Positive  UMETA(DisplayName = "O형 (RH+)"),
	O_Negative  UMETA(DisplayName = "O형 (RH-)"),
	AB_Positive UMETA(DisplayName = "AB형 (RH+)"),
	AB_Negative UMETA(DisplayName = "AB형 (RH-)")
};

// =================================================================
// 1. 센서와 통제탑 간의 통신 데이터 (Payload)
// =================================================================
USTRUCT(BlueprintType)
struct FInteractionPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	FGameplayTag UniqueID;		// 특정 엔트리르 가리킬 태그

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	FGameplayTagContainer InteractionTags; // 어떤 행동인가? (예: Action.Clean)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	AActor* TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	class APlayerState* InstigatorPlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Payload")
	TMap<FGameplayTag, FString> AdditionalData; // 약물명, 용량 등 추가 정보 딕셔너리
};

// =================================================================
// 2. BP_IZ 동적 스폰 세팅 데이터
// =================================================================
USTRUCT(BlueprintType)
struct FZoneData
{
	GENERATED_BODY()

	// 일회성 여부 변수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	bool bIsSingleUse = false;

	// 충돌처리 필터 태그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	FGameplayTagContainer CollisionFilterTags;

	// 환자에게 부착된 경우 사전에 필요한 처치 조건 태그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	FGameplayTagContainer RequiredStateTags;

	// 목표 인터랙션을 전달할 태그 컨테이너
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FGameplayTagContainer TargetInteractionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FName TargetSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FVector BoxExtent = FVector(10.f, 10.f, 10.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FTransform RelativeOffset;

	// 힌트를 활성화 체크 태그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Hint")
	FGameplayTag HintTargetTag;
};

USTRUCT(BlueprintType)
struct FZoneDataWrapper
{
	GENERATED_BODY()

	// 엔트리 스폰시 InteractionZone을 특정하기 위한 고유 ID
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FGameplayTag ZoneID;

	// 스폰할 BP_IZ의 서브클래스 (메모리 최적화를 위해 Soft Class Pointer 적용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	TSoftClassPtr<class AInteractionZoneBase> ZoneClass;

	// 어디를 기준으로 배치할 것인가?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	EZoneAnchorType AnchorType = EZoneAnchorType::Patient;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone", meta = (EditCondition =  "AnchorType == EZoneAnchorType::AttachedObject"))
	FName AnchorObjectTag = NAME_None;

	// 스폰된 클래스에 주입할 세팅 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	FZoneData ZoneData;
};

// =================================================================
// 3. 데이터 테이블 관리용 구조체 (FTableRowBase 상속 필수)
// =================================================================
USTRUCT(BlueprintType)
struct FZoneSpawnRow : public FTableRowBase
{
	GENERATED_BODY()

	// 스폰된 클래스에 주입할 세팅 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Zone")
	TArray<FZoneDataWrapper> ZoneDatas;
};


// =================================================================
// 1. 마스터 데이터테이블 규격 (엔트리의 모든 속성을 고정 가동하는 마스터 템플릿)
// (RowName을 EntryName 고유 키로 공용 사용)
// =================================================================
USTRUCT(BlueprintType)
struct FScenarioEntryTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MasterData")
	FGameplayTag EntryID;

	// 실제 월드에 스폰될 C++ 또는 블루프린트 엔트리 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MasterData")
	TSubclassOf<class AScenarioEntryBase> EntryClass;

	// [신규] 상호작용 발생 시 전달된 페이로드 태그와 비교할 목표 태그
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MasterData")
	FGameplayTag TargetInteractionTag;

	// 이 처치 행동의 목표 수행 횟수 기본값
	// -1일 경우 횟수로 성공조건 판단하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MasterData")
	int32 TargetExecutionCount = -1;
};

// 환자 처치시 추가할 비주얼 메시 구조체
USTRUCT(BlueprintType)
struct FTreatmentVisuals : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Treatment|Visual")
	FGameplayTag ObjectID;

	// 하드 포인터 대신 소프트 오브젝트 포인터를 사용하여 필요할 때만 메모리에 로드하도록 최적화합니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Treatment|Visual")
	TSoftObjectPtr<UStaticMesh> VisualMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Treatment|Visual")
	FName TargetSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Treatment|Visual")
	FTransform RelativeOffset = FTransform::Identity;
};

// 환자 수치 스펙에 사용될 구조체
USTRUCT(BlueprintType)
struct FNumericStat
{
	GENERATED_BODY()

	// 고정값, 범위 선택
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsFixed = true;

	// 최소값 (고정값 역할도 함께 수행)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinValue = 0.0f;
	
	// 최대값
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxValue = 0.0f;
};

USTRUCT(BlueprintType)
struct FPatientBaseInfo
{
	GENERATED_BODY()

	// 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	FName Name = NAME_None;

	// 성별
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	bool bIsMale = true;

	// 나이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	int32 Age = 20;

	// 혈액형
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	EBloodType BloodType = EBloodType::B_Positive;

	// 키
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	float Height = 180.0f;

	// 몸무게
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	float Weight = 80.0f;
};

// 시나리오 빌더 설정용 환자 기본 정보 구조체
USTRUCT(BlueprintType)
struct FPatientBaseInfoConfig
{
	GENERATED_BODY()

	// 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	FName Name = NAME_None;

	// 성별
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	bool bIsMale = true;

	// 나이
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	int32 Age = 20;

	// 혈액형
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	EBloodType BloodType = EBloodType::B_Positive;

	// 키
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	FNumericStat HeightStat;

	// 몸무게
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|Info")
	FNumericStat WeightStat;

	FPatientBaseInfo GenerateActualInfo() const
	{
		FPatientBaseInfo ActualInfo;
		ActualInfo.Name = Name;
		ActualInfo.bIsMale = bIsMale;
		ActualInfo.Age = Age;
		ActualInfo.BloodType = BloodType;

		// 키 무작위 산출 (Min/Max 교차 입력 방어)
		const float MinH = FMath::Min(HeightStat.MinValue, HeightStat.MaxValue);
		const float MaxH = FMath::Max(HeightStat.MinValue, HeightStat.MaxValue);
		ActualInfo.Height = HeightStat.bIsFixed ? MinH : FMath::FRandRange(MinH, MaxH);

		// 몸무게 무작위 산출 (Min/Max 교차 입력 방어)
		const float MinW = FMath::Min(WeightStat.MinValue, WeightStat.MaxValue);
		const float MaxW = FMath::Max(WeightStat.MinValue, WeightStat.MaxValue);
		ActualInfo.Weight = WeightStat.bIsFixed ? MinW : FMath::FRandRange(MinW, MaxW);

		return ActualInfo;
	}
};

USTRUCT(BlueprintType)
struct FPatientPartState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|PartState")
	EBodyPartState HeadState = EBodyPartState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|PartState")
	EBodyPartState TorsoState = EBodyPartState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|PartState")
	EBodyPartState RightArmState = EBodyPartState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|PartState")
	EBodyPartState LeftArmState = EBodyPartState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|PartState")
	EBodyPartState RightLegState = EBodyPartState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patient|PartState")
	EBodyPartState LeftLegState = EBodyPartState::Normal;
};

USTRUCT(BlueprintType)
struct FVitalSign
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalSign")
	bool IsABP = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalSign")
	int32 MinBP = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalSign")
	int32 MaxBP = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalSign")
	int32 HR = 80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalSign")
	int32 RR = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalSign")
	int32 SPO2 = 98;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalSign")
	float BT = 36.5f;
};

/** 페이즈나 엔트리 시작 시 바이탈 항목별로 정밀 변조하기 위한 데이터 구조체입니다. */
USTRUCT(BlueprintType)
struct FScenarioVitalModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	EVitalModifierOp HROp = EVitalModifierOp::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	int32 HRValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	EVitalModifierOp RROp = EVitalModifierOp::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	int32 RRValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	EVitalModifierOp SPO2Op = EVitalModifierOp::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	int32 SPO2Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	EVitalModifierOp BPOp = EVitalModifierOp::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	int32 MinBPValue = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	int32 MaxBPValue = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	bool bIsABP = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	EVitalModifierOp BTOp = EVitalModifierOp::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VitalModifier")
	float BTValue = 0.0f;
};