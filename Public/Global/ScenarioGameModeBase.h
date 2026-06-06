// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ScenarioGameModeBase.generated.h"

UCLASS()
class SCENARIOCONTENT_API AScenarioGameModeBase : public AGameModeBase
{
    GENERATED_BODY()

public:
    AScenarioGameModeBase();

    // 에디터 디테일 패널에서 지정할 호스트 PC용 폰 클래스입니다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classes")
    TSubclassOf<APawn> PCPawnClass;

    // 에디터 디테일 패널에서 지정할 클라이언트 VR용 폰 클래스입니다
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Classes")
    TSubclassOf<APawn> VRPawnClass;

    // 플레이어가 스폰될 PlayerStart 액터를 선택하는 가상 함수를 오버라이드합니다.
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

protected:
    virtual void BeginPlay() override;

    // 엔진의 기본 폰 결정 로직을 가로채기 위한 핵심 오버라이드 함수 선언입니다
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};