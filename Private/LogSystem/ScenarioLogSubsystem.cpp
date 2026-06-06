#include "LogSystem/ScenarioLogSubsystem.h"
#include "Global/ScenarioPlayerControllerBase.h"
#include "Global/ScenarioPlayerStateBase.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Engine/World.h"

bool UScenarioLogSubsystem::IsHost() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // 클라이언트가 아닌 경우(리슨 서버 혹은 데디케이트 서버) 즉, 호스트 PC인 경우에만 true 반환
    return World->GetNetMode() != NM_Client;
}

void UScenarioLogSubsystem::StartLogging(const FString& SessionName)
{
    if (!IsHost())
    {
        return;
    }

    LogEntries.Empty();
    CurrentSessionName = SessionName.IsEmpty() ? TEXT("DefaultSession") : SessionName;
    SessionStartTime = FDateTime::Now();
    bIsLogging = true;

    LogAction(TEXT("Practice Session Started."));
}

void UScenarioLogSubsystem::LogAction(const FString& LogMessage)
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (IsHost())
    {
        if (!bIsLogging) return;

        FString FinalMessage = LogMessage;

        // 만약 호스트 자신이 직접 이 함수를 호출했고, 아직 이름 테이블 작업([유저명])이 안 되어 있다면
        // 호스트의 PlayerState에서 이름을 찾아 붙여줍니다.
        if (!LogMessage.StartsWith(TEXT("[")))
        {
            FString HostName = TEXT("Host_PC");
            if (APlayerController* LocalPC = World->GetFirstPlayerController())
            {
                if (AScenarioPlayerStateBase* HostPS = LocalPC->GetPlayerState<AScenarioPlayerStateBase>())
                {
                    if (!HostPS->UserName.IsEmpty())
                    {
                        HostName = HostPS->UserName;
                    }
                }
            }
            FinalMessage = FString::Printf(TEXT("[%s] %s"), *HostName, *LogMessage);
        }

        // 시간 계산 및 로그 배열 추가 로직 (기존과 동일)
        float ElapsedTime = World->GetTimeSeconds();
        int32 Hours = FMath::FloorToInt(ElapsedTime / 3600.0f);
        int32 Minutes = FMath::FloorToInt(ElapsedTime / 60.0f) % 60;
        int32 Seconds = FMath::FloorToInt(ElapsedTime) % 60;

        FString FormattedLog = FString::Printf(TEXT("[%02d:%02d:%02d] %s"), Hours, Minutes, Seconds, *FinalMessage);
        LogEntries.Add(FormattedLog);
    }
    else
    {
        // 클라이언트인 경우 본인의 컨트롤러를 통해 서버 RPC 호출 (기존과 동일)
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (AScenarioPlayerControllerBase* MyPC = Cast<AScenarioPlayerControllerBase>(PC))
            {
                MyPC->Server_RequestLog(LogMessage);
            }
        }
    }
}

void UScenarioLogSubsystem::EndLogging()
{
    if (!IsHost() || !bIsLogging)
    {
        return;
    }

    LogAction(TEXT("Practice Session Ended."));
    bIsLogging = false;

    // 저장 디렉토리 경로 설정
    FString DirectoryPath = TEXT("C:/IPT/");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // C드라이브에 IPT 폴더가 없으면 생성
    if (!PlatformFile.DirectoryExists(*DirectoryPath))
    {
        PlatformFile.CreateDirectory(*DirectoryPath);
    }

    // 파일명 생성: 세션이름_년월일_시분초.txt
    FString Timestamp = SessionStartTime.ToString(TEXT("%Y%m%d_%H%M%S"));
    FString FullPath = FString::Printf(TEXT("%s%s_%s.txt"), *DirectoryPath, *CurrentSessionName, *Timestamp);

    // 누적된 로그 배열을 하나의 문자열로 결합
    FString FinalLogString;
    for (const FString& Entry : LogEntries)
    {
        FinalLogString += Entry + TEXT("\n");
    }

    // 파일 저장 (한글 깨짐 방지를 위해 UTF-8 적용)
    FFileHelper::SaveStringToFile(FinalLogString, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}