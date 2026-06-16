#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ScenarioDataTypes.h" // 우리가 만든 구조체 헤더 필수 인클루드
#include "InteractionZoneBase.generated.h"

// 전방 선언
class UBoxComponent;
class UStaticMeshComponent;

// 통제탑(Phase/Entry)으로 페이로드를 던져줄 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionTriggered, const FInteractionPayload&, Payload);

UCLASS(Abstract)
class SCENARIOCONTENT_API AInteractionZoneBase : public AActor
{
	GENERATED_BODY()

public:
	AInteractionZoneBase();

	// 충돌 비활성화 및 메모리 누수를 막기 위한 변수 초기화
	UFUNCTION(BlueprintCallable, Category = "Zone|Control")
	virtual void DeactivateAndShutdown();

	// 환자 인터페이스 캐싱을 위한 함수
	void SetPatientActor(AActor* InPatientActor);

	UFUNCTION(NetMulticast, Reliable, Category = "Zone|Hint")
	void Multicast_SetActivateHint(bool bActivate);
	virtual void Multicast_SetActivateHint_Implementation(bool bActivate);

	// ==========================================
	// 컴포넌트
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|Components")
	UBoxComponent* CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|Components")
	UStaticMeshComponent* HighlightMeshComp;

	// ==========================================
	// 변수 & 델리게이트
	// ==========================================
	// ExposeOnSpawn을 통해 스폰 시점에 데이터를 주입받고, RepNotify로 클라이언트 동기화
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|Data")
	FGameplayTag ZoneID;

	UPROPERTY(ReplicatedUsing = OnRep_ZoneData, BlueprintReadWrite, EditAnywhere, Category = "Zone|Data", meta = (ExposeOnSpawn = "true"))
	FZoneData ZoneData;

	UPROPERTY(BlueprintAssignable, Category = "Zone|Events")
	FOnInteractionTriggered OnInteractionTriggered;


protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ZoneData 동기화용 RepNotify 함수
	UFUNCTION()
	virtual void OnRep_ZoneData();

	/** 충돌한 액터가 유효한 도구인지 검사하고 페이로드를 조립하여 반환하는 Native Event (BP 오버라이드 가능) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Zone|Interaction")
	bool CheckAndBuildPayload(AActor* OtherActor, FInteractionPayload& OutPayload);

	virtual bool CheckAndBuildPayload_Implementation(AActor* OtherActor, FInteractionPayload& OutPayload);

	// 인터랙션 통과하는 경우 델리게이트 전파할 함수
	UFUNCTION(BlueprintCallable, Category = "Zone|Interaction")
	virtual void BroadcastInteractionTriggered(const FInteractionPayload& Payload);

	/** 액터 소유권 체인을 정밀 역추적하여 상위 VR 유저의 최상위 PlayerState 주소를 반환하는 내부 헬퍼 */
	APlayerState* GetPlayerStateFromActor(AActor* InActor) const;

	/** [★ 추가] 진입한 액터가 인터페이스를 구현했는지, 필터 태그에 합격했는지 C++ 단에서 초고속 검사하는 방어 함수 */
	bool IsValidInteractableActor(AActor* OtherActor) const;

	// 구역의 셧다운 상태를 모든 클라이언트에 동기화하기 위한 복제 변수입니다.
	UPROPERTY(ReplicatedUsing = OnRep_bIsShutdown, BlueprintReadOnly, Category = "Zone|State")
	bool bIsShutdown = false;

	// 셧다운 변수가 복제되었을 때 콜리전과 가시성을 정리하는 RepNotify 함수입니다.
	UFUNCTION()
	virtual void OnRep_bIsShutdown();

	// 환자 인터페이스 캐싱
	UPROPERTY()
	AActor* CachedPatientActor;
};