#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ScenarioDataTypes.h"
#include "Data/ScenarioSaveTypes.h"
#include "ScenarioBuildSubsystem.generated.h"

// 시나리오 제작하는 동안 사용될 페이즈 구조체
// 어떤 엔트리를 담고있는지 저장
USTRUCT(BlueprintType)
struct FPhaseEditData
{
    GENERATED_BODY()

    FPhaseSaveData PhaseData;

    TArray<FName> ContainEntries;
};

USTRUCT(BlueprintType)
struct FLoadedNodePositionData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Scenario")
    TMap<FName, FVector2D> Positions;
};

// 선행 엔트리 확인을 위한 데이터테이블 구조체
USTRUCT(BlueprintType)
struct FBuilderValidationRule : public FTableRowBase
{
    GENERATED_BODY()

    // 이 엔트리를 실행하기 위해 배낭에 반드시 들어있어야 하는 선행 RowName들
    UPROPERTY(EditAnywhere, Category = "Builder Rules")
    TArray<FName> RequiredEntryRowNames;
};

// 시나리오 무결성 발견시 심각도를 나타내는 Enum
UENUM(BlueprintType)
enum class EValidationSeverity : uint8
{
    Error   UMETA(DisplayName = "치명적 오류"),  // 게임 진행 불가 (Soft Lock 등)
    Warning UMETA(DisplayName = "경고")          // 진행은 되지만 의도치 않은 결과 발생 가능성
};

// 단일 무결성 검사 결과를 담는 구조체
USTRUCT(BlueprintType)
struct FValidationResult
{
    GENERATED_BODY()

    // 에러의 심각도 (빨간색 에러 마크냐, 노란색 경고 마크냐)
    UPROPERTY(BlueprintReadWrite, Category = "Scenario|Validation")
    EValidationSeverity Severity = EValidationSeverity::Error;

    // 에러가 발생한 노드의 ID (UI에서 해당 노드를 찾아 빨간 테두리를 켤 때 사용)
    UPROPERTY(BlueprintReadWrite, Category = "Scenario|Validation")
    FName TargetPhaseID = NAME_None;

    // 사용자에게 보여줄 에러 메시지 내용
    UPROPERTY(BlueprintReadWrite, Category = "Scenario|Validation")
    FString ErrorMessage = TEXT("");

    // 편의를 위한 생성자
    FValidationResult() {}
    FValidationResult(EValidationSeverity InSeverity, FName InTargetID, FString InMessage)
        : Severity(InSeverity), TargetPhaseID(InTargetID), ErrorMessage(InMessage) {
    }
};

// 시나리오 기본정보 델리게이트
// 카테고리별 구분하여 사용
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScenarioMetaUpdated);     // 이름, 설명 변경 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPatientInfoUpdated);      // 환자 기본정보
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPatientBodyPartUpdated);  // 환자 부위별 상태 변경 시
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVitalSignUpdated);        // 초기 VS 변경 시
// 페이즈 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseAdded, FName, PhaseID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseRemoved, FName, PhaseID);
// 이름, 시간, 연결, VS 등 페이즈의 '속성'이 변경되었을 때 범용으로 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseUpdated, FName, PhaseID);
// 페이즈의 연결 상태(인풋/아웃풋)가 변경되었을 때 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseConnectionChanged, FName, PhaseID);
// 엔트리 델리게이트 (어떤 페이즈의 엔트리인지 알기 위해 PhaseID도 함께 전달)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEntryAdded, FName, PhaseID, FName, EntryID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEntryRemoved, FName, PhaseID, FName, EntryID);
// RowName, 필수여부, VS 등 엔트리의 '속성'이 변경되었을 때 범용으로 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEntryUpdated, FName, EntryID);
// 특정 페이즈 내에서 엔트리 순서가 변경되었을 때 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEntryOrderChanged, FName, PhaseID);
// 시나리오 로드시
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnScenarioLoaded, const FLoadedNodePositionData&, NodePositionData, FVector2D, StartNodePos, FVector2D, EndNodePos);
// 시나리오 초기화시
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScenarioReset);

// 실시간 무결성 검사 결과를 UI에 쏴주는 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnValidationUpdated, const TArray<FValidationResult>&, ValidationResults);

