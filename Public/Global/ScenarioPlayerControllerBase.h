// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ScenarioPlayerControllerBase.generated.h"

UCLASS()
class SCENARIOCONTENT_API AScenarioPlayerControllerBase : public APlayerController
{
    GENERATED_BODY()

public:
    AScenarioPlayerControllerBase();

    // ==========================================
    // 1. 유저 및 세션 제어 인터페이스 (RPC)
    // ==========================================

    /** 위젯에서 특정 유저의 이름 변경을 요청하는 서버 RPC */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Scenario|Network")
    void Server_RequestChangeName(int32 TargetUserIndex, const FString& NewName);

    /** 호스트 위젯에서 현재 게임 세션 종료를 요청하는 서버 RPC */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Scenario|Network")
    void Server_RequestShutdownSession();

    /** 서버가 특정 클라이언트만 타겟팅하여 로비 레벨로 강제 이동시키는 클라이언트 RPC */
    UFUNCTION(Client, Reliable)
    void Client_ForceMoveToClientLobby();

    /** UI 위젯 버튼 클릭 시 특정 엔트리의 완료 처리를 서버에 요청하는 RPC 함수 */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Scenario|Network")
    void Server_RequestCompleteEntry(FGameplayTag EntryID);

    // ==========================================
    // 2. 시나리오 실행 및 데이터 로그 인터페이스
    // ==========================================

    /** 호스트 UI 위젯에서 시나리오 시작 버튼을 눌렀을 때 호출하는 로컬 함수 */
    UFUNCTION(BlueprintCallable, Category = "Scenario|Control")
    void RequestStartScenario();

    /** 호스트 UI 위젯에서 일시정지 버튼을 눌렀을 때 호출하는 로컬 함수 */
    UFUNCTION(BlueprintCallable, Category = "Scenario|Control")
    void RequestPauseScenario();

    /** 검증 과정을 거쳐 서버에서 실제 시나리오 환경을 조립하도록 지시하는 서버 RPC */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestStartScenario();

    /** 호스트 UI 위젯에서 재개 버튼을 눌렀을 때 호출하는 로컬 함수 */
    UFUNCTION(BlueprintCallable, Category = "Scenario|Control")
    void RequestResumeScenario();

    /** 서버에서 실제 시나리오를 일시정지하도록 지시하는 서버 RPC */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestPauseScenario();

    UFUNCTION(BlueprintCallable, Category = "Scenario|Control")
    void RequestSwitchPauseScenario();

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestSwitchPauseScenario();

    /** 서버에서 일시정지된 시나리오를 다시 재생하도록 지시하는 서버 RPC */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestResumeScenario();

    /** 클라이언트가 기록한 실습 로그 내용을 서버로 전송하여 취합하는 서버 RPC */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestLog(const FString& LogMessage);
};