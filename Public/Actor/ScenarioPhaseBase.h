#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ScenarioDataTypes.h"
#include "ScenarioPhaseBase.generated.h"

class AScenarioEntryBase;
class AInteractionZoneBase;

// 페이즈의 정밀한 생명주기를 제어하기 위한 상태 정의
UENUM(BlueprintType)
enum class EPhaseState : uint8
{
	Active,             // 정상 진행 중
	Timeover,           // 시간 초과되었으나 다음 단계가 없어 유예(대기) 중인 상태
	Completed,			// 완료됨
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseCompleted, class AScenarioPhaseBase*, CompletedPhase, EPhaseState, EndCondition);

UCLASS(Abstract)
class SCENARIOCONTENT_API AScenarioPhaseBase : public AActor
{
	GENERATED_BODY()

public:
	AScenarioPhaseBase();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// ==========================================
	// 1. 페이즈 데이터 및 상태
	// ==========================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Data")
	FName PhaseName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Phase|Data")
	float TimeLimit = 120.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Phase|State")
	float PhaseStartTime = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsPhaseCompleted, BlueprintReadOnly, Category = "Phase|State")
	bool bIsPhaseCompleted = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Phase|State")
	bool bIsPhaseSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Phase|Vital")
	FScenarioVitalModifier VitalModifier;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Scenario")
	EPhaseState PhaseState = EPhaseState::Active;

	UFUNCTION()
	virtual void OnRep_IsPhaseCompleted();

	UPROPERTY(BlueprintAssignable, Category = "Phase|Events")
	FOnPhaseCompleted OnPhaseCompleted;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Phase")
	FName NextSuccessPhaseName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scenario|Phase")
	FName NextFailurePhaseName;

	// ==========================================
	// 2. 객체 관리
	// ==========================================
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Phase|State")
	TArray<AScenarioEntryBase*> ActiveEntries;

	UPROPERTY(BlueprintReadOnly, Category = "Phase|State")
	TMap<FName, AInteractionZoneBase*> ActiveZones;

	FTimerHandle PhaseTimerHandle;

	// ==========================================
	// 3. 라이프사이클 및 핵심 함수
	// ==========================================
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Phase|Control")
	virtual void StartPhase();

	// [신규] 블루프린트 오버라이드용 이벤트 (페이즈 시작 시 - 여기서 환자 스폰 가능)
	UFUNCTION(BlueprintNativeEvent, Category = "Phase|Events")
	void ReceiveStartPhase();

	// [신규] 블루프린트 오버라이드용 이벤트 (페이즈 종료 시)
	UFUNCTION(BlueprintNativeEvent, Category = "Phase|Events")
	void ReceiveEndPhase(EPhaseState EndCondition);

	UFUNCTION()
	virtual void OnPhaseTimeout();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Phase|Condition")
	virtual void CheckPhaseCompletion();

	UFUNCTION()
	virtual void HandleEntryCompleted(AScenarioEntryBase* CompletedEntry);

	virtual void EndPhase(EPhaseState EndCondition);

	void DeactivateEntries();
};