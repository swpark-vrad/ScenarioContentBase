#include "LogSystem/ScenarioLogBlueprintLibrary.h"
#include "LogSystem/ScenarioLogSubsystem.h"
#include "Global/ScenarioGameStateBase.h"
#include "Engine/World.h"

/*
1. 호스트(서버)가 로그를 남길 때의 호출 순서
서버는 스스로 권한이 있으므로 RPC 없이 바로 Multicast를 쏘는 흐름입니다.

BFL::RecordScenarioLog (최초 호출)
LogSubsystem::AddLog (문자열 포맷팅 및 서버 로컬 배열에 저장 후 FormattedLog 반환)
GS::AddDisplayLog (FormattedLog 전달)
GS::Multicast_BroadcastLog (모든 기기로 전파)
LogSubsystem::ReceiveNetworkLog (서버 본인은 배열 추가 생략, UI 갱신 이벤트만 발생)

2. 클라이언트가 로그를 남길 때의 호출 순서
클라이언트는 권한이 없으므로, 서버의 PlayerController를 통해 RPC를 보내 서버가 대신 로그를 처리하고 뿌려주도록 요청하는 흐름입니다.

BFL::RecordScenarioLog (최초 호출)
LogSubsystem::AddLog (클라이언트 내부 실행)
PC::Server_RequestLog (서버로 원본 데이터 전송 RPC 발송)
--- (이하 서버에서 실행) ---
LogSubsystem::AddLog (서버가 문자열 포맷팅 및 서버 로컬 배열에 저장 후 FormattedLog 반환)
GS::AddDisplayLog (FormattedLog 전달)
GS::Multicast_BroadcastLog (모든 기기로 전파)
--- (이하 다시 클라이언트에서 실행) ---
LogSubsystem::ReceiveNetworkLog (전달받은 FormattedLog를 로컬 배열에 추가하고 UI 갱신 이벤트 발생)
*/


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