UCLASS()
class SCENARIOCONTENT_API UScenarioBuildSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    // 위젯에서 사용할 번역용 텍스트맵 변수 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem")
    void SetEntryCultureTextData(const TMap<FName, FText>& InCultureTextData);
    // 번역용 텍스트맵 참조 함수
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Getter")
    const TMap<FName, FText>& GetEntryCultureTextData() const { return EntryCultureTextData; }

    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Getter")
    FText GetEntryCultureText(FName EntryRowName) const;

    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Data")
    const FScenarioSaveData& GetScenarioSaveData() const;

    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Getter")
    bool GetPhaseData(FName PhaseID, FPhaseSaveData& OutPhaseData) const;

    // 특정 엔트리의 데이터 가져오기
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Getter")
    bool GetEntryData(FName EntryID, FEntrySaveData& OutEntryData) const;

    // 편의성 함수: 특정 페이즈에 속한 '엔트리 ID 배열'만 빠르게 가져오기 (순서 갱신용)
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Getter")
    bool GetEntriesInPhase(FName PhaseID, TArray<FName>& OutEntryIDs) const;
     
    // 시작 페이즈ID 참조
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Save")
    FName GetStartPhaseID() const { return StartPhaseID; }

    // 기본정보 ============================================================
    // 시나리오ID 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem")
    void SetScenarioID(FName ScenarioID);

    // 상황소개지 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem")
    void SetScenarioDescription(FText Description);

    // 환자 기본정보 설정
    // 환자 기본정보 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem")
    void SetPatientBaseInfoConfig(const FPatientBaseInfoConfig& Config);

    // 초기 VS 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem")
    void SetInitVitalSign(const FVitalSign& VitalSign);

    // 환자 부위별 상태 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem")
    void SetPatientPartState(const FPatientPartState& PartState);
    //=========================================================================

    // 페이즈 ==================================================================
    // 새로운 페이즈 추가
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    FName AddNewPhase();

    // 페이즈 제거
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    bool RemovePhase(FName PhaseID);

    // 페이즈 진행 시간 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    bool SetPhaseName(FName TargetPhaseID, FName NewPhaseName);

    // 페이즈 진행 시간 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    void SetPhaseDuration(FName TargetPhaseID, float NewDuration);

    // 성공시 다음 페이즈 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    void SetNextPhase_Success(FName TargetPhaseID, FName NextPhaseID);
    // 실패시 다음 페이즈 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    void SetNextPhase_Fail(FName TargetPhaseID, FName NextPhaseID);

    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    void SetNextPhase_End(FName TargetPhaseID, bool bIsSuccessPin);

    // 시작 페이즈ID 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    void SetStartPhaseID(FName NewStartPhaseID);

    // 페이즈 VSModOp 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    void SetPhaseVSModOp(FName TargetPhaseID, const FScenarioVitalModifier& VSModOp);

    // 필수 엔트리가 없을 때 호출하여 Fail 핀의 연결 데이터를 NAME_None으로 초기화합니다.
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Phase")
    void ClearFailureConnection(FName PhaseID);


    // =======================================================================

    // 엔트리 =================================================================
    // 엔트리를 보유한 페이즈ID 참조
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Entry")
    FName GetParentPhaseID(FName EntryID) const;

    // 새로운 엔트리 추가
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Entry")
    FName AddNewEntry(FName OwnPhaseID, FName EntryRowName);

    // 엔트리 제거
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Entry")
    bool RemoveEntry(FName EntryID);

    // 특정 엔트리를 한 칸 위로(앞으로) 이동
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Entry")
    bool MoveEntryUp(FName PhaseID, FName EntryID);

    // 특정 엔트리를 한 칸 아래로(뒤로) 이동
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Entry")
    bool MoveEntryDown(FName PhaseID, FName EntryID);

    // 엔트리 코드 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Entry")
    bool SetEntryRowName(FName TargetEntryID, FName NewEntryRowName);

    // 필수 여부 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Entry")
    void SetEntryMandatory(FName TargetEntryID, bool bIsMandatory);

    // VSModifier 설정
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Entry")
    void SetEntryVSModOp(FName TargetEntryID, const FScenarioVitalModifier& VSModOp);


    // =======================================================================

    // 델리게이트 변수==========================================================
    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnScenarioMetaUpdated OnScenarioMetaUpdated;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnPatientInfoUpdated OnPatientInfoUpdated;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnPatientBodyPartUpdated OnPatientBodyPartUpdated;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnVitalSignUpdated OnVitalSignUpdated;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnPhaseAdded OnPhaseAdded;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnPhaseRemoved OnPhaseRemoved;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnPhaseUpdated OnPhaseUpdated;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnPhaseConnectionChanged OnPhaseConnectionChanged;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnEntryAdded OnEntryAdded;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnEntryRemoved OnEntryRemoved;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnEntryUpdated OnEntryUpdated;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnEntryOrderChanged OnEntryOrderChanged;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnScenarioLoaded OnScenarioLoaded;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnScenarioReset OnScenarioReset;

    UPROPERTY(BlueprintAssignable, Category = "ScenarioBuildSubsystem|Event")
    FOnValidationUpdated OnValidationUpdated;

    // =======================================================================

    // 무결성 체크 ============================================================
    // 특정 페이즈의 연결 여부 확인
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Validate")
    void GetPhaseConnectionStates(FName PhaseID, bool& bOutInputConnected, bool& bOutSuccessConnected, bool& bOutFailConnected) const;
    
    // 시작 페이즈 설정 여부 확인
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Validation")
    bool IsValidStartPhase() const;

    // 종료 노드의 핀(Input)이 연결되어 있는지 반환
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Validation")
    bool IsEndConnected() const;

    // 해당 페이즈에 '필수(Mandatory)' 엔트리가 최소 1개 이상 존재하는지 검사합니다.
    UFUNCTION(BlueprintPure, Category = "ScenarioBuildSubsystem|Validation")
    bool HasMandatoryEntries(FName PhaseID) const;

    // [마스터 함수] 에러 리스트를 갱신하고 반환
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|Validation")
    TArray<FValidationResult> ValidateScenario();

    // ======================================================================

    // 시나리오 저장 ===========================================================
    // 시나리오 초기화
    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem")
    void ResetScenario();

    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|SaveLoad")
    bool SaveScenario(const TMap<FName, FVector2D>& NodePositions, FVector2D InStartNodePos, FVector2D InEndNodePos);

    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|SaveLoad")
    TArray<class UScenarioBuilderSaveGame*> GetAllScenarioSaves();

    UFUNCTION(BlueprintCallable, Category = "ScenarioBuildSubsystem|SaveLoad")
    bool LoadScenario(FString SlotNameToLoad);

    // =======================================================================

