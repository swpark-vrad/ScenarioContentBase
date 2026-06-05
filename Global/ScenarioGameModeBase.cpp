// Fill out your copyright notice in the Description page of Project Settings.


#include "Global/ScenarioGameModeBase.h"
#include "Global/ScenarioGameInstanceBase.h"
#include "Global/ScenarioGameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

AScenarioGameModeBase::AScenarioGameModeBase()
{
    // [중요] 생성자 내부에는 절대로 Super::BeginPlay()나 월드 호출 로직이 들어가면 안 됩니다.
    PrimaryActorTick.bCanEverTick = false;

    // 실습실용 전용 게임스테이트를 기본 클래스로 매핑 등록합니다
    GameStateClass = AScenarioGameStateBase::StaticClass();
}

void AScenarioGameModeBase::BeginPlay()
{
    Super::BeginPlay();

    // 호스트 자신의 로컬 GameInstance와 현재 매칭된 월드의 GameState를 참조합니다
    UScenarioGameInstanceBase* MyGI = Cast<UScenarioGameInstanceBase>(GetGameInstance());
    AScenarioGameStateBase* MyGS = GetGameState<AScenarioGameStateBase>();

    if (MyGI && MyGS)
    {
        // 1. 호스트가 메뉴 화면에서 들고 온 시나리오 ID를 가져옵니다
        FName SelectedID = MyGI->GetCurrentScenarioID();

        // 2. 공용 GameState 변수에 대입하여 네트워크를 통한 전체 클라이언트 전파를 개시합니다
        MyGS->ActiveScenarioID = SelectedID;

        // 3. [호스트 전용 로직] 호스트 자신도 로컬 에셋 폴더에서 데이터를 꺼내 메모리에 로드합니다
        MyGI->LoadScenarioToMemory();

        UE_LOG(LogTemp, Log, TEXT("ScenarioGM: 호스트단 내장 시나리오 [%s] 메모리 적재 완수 및 복제 네트워크 패킷 송신"), *SelectedID.ToString());

        // 호스트의 로컬 위젯들에게 데이터 준비가 끝났음을 알립니다.
        MyGS->NotifyDataReadyToLocalClients();

        // 준비가 완료되었다면 시나리오 데이터 로드
        MyGS->BuildGlobalScenarioEnvironment();
    }
}

UClass* AScenarioGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    APlayerController* PC = Cast<APlayerController>(InController);
    if (PC)
    {
        // GetLocalPlayer()가 유효하다면 이 게임 프로세스(호스트 서버)에 물리적으로 직접 연결된 로컬 마스터 유저입니다
        if (PC->GetLocalPlayer() != nullptr)
        {
            return PCPawnClass ? *PCPawnClass : Super::GetDefaultPawnClassForController_Implementation(InController);
        }
        else
        {
            // GetLocalPlayer()가 nullptr라면 네트워크 소켓 소통을 통해 외부에서 접속한 원격 클라이언트(퀘스트 유저)입니다
            return VRPawnClass ? *VRPawnClass : Super::GetDefaultPawnClassForController_Implementation(InController);
        }
    }

    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

AActor* AScenarioGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
    APlayerController* PC = Cast<APlayerController>(Player);
    if (PC)
    {
        // GetLocalPlayer() 대신 IsLocalPlayerController()를 사용하여 
        // 물리적으로 이 서버 프로세스를 구동 중인 로컬 호스트 유저인지 판별합니다.
        const bool bIsHost = PC->IsLocalPlayerController();
        FName TargetTag = bIsHost ? FName(TEXT("HostStart")) : FName(TEXT("VRStart"));

        // 일치하는 PlayerStart 액터 검색
        for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
        {
            APlayerStart* StartSpot = *It;
            if (StartSpot && StartSpot->PlayerStartTag == TargetTag)
            {
                return StartSpot;
            }
        }

        // [디버깅 로그] 만약 에디터 설정 오류로 태그를 못 찾았다면 출력 로그 창에 경고를 띄웁니다.
        UE_LOG(LogTemp, Warning, TEXT("ScenarioGM: [%s] 태그를 가진 PlayerStart를 레벨에서 찾을 수 없습니다. 기본 스폰 규칙을 적용합니다."), *TargetTag.ToString());
    }

    return Super::ChoosePlayerStart_Implementation(Player);
}
