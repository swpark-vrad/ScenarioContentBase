#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ScenarioDataTypes.h"
#include "ScenarioEntryBase.generated.h"

class AInteractionZoneBase;
class AScenarioPhaseBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEntryCompletedSignature, AScenarioEntryBase*, Entry, bool, bIsForced);

UCLASS(Abstract)
class SCENARIOCONTENT_API AScenarioEntryBase : public AActor
{
	GENERATED_BODY()

public:
	AScenarioEntryBase();

	// ==========================================
	// 1. 엔트리 기본 정보
	// ==========================================
	UPROPERTY(BlueprintReadOnly, Category = "Entry|Data")
	FGameplayTag EntryID;

	UPROPERTY(BlueprintReadOnly, Category = "Entry|Data")
	FGameplayTag TargetInteractionTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Entry|Data")
	FName EntryName;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Entry|Localization")
	FString GetActualName() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Entry|Data")
	bool bIsMandatory = true;

	// 이 처치 행동의 목표 수행 횟수 기본값
	// -1일 경우 횟수로 성공조건 판단하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Entry|Data")
	int32 TargetExecutionCount = -1;

	// ==========================================
	// 2. 엔트리 상태 변수
	// ==========================================

	// 현재 이 엔트리가 심사(체크) 중인지 여부
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Entry|State")
	bool bIsActive = false;

	// 컨텐츠 시작 후 수행된 전체 횟수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Entry|State")
	int32 TotalExecutionCount = 0;

	// 페이즈 시작 후 수행된 횟수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Entry|State")
	int32 CurrentExecutionCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_IsCompleted, BlueprintReadOnly, Category = "Entry|State")
	bool bIsCompleted = false;

	UFUNCTION()
	virtual void OnRep_IsCompleted();

	UPROPERTY(BlueprintAssignable, Category = "Entry|Events")
	FOnEntryCompletedSignature OnEntryCompleted;

	UPROPERTY(BlueprintReadOnly, Category = "Entry|State")
	AScenarioPhaseBase* OwnerPhase;

	// ==========================================
	// 3. 라이프사이클 및 핵심 함수
	// ==========================================
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Entry|Setup")
	virtual void InitializeEntry(AScenarioPhaseBase* InOwnerPhase, AInteractionZoneBase* TargetZone = nullptr);

	UFUNCTION(BlueprintPure, Category = "Entry|Data")
	const TArray<class AInteractionZoneBase*>& GetAssociatedZones() const;

	UFUNCTION(BlueprintPure, Category = "Entry|Data")
	bool FindAssociatedZone(FGameplayTag ZoneID, class AInteractionZoneBase*& FindZone) const;

	// [신규] 페이즈가 이 엔트리의 체크를 시작할 때 호출
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Entry|Control")
	virtual void StartEntry();

	// [신규] 페이즈가 종료되거나 엔트리가 완료되어 체크를 중단할 때 호출
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Entry|Control")
	virtual void EndEntry();

	// [신규] 블루프린트 오버라이드용 이벤트 (시작 시)
	UFUNCTION(BlueprintNativeEvent, Category = "Entry|Events")
	void ReceiveStartEntry();

	// [신규] 블루프린트 오버라이드용 이벤트 (종료 시)
	UFUNCTION(BlueprintNativeEvent, Category = "Entry|Events")
	void ReceiveEndEntry();

	// [추가] 페이즈 시작 시 자동으로 완료 처리할 조건이 있는지 검사하는 함수
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Entry|Condition")
	bool CheckAutoCompletionCondition() const;
	virtual bool CheckAutoCompletionCondition_Implementation() const;

	// 목표 수행횟수에 도달했는가
	UFUNCTION(BlueprintPure, Category = "Entry|Condition")
	bool IsTargetExecutionCountReached() const;

	UFUNCTION()
	void BroadcastProcessPayload(const FInteractionPayload& Payload);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintAuthorityOnly, Category = "Entry|Process")
	void ProcessPayload(const FInteractionPayload& Payload);

	// BlueprintNativeEvent의 실제 C++ 본문 연산을 담당할 구현부 함수입니다.
	virtual void ProcessPayload_Implementation(const FInteractionPayload& Payload);

	UFUNCTION(BlueprintNativeEvent, Category = "Entry|Condition")
	bool CheckTargetInteraction(const FInteractionPayload& Payload) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Entry|State")
	virtual void CompleteEntry();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Entry|State")
	virtual void ForceToCompleteEntry();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Scenario|Log")
	FString MakeExecuteMessage() const;
	virtual FString MakeExecuteMessage_Implementation() const;

	UPROPERTY(BlueprintReadOnly, Category = "Entry|Vital")
	FScenarioVitalModifier VitalModifier;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Entry|Data")
	FInteractionPayload CachedCurrentPayload;

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly, Category = "Entry|Process")
	void OnManualInteractionTriggered();
	virtual void OnManualInteractionTriggered_Implementation();

private:
	UPROPERTY()
	TArray<class AInteractionZoneBase*> AssociatedZones;
};