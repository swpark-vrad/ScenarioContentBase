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

	// 오버랩(충돌) 이벤트 함수
	UFUNCTION()
	virtual void OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// ZoneData 동기화용 RepNotify 함수
	UFUNCTION()
	virtual void OnRep_ZoneData();

	/*
	 * 충돌한 액터가 유효한 도구인지 검사하고 페이로드를 조립하여 반환합니다.
	 * 회사 블루프린트 프레임워크와의 호환성을 위해 BP에서 오버라이드하여 구현합니다.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Zone|Interaction")
	bool CheckAndBuildPayload(AActor* OtherActor, FInteractionPayload& OutPayload);

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