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

    FString Formatted = AddLog(FString(), TEXT("Practice Session Started."));
}

FString UScenarioLogSubsystem::AddLog(const FString& LogInstigator, const FString& LogMessage)
{
    UWorld* World = GetWorld();
    if (!World) return FString();

    // [신규 추가] Instigator가 비어있을 경우 "System" 명의로 자동 보정 처리
    FString FinalInstigator = LogInstigator.IsEmpty() ? TEXT("System") : LogInstigator;

    float ElapsedTime = World->GetTimeSeconds();
    int32 Hours = FMath::FloorToInt(ElapsedTime / 3600.0f);
    int32 Minutes = FMath::FloorToInt(ElapsedTime / 60.0f) % 60;
    int32 Seconds = FMath::FloorToInt(ElapsedTime) % 60;

    // 보정된 FinalInstigator 문자열을 사용하여 최종 포맷 조립
    FString FormattedLog = FString::Printf(TEXT("[%02d:%02d:%02d] [%s] %s"), Hours, Minutes, Seconds, *FinalInstigator, *LogMessage);

    if (IsHost())
    {
        if (!bIsLogging) return FormattedLog;
        LogEntries.Add(FormattedLog);
    }
    else
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (AScenarioPlayerControllerBase* MyPC = Cast<AScenarioPlayerControllerBase>(PC))
            {
                // 클라이언트 런타임일 경우 원본 인자를 그대로 RPC 파이프라인으로 우회 전송
                MyPC->Server_RequestLog(FinalInstigator, LogMessage);
            }
        }
    }
    return FormattedLog;
}

void UScenarioLogSubsystem::EndLogging()
{
    if (!IsHost() || !bIsLogging)
    {
        return;
    }

    FString Formatted = AddLog(FString(), TEXT("Practice Session Ended."));
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