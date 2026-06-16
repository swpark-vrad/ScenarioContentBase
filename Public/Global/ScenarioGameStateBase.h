#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Data/ScenarioDataTypes.h"
#include "Data/ScenarioSaveTypes.h"
#include "Data/ScenarioUITypes.h"
#include "ScenarioGameStateBase.generated.h"

class AScenarioPhaseBase;
class AScenarioEntryBase;
class AInteractionZoneBase;
class AScenarioPatientBase;
class APlayerState;
class AScenarioPatientBase;

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
// 환자 스폰시 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPatientSpawnedSignature, class AScenarioPatientBase*, SpawnedPatient);
// 페이즈 데이터 변경시(새로운 페이즈로 바뀌거나, 엔트리 체크 변경) 호출
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEntryDatasUpdatedSignature);
// 로그 추가시 호출될 델리게이트 (실습실 모니터에 로그 출력할때 사용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDisplayLogsUpdatedSignature);

// 캐싱에 사용될 래퍼 구조체
USTRUCT()
struct FInteractionZoneWrapper
{
    GENERATED_BODY()   

    UPROPERTY()
    TArray<class AInteractionZoneBase*> InteractionZoneActors;
};

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

    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Control")
    void RequestCompleteEntryByID(FGameplayTag EntryID);

    UFUNCTION(BlueprintImplementableEvent, Category = "Scenario|UI")
    void OnEntryForcedCompletedFromUI(FGameplayTag EntryID);

    UPROPERTY(BlueprintAssignable, Category = "Scenario|Patient")
    FOnPatientSpawnedSignature OnPatientSpawned;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Scenario|Lifecycle")
    void ActivatePatient(USceneComponent* InParentComponent);
    virtual void ActivatePatient_Implementation(USceneComponent* InParentComponent);

    UFUNCTION(BlueprintCallable, Category = "Scenario|Data")
    bool GetTreatmentVisualData(FGameplayTag TreatmentTag, FTreatmentVisuals& OutVisualData) const;

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
    void HandlePhaseCompleted(AScenarioPhaseBase* CompletedPhase, EPhaseState EndCondition);

    UFUNCTION(BlueprintCallable, Category = "Scenario|Hint")
    void ReceiveGrabSignal(FGameplayTag GrabbedTag);

    UFUNCTION(BlueprintCallable, Category = "Scenario|Data")
    class AScenarioEntryBase* GetActiveEntryByID(FGameplayTag EntryID) const;

    // [신규 추가] 서버 권한 단에서 포맷된 로그 문자열을 복제 배열에 탑재하는 함수
    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Scenario|Control|Log")
    void AddDisplayLog(const FString& NewLog);

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
    void UpdateEntryUIState(FGameplayTag EntryID, bool bCompleted);

    // [신규 추가] 실습실 모니터 위젯이 실시간 리프레시를 위해 구독할 동적 대리자
    UPROPERTY(BlueprintAssignable, Category = "Scenario|UI|Log")
    FOnDisplayLogsUpdatedSignature OnDisplayLogsUpdated;

    // [신규 추가] 모든 원격 VR 클라이언트로 패킷이 동시 자동 복제되는 디스플레이 로그 장부 배열
    UPROPERTY(ReplicatedUsing = OnRep_DisplayLogs, BlueprintReadOnly, Category = "Scenario|UI|Log")
    TArray<FString> DisplayLogs;

    UFUNCTION()
    void OnRep_DisplayLogs();

protected:
    FTimerHandle ClockTimerHandle;

    UFUNCTION()
    void UpdateScenarioClock();

    UFUNCTION()
    void OnRep_ProgressTime();

    // 에디터에서 지정할 마스터 엔트리 데이터 테이블 (DT_EntryMaster)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Setup")
    class UDataTable* EntryMasterTable;

    // 에디터에서 지정할 마스터 구역 데이터 테이블 (DT_ZoneMaster)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Setup")
    class UDataTable* ZoneMasterTable;

    // 처치 비주얼 마스터 데이터 테이블을 관리자 단인 GameState에서 단독 소유합니다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Setup")
    class UDataTable* TreatmentVisualTable;

    // 데이터 기반 페이즈 액터를 동적 생성하기 위한 기본 페이즈 클래스 (BP_Phase_Base)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Setup")
    TSubclassOf<AScenarioPhaseBase> DefaultPhaseClass;

    // 에디터 디테일 패널에서 지정할 환자 스폰 클래스 변수 추가
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scenario|Setup")
    TSubclassOf<AScenarioPatientBase> DefaultPatientClass;

    // 런타임에 스폰된 환자 액터의 포인터를 보관할 변수 (가비지 컬렉션 방지용 UPROPERTY)
    UPROPERTY(ReplicatedUsing = OnRep_SpawnedPatient, Transient, BlueprintReadOnly, Category = "Scenario|Runtime")
    AScenarioPatientBase* SpawnedPatient;

    UFUNCTION()
    void OnRep_SpawnedPatient();

    UFUNCTION()
    void HandleEntryCompletedFromWorld(AScenarioEntryBase* Entry, bool bIsForced);

    UPROPERTY()
    TMap<FGameplayTag, AScenarioEntryBase*> ActiveEntryMap;

    // 힌트활성화를 위한 IZ 캐싱
    UPROPERTY()
    TMap<FGameplayTag, FInteractionZoneWrapper> ActiveHintRegistry;
};