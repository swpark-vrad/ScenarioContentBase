#include "LogSystem/ScenarioLogBlueprintLibrary.h"
#include "LogSystem/ScenarioLogSubsystem.h"
#include "Global/ScenarioGameStateBase.h"
#include "Engine/World.h"

void UScenarioLogBlueprintLibrary::RecordScenarioLog(const UObject* WorldContextObject, const FString& Instigator, const FString& Message)
{
    if (!WorldContextObject) return;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return;

    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return;

    UScenarioLogSubsystem* LogSubsystem = GI->GetSubsystem<UScenarioLogSubsystem>();
    if (!LogSubsystem) return;

    // 서브시스템에 정밀 포맷팅 처리 위임 및 파일 기록
    FString FormattedLog = LogSubsystem->AddLog(Instigator, Message);

    // 현재 노드를 가동시킨 맥락이 서버 환경인 경우 실습실 모니터 디스플레이 배열 동시 탑재
    if (World->GetNetMode() != NM_Client)
    {
        AScenarioGameStateBase* GS = World->GetGameState<AScenarioGameStateBase>();
        if (GS)
        {
            GS->AddDisplayLog(FormattedLog);
        }
    }
}