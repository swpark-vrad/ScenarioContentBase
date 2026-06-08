#pragma once

#include "CoreMinimal.h"
#include "Actor/InteractionZoneBase.h"
#include "GameplayTagContainer.h"
#include "InteractionZone_Responsive.generated.h"

class APlayerState;

// 실시간 상태 변화 프레임에 맞춰 유연하게 위젯 연출이나 엔트리로 전달할 전용 멀티캐스트 대리자
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResponsiveInputSignature, const FInteractionPayload&, Payload);

UCLASS()
class SCENARIOCONTENT_API AInteractionZone_Responsive : public AInteractionZoneBase
{
	GENERATED_BODY()

public:
	AInteractionZone_Responsive();
	
	// 요청하신 6가지 정밀 분기 이벤트 채널
	UPROPERTY(BlueprintAssignable, Category = "Zone|ResponsiveEvents")
	FOnResponsiveInputSignature OnGrabPressed;

	UPROPERTY(BlueprintAssignable, Category = "Zone|ResponsiveEvents")
	FOnResponsiveInputSignature OnGrabHeld;

	UPROPERTY(BlueprintAssignable, Category = "Zone|ResponsiveEvents")
	FOnResponsiveInputSignature OnGrabReleased;

	UPROPERTY(BlueprintAssignable, Category = "Zone|ResponsiveEvents")
	FOnResponsiveInputSignature OnTriggerPressed;

	UPROPERTY(BlueprintAssignable, Category = "Zone|ResponsiveEvents")
	FOnResponsiveInputSignature OnTriggerHeld;

	UPROPERTY(BlueprintAssignable, Category = "Zone|ResponsiveEvents")
	FOnResponsiveInputSignature OnTriggerReleased;

	// 충돌 비활성화 및 메모리 누수를 막기 위한 변수 초기화
	UFUNCTION(BlueprintCallable, Category = "Zone|Control")
	void DeactivateAndShutdown();

protected:
	virtual void BeginPlay() override;

	// 피지컬 월드 상의 진입 및 이탈 라이프사이클 이벤트 감시용 핸들러
	UFUNCTION()
	virtual void OnBoxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnBoxOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 0.1초마다 비동기 쿼리를 날려 직전 프레임 태그 버퍼와 비교 분석하는 핵심 알고리즘 */
	void OnResponsiveTrackTick();

	/** 액터 소유권 체인을 정밀 역추적하여 상위 VR 유저의 최상위 PlayerState 주소를 반환하는 내부 헬퍼 */
	APlayerState* GetPlayerStateFromActor(AActor* InActor) const;

	// [선점형 소유권 락] 현재 이 구역을 독점 선점하여 작업 중인 마스터 유저 정보
	UPROPERTY()
	APlayerState* ActiveTrackingPlayer;

	// [양손 교체 예외처리] 선점 유저가 해당 영역 내에 밀어 넣은 도구 및 핸들의 실시간 총 누적 개수
	int32 CurrentPlayerOverlapCount;

	// [상태 천이 알고리즘] 직전 분석 주기(0.1초 전)에 활성화되어 저장되어 있던 입력 태그 버퍼
	FGameplayTagContainer PreviousInputTags;

	FTimerHandle ResponsiveTrackTimerHandle;
	float TrackInterval;

};