#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FrameworkBridgeInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UFrameworkBridgeInterface : public UInterface
{
	GENERATED_BODY()
};

class SCENARIOCONTENT_API IFrameworkBridgeInterface
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Scenario|Interface")
	FString GetCultureToString() const;

};