private:

    // 현재 에디터에서 열려있거나 마지막으로 저장된 슬롯(파일) 이름. 
    // 비어있다면 새로 생성(Save)하는 시나리오를 의미합니다.
    FString CurrentSaveSlotName;

    // 저장용 구조체
    FScenarioSaveData ScenarioSaveData;

    // 시작 페이즈ID 저장
    FName StartPhaseID;

    // 페이즈와 엔트리를 별도로 관리할 맵 변수
    TMap<FName, FPhaseEditData> ActivePhaseData;
    TMap<FName, FEntrySaveData> ActiveEntryData;

    // 페이즈 추가시 이름 설정에 사용될 변수
    int32 PhaseIdCounter = 0;
    // 페이즈 추가시 이름 설정에 사용될 변수
    int32 EntryIdCounter = 0;

    // 동일한 엔트리도 맵에 추가할 수 있도록 EntryRowName이 아닌
    // 고유의ID를 생성하여 맵에 저장
    FName MakeUniqueEntryID();


    // 엔트리 코드와 번역된 텍스트를 관리할 맵
    TMap<FName, FText> EntryCultureTextData;


    // [헬퍼 함수들] 각 카테고리별 검사를 수행합니다. C++ 내부에서만 사용됩니다.
    void CheckFlowErrors(TArray<FValidationResult>& OutErrors) const;
    void CheckPhaseErrors(TArray<FValidationResult>& OutErrors) const;
    void CheckEntryErrors(TArray<FValidationResult>& OutErrors) const;
    void CheckMetaDataErrors(TArray<FValidationResult>& OutErrors) const;
};
