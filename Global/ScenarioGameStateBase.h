#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Data/ScenarioDataTypes.h"
#include "ScenarioGameStateBase.generated.h"

class AScenarioPhaseBase;
class AScenarioEntryBase;
class AInteractionZoneBase;
class APlayerState;

// 델리게이트
// 시나리오 데이터 로드 완료시 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScenarioDataReadySignature);
// 유저 접속, 접속해제시 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserChangedSignature, APlayerState*, PlayerState);
// 시간이 변경될 때마다 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClockUpdatedSignature, int32, ElapsedSeconds);
// 페이즈 시간 변경될때마다 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseTimeUpdatedSignature, int32, RemainingSeconds, float, RemainingRatio);
// 시나리오 시작시 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScenarioStartedSignature, bool, bIsStarted);
// 시나리오 일시정지시 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScenarioPausedSignature, bool, bIsPaused);
// 페이즈 시작시 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseStartedSignature, FName, PhaseName);
// 페이즈 데이터 변경시(새로운 페이즈로 바뀌거나, 엔트리 체크 변경) 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEntryDatasUpdatedSignature);


UCLASS()
class SCENARIOCONTENT_API AScenarioGameStateBase : public AGameStateBase
{
    GENERATED_BODY()

public:
    AScenarioGameStateBase();

    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void RemovePlayerState(APlayerState* PlayerState) override;

    // ==========================================
    // 1. 기존 시스템 (유저, 시간, 기본 데이터)
    // ==========================================
    void OnPlayerIdentityReady(APlayerState* PlayerState);

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Time")
    void StartScenarioClock();

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Time")
    void StopScenarioClock();

    void NotifyDataReadyToLocalClients();

    UPROPERTY(BlueprintAssignable, Category = "Scenario|User")
    FOnUserChangedSignature OnUserAdded;

    UPROPERTY(BlueprintAssignable, Category = "Scenario|User")
    FOnUserChangedSignature OnUserRemoved;

    UPROPERTY(BlueprintAssignable, Category = "Scenario|Time")
    FOnClockUpdatedSignature OnClockUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Scenario|Data")
    FOnScenarioDataReadySignature OnScenarioDataReady;

    UPROPERTY(BlueprintAssignable, Category = "Scenario|Phase")
    FOnPhaseStartedSignature OnPhaseStarted;

    UPROPERTY(BlueprintAssignable, Category = "Scenario|UI")
    FOnEntryDatasUpdatedSignature OnEntryDatasUpdated;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ActiveScenarioID)
    FName ActiveScenarioID;

    UFUNCTION()
    void OnRep_ActiveScenarioID();

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scenario|Time")
    FDateTime StartDateTime;

    UPROPERTY(ReplicatedUsing = OnRep_ProgressTime, BlueprintReadOnly, Category = "Scenario|Time")
    int32 ProgressTime;

    UPROPERTY(BlueprintAssignable, Category = "Scenario|Control")
    FOnPhaseTimeUpdatedSignature OnPhaseTimeUpdated;

    /** 현재 진행 중인 페이즈의 남은 시간 (초 단위, 복제 변수) */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseRemainingTime, BlueprintReadOnly, Category = "Scenario|Control")
    int32 CurrentPhaseRemainingTime = 0;

    /** 신규: 비율 계산을 위해 늦게 들어온 유저에게도 공유될 페이즈의 총 제한시간 */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Scenario|Control")
    int32 CurrentPhaseTotalTime = 0;

    UFUNCTION()
    void OnRep_CurrentPhaseRemainingTime();

    UFUNCTION(BlueprintCallable, Category = "Scenario|UI")
    FText GetFormattedPhaseTimeText() const;

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Control")
    void ProcessInteractionPayload(const FInteractionPayload& Payload);

protected:
    FTimerHandle ClockTimerHandle;

    UFUNCTION()
    void UpdateScenarioClock();

    UFUNCTION()
    void OnRep_ProgressTime();

    // 에디터에서 지정할 마스터 엔트리 데이터 테이블 (DT_EntryMaster)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Setup")
    class UDataTable* EntryMasterTable;

    // 데이터 기반 페이즈 액터를 동적 생성하기 위한 기본 페이즈 클래스 (BP_Phase_Base)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Setup")
    TSubclassOf<AScenarioPhaseBase> DefaultPhaseClass;

    UFUNCTION()
    void HandleEntryCompletedFromWorld(AScenarioEntryBase* CompletedEntry);

    UPROPERTY()
    TMap<FGameplayTag, AScenarioEntryBase*> ActiveEntryMap;


public:
    // ==========================================
    // 2. 글로벌 존 및 페이즈 샌드박스 관리
    // ==========================================
    UPROPERTY(BlueprintAssignable, Category = "Scenario|Control")
    FOnScenarioStartedSignature OnScenarioStarted;

    UPROPERTY(BlueprintAssignable, Category = "Scenario|Control")
    FOnScenarioPausedSignature OnScenarioPaused;

    // 늦게 들어오는 유저의 동기화를 위해 시작 변수를 상시 리플리케이션 구조로 복원합니다
    UPROPERTY(ReplicatedUsing = OnRep_bIsStarted, BlueprintReadOnly, Category = "Scenario|Control")
    bool bIsStarted = false;

    UPROPERTY(ReplicatedUsing = OnRep_bIsPaused, BlueprintReadOnly, Category = "Scenario|Control")
    bool bIsPaused = false;

    UFUNCTION()
    void OnRep_bIsStarted();

    UFUNCTION()
    void OnRep_bIsPaused();

    UPROPERTY(ReplicatedUsing = OnRep_CurrentPhaseName, BlueprintReadOnly, Category = "Scenario|Phase")
    FName CurrentPhaseName;

    UFUNCTION()
    void OnRep_CurrentPhaseName();

    UPROPERTY(BlueprintReadOnly, Category = "Scenario|Phase")
    AScenarioPhaseBase* CurrentPhase;

    UPROPERTY(BlueprintReadOnly, Category = "Scenario|Phase")
    TArray<AScenarioPhaseBase*> ScenarioPhases;

    UPROPERTY(BlueprintReadOnly, Category = "Scenario|Zone")
    TMap<FName, AInteractionZoneBase*> GlobalActiveZones;

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Setup")
    void BuildGlobalScenarioEnvironment();

    // 호스트의 명시적 요청에 의해 실제 시나리오를 가동시키는 뇌관 함수를 추가합니다
    UFUNCTION(BlueprintAuthorityOnly, BlueprintNativeEvent, BlueprintCallable, Category = "Scenario|Control")
    void StartScenario();
    virtual void StartScenario_Implementation();

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Control")
    void PauseScenario();

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Control")
    void ResumeScenario();

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Phase")
    void StartPhaseByName(FName PhaseName);

    UFUNCTION()
    void HandlePhaseCompleted(AScenarioPhaseBase* CompletedPhase, bool bIsSuccess);

    // ==========================================
    // 3. 통합 UI 및 히스토리 관리 (네트워크 최적화)
    // ==========================================
    UPROPERTY(ReplicatedUsing = OnRep_CurrentEntryDatas, BlueprintReadOnly, Category = "Scenario|UI")
    TArray<FScenarioEntryUIData> CurrentEntryDatas;

    UFUNCTION()
    void OnRep_CurrentEntryDatas();

    UPROPERTY(BlueprintReadOnly, Category = "Scenario|History")
    TMap<FName, FPhaseHistoryData> PhaseHistoryMap;

    void InitializePhaseEntryDatas();

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|UI")
    void UpdateEntryUIState(FName EntryName, bool bCompleted);
};