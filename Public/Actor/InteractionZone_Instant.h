#pragma once

#include "CoreMinimal.h"
#include "Actor/InteractionZoneBase.h"
#include "InteractionZone_Instant.generated.h"

UCLASS()
class SCENARIOCONTENT_API AInteractionZone_Instant : public AInteractionZoneBase
{
	GENERATED_BODY()

public:
	AInteractionZone_Instant();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 스패밍 방지용 체크 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Instant")
	float CooldownDuration;

private:
	float LastTriggerTime;
};