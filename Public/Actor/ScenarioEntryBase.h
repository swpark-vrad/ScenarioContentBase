#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ScenarioDataTypes.h"
#include "ScenarioEntryBase.generated.h"

class AInteractionZoneBase;
class AScenarioPhaseBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEntryCompleted, class AScenarioEntryBase*, CompletedEntry);

UCLASS(Abstract)
class SCENARIOCONTENT_API AScenarioEntryBase : public AActor
{
	GENERATED_BODY()

public:
	AScenarioEntryBase();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

public:
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Entry|Data")
	int32 TargetExecutionCount = 1;

	// ==========================================
	// 2. 엔트리 상태 변수
	// ==========================================

	// 현재 이 엔트리가 심사(체크) 중인지 여부
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Entry|State")
	bool bIsActive = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Entry|State")
	int32 CurrentExecutionCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_IsCompleted, BlueprintReadOnly, Category = "Entry|State")
	bool bIsCompleted = false;

	UFUNCTION()
	virtual void OnRep_IsCompleted();

	UPROPERTY(BlueprintAssignable, Category = "Entry|Events")
	FOnEntryCompleted OnEntryCompleted;

	UPROPERTY(BlueprintReadOnly, Category = "Entry|State")
	AScenarioPhaseBase* OwnerPhase;

	// ==========================================
	// 3. 라이프사이클 및 핵심 함수
	// ==========================================
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Entry|Setup")
	virtual void InitializeEntry(AScenarioPhaseBase* InOwnerPhase, AInteractionZoneBase* TargetZone = nullptr);

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
};