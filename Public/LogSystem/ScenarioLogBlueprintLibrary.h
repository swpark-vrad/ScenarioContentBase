#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ScenarioLogBlueprintLibrary.generated.h"

UCLASS()
class SCENARIOCONTENT_API UScenarioLogBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** 시나리오 컨텐츠 전역에서 공용으로 사용할 원스톱 로그 및 모니터 출력 함수 */
    UFUNCTION(BlueprintCallable, Category = "Scenario|Log", meta = (WorldContext = "WorldContextObject"))
    static void RecordScenarioLog(const UObject* WorldContextObject, const FString& Instigator, const FString& Message);
};