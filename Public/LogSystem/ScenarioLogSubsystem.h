#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScenarioLogSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLogUpdatedSignature);

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

    // UI가 바인딩할 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Practice Log")
    FOnLogUpdatedSignature OnLogUpdated;

    // 네트워크를 통해 전달받은 로그를 로컬에 저장하는 함수
    void ReceiveNetworkLog(const FString& NewLog);

    // UI 출력을 위해 전체 로그를 반환하는 함수
    UFUNCTION(BlueprintPure, Category = "Practice Log")
    TArray<FString> GetLogEntries() const { return LogEntries; }

private:
    bool bIsLogging = false;
    FString CurrentSessionName;
    FDateTime SessionStartTime;
    TArray<FString> LogEntries;

    // 현재 실행 중인 환경이 호스트(서버)인지 확인하는 함수
    bool IsHost() const;
};