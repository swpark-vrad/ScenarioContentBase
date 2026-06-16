#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioLogSubsystem.generated.h"

UCLASS()
class SCENARIOCONTENT_API UScenarioLogSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 실습 로그 기록 시작 (레벨 이동 후 호스트에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Practice Log")
    void StartLogging(const FString& SessionName);

    // 실습 중 학생 행동 및 로그 추가
    UFUNCTION(BlueprintCallable, Category = "Practice Log")
    FString AddLog(const FString& LogInstigator, const FString& LogMessage);

    // 실습 종료 및 파일 저장
    UFUNCTION(BlueprintCallable, Category = "Practice Log")
    void EndLogging();

private:
    bool bIsLogging = false;
    FString CurrentSessionName;
    FDateTime SessionStartTime;
    TArray<FString> LogEntries;

    // 현재 실행 중인 환경이 호스트(서버)인지 확인하는 함수
    bool IsHost() const;
};