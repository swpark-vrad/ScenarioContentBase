#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ScenarioPlayerStateBase.generated.h"

UCLASS()
class SCENARIOCONTENT_API AScenarioPlayerStateBase : public APlayerState
{
	GENERATED_BODY()

public:
	AScenarioPlayerStateBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 블루프린트(에디터)에서 색상 프리셋을 미리 채워넣을 수 있도록 설정합니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UserData|Config")
	TArray<FLinearColor> ColorPreset;

	// UserIndex와 UserColor는 동일한 OnRep 함수를 공유합니다.
	UPROPERTY(ReplicatedUsing = OnRep_IdentityData, BlueprintReadOnly, Category = "UserData")
	int32 UserIndex;

	UPROPERTY(ReplicatedUsing = OnRep_IdentityData, BlueprintReadOnly, Category = "UserData")
	FLinearColor UserColor;

	// UserName은 실시간 변경을 위해 독자적인 OnRep 함수를 사용합니다.
	UPROPERTY(ReplicatedUsing = OnRep_UserName, BlueprintReadOnly, Category = "UserData")
	FString UserName;

	/** 블루프린트에서 호출도 가능하고 오버라이드도 가능하도록 매크로를 변경합니다. */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, BlueprintNativeEvent, Category = "Scenario|User")
	void SetUserIndex(int32 NewIndex);

	/** BlueprintNativeEvent의 실제 C++ 본문이 될 구현부 함수입니다. */
	virtual void SetUserIndex_Implementation(int32 NewIndex);

	/** 서버에서 유저 이름을 변경할 때 호출할 함수 */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "UserData")
	void SetUserName(const FString& NewName);

protected:
	/** 인덱스와 색상 정보가 복제되었을 때 호출되는 함수 (초기 설정용) */
	UFUNCTION()
	void OnRep_IdentityData();

	/** 이름이 변경되었을 때 호출되는 함수 (실시간 갱신용) */
	UFUNCTION()
	void OnRep_UserName();

};