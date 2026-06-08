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

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ZoneData 동기화용 RepNotify 함수
	UFUNCTION()
	virtual void OnRep_ZoneData();

	/** 충돌한 액터가 유효한 도구인지 검사하고 페이로드를 조립하여 반환하는 Native Event (BP 오버라이드 가능) */
	UFUNCTION(BlueprintNativeEvent, Category = "Zone|Interaction")
	bool CheckAndBuildPayload(AActor* OtherActor, FInteractionPayload& OutPayload);

	/** 하위 상속 클래스들이 조립 완료된 페이로드를 안전하게 방송할 수 있도록 캡슐화된 통로 함수 */
	void BroadcastInteractionTriggered(const FInteractionPayload& Payload);

	/** [★ 추가] 진입한 액터가 인터페이스를 구현했는지, 필터 태그에 합격했는지 C++ 단에서 초고속 검사하는 방어 함수 */
	bool IsValidInteractableActor(AActor* OtherActor) const;

public:
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
	UPROPERTY(ReplicatedUsing = OnRep_ZoneData, BlueprintReadWrite, EditAnywhere, Category = "Zone|Data", meta = (ExposeOnSpawn = "true"))
	FZoneData ZoneData;

	UPROPERTY(BlueprintAssignable, Category = "Zone|Events")
	FOnInteractionTriggered OnInteractionTriggered;
};