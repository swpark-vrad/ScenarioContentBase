#include "Global/ScenarioPlayerStateBase.h"
#include "Global/ScenarioGameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Interface/ScenarioPlayerAppearance.h"
#include "GameFramework/Pawn.h"

AScenarioPlayerStateBase::AScenarioPlayerStateBase()
{
	bReplicates = true;
	UserIndex = -1;
	UserName = TEXT("Unknown");
	UserColor = FLinearColor::White;
}

void AScenarioPlayerStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AScenarioPlayerStateBase, UserIndex);
	DOREPLIFETIME(AScenarioPlayerStateBase, UserColor);
	DOREPLIFETIME(AScenarioPlayerStateBase, UserName);
}

void AScenarioPlayerStateBase::SetUserName(const FString& NewName)
{
	if (!HasAuthority()) return;

	UserName = NewName;

	// 리슨 서버 호스트 본인의 화면 갱신을 위해 직접 인터페이스를 호출합니다.
	APawn* MyPawn = GetPawn();
	if (IsValid(MyPawn) && MyPawn->GetClass()->ImplementsInterface(UScenarioPlayerAppearance::StaticClass()))
	{
		IScenarioPlayerAppearance::Execute_UpdatePawnName(MyPawn, UserName);
	}
}

void AScenarioPlayerStateBase::SetUserIndex_Implementation(int32 NewIndex)
{
	// 네트워크 권한 체크 (서버에서만 실행되도록 보장)
	if (!HasAuthority()) return;

	UserIndex = NewIndex;

	// 배열 인덱스 에러 방지 (중요!)
	if (ColorPreset.IsValidIndex(UserIndex))
	{
		UserColor = ColorPreset[UserIndex];
	}
	else
	{
		// 프리셋 범위를 벗어나는 인덱스가 오면 기본 흰색이나 무작위 색상 처리
		UserColor = FLinearColor::White;
		UE_LOG(LogTemp, Warning, TEXT("ScenarioPS: UserIndex [%d]가 ColorPreset 범위를 벗어났습니다!"), UserIndex);
	}

	// 호스트 본인 화면용 외형 업데이트
	APawn* MyPawn = GetPawn();
	if (IsValid(MyPawn) && MyPawn->GetClass()->ImplementsInterface(UScenarioPlayerAppearance::StaticClass()))
	{
		IScenarioPlayerAppearance::Execute_UpdatePawnIdentity(MyPawn, UserIndex, UserColor);
	}

	// [서버단 조치] 인덱스 설정이 완료되었으므로 GameState에게 위젯을 갱신하라고 알립니다.
	if (AScenarioGameStateBase* GS = GetWorld()->GetGameState<AScenarioGameStateBase>())
	{
		GS->OnPlayerIdentityReady(this);
	}
}

void AScenarioPlayerStateBase::OnRep_IdentityData()
{
	// 클라이언트단 외형 업데이트
	APawn* MyPawn = GetPawn();
	if (IsValid(MyPawn) && MyPawn->GetClass()->ImplementsInterface(UScenarioPlayerAppearance::StaticClass()))
	{
		IScenarioPlayerAppearance::Execute_UpdatePawnIdentity(MyPawn, UserIndex, UserColor);
	}

	// [클라이언트단 조치] 복제된 인덱스가 안착했으므로 GameState에게 위젯을 갱신하라고 알립니다.
	if (AScenarioGameStateBase* GS = GetWorld()->GetGameState<AScenarioGameStateBase>())
	{
		GS->OnPlayerIdentityReady(this);
	}
}

void AScenarioPlayerStateBase::OnRep_UserName()
{
	// [클라이언트용] 이름만 바뀌었을 때는 오직 이름 변경 인터페이스만 실행 (텍스트 컴포넌트만 갱신)
	APawn* MyPawn = GetPawn();
	if (IsValid(MyPawn) && MyPawn->GetClass()->ImplementsInterface(UScenarioPlayerAppearance::StaticClass()))
	{
		IScenarioPlayerAppearance::Execute_UpdatePawnName(MyPawn, UserName);
	}
}
