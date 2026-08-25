#include "Global/ScenarioBuildSubsystem.h"
#include "Data/ScenarioBuilderSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

void UScenarioBuildSubsystem::SetEntryCultureTextData(const TMap<FName, FText>& InCultureTextData)
{
	EntryCultureTextData = InCultureTextData;

}

FText UScenarioBuildSubsystem::GetEntryCultureText(FName EntryRowName) const
{
	if (const FText* FoundText = EntryCultureTextData.Find(EntryRowName))
	{
		return *FoundText;
	}
	return FText::GetEmpty();
}

const FScenarioSaveData& UScenarioBuildSubsystem::GetScenarioSaveData() const
{
	return ScenarioSaveData;
}

bool UScenarioBuildSubsystem::GetPhaseData(FName PhaseID, FPhaseSaveData& OutPhaseData) const
{
	// 맵에서 PhaseID로 데이터를 찾습니다.
	if (const FPhaseEditData* FoundData = ActivePhaseData.Find(PhaseID))
	{
		// 데이터가 존재하면 Out 매개변수에 복사하여 넘겨주고 true를 반환합니다.
		OutPhaseData = FoundData->PhaseData;
		return true;
	}

	// 찾지 못했다면 false를 반환하여 위젯이 예외 처리를 할 수 있게 돕습니다.
	return false;
}

bool UScenarioBuildSubsystem::GetEntryData(FName EntryID, FEntrySaveData& OutEntryData) const
{
	if (const FEntrySaveData* FoundData = ActiveEntryData.Find(EntryID))
	{
		OutEntryData = *FoundData; // FEntrySaveData 원본 데이터 전달
		return true;
	}
	return false;
}

bool UScenarioBuildSubsystem::GetEntriesInPhase(FName PhaseID, TArray<FName>& OutEntryIDs) const
{
	if (const FPhaseEditData* FoundData = ActivePhaseData.Find(PhaseID))
	{
		// 페이즈가 가진 엔트리 목록(ContainEntries)만 쏙 뽑아서 전달합니다.
		OutEntryIDs = FoundData->ContainEntries;
		return true;
	}
	return false;
}


void UScenarioBuildSubsystem::SetScenarioID(FName ScenarioID)
{
	ScenarioSaveData.ScenarioID = ScenarioID;
	OnScenarioMetaUpdated.Broadcast();
}

void UScenarioBuildSubsystem::SetScenarioDescription(FText Description)
{
	ScenarioSaveData.Description = Description;
	OnScenarioMetaUpdated.Broadcast();
}

void UScenarioBuildSubsystem::SetPatientBaseInfoConfig(const FPatientBaseInfoConfig& Config)
{
	ScenarioSaveData.PatientInfoConfig = Config;
	OnPatientInfoUpdated.Broadcast();
}

void UScenarioBuildSubsystem::SetInitVitalSign(const FVitalSign& VitalSign)
{
	ScenarioSaveData.InitVitalSign = VitalSign;
	OnVitalSignUpdated.Broadcast();
}

void UScenarioBuildSubsystem::SetPatientPartState(const FPatientPartState& PartState)
{
	ScenarioSaveData.PatientPartState = PartState;
	OnPatientBodyPartUpdated.Broadcast();
}

FName UScenarioBuildSubsystem::AddNewPhase()
{
	// 고유 페이즈ID 생성 후 맵에 추가
	FName UniquePhaseID;
	do
	{
		UniquePhaseID = FName(*FString::Printf(TEXT("Phase%d"), PhaseIdCounter++));
	} while (ActivePhaseData.Contains(UniquePhaseID)); // 혹시 중복될 경우 다시 반복
	
	FPhaseEditData NewPhaseEditData; 
	// 초기 페이즈 이름은 페이즈ID로 설정
	NewPhaseEditData.PhaseData.PhaseName = UniquePhaseID;

	//  맵에 페이즈 데이터 추가
	ActivePhaseData.Add(UniquePhaseID, NewPhaseEditData);

	OnPhaseAdded.Broadcast(UniquePhaseID);

	return UniquePhaseID;
}

bool UScenarioBuildSubsystem::RemovePhase(FName PhaseID)
{
	// 존재하지 않는 페이즈 예외처리
	if (!ActivePhaseData.Contains(PhaseID)) return false;

	// 삭제하려는 페이즈가 현재 시작 노드에 연결되어 있다면 변수 설정
	if (StartPhaseID == PhaseID)
	{
		StartPhaseID = NAME_None;
	}

	// 엔트리를 관리하는 맵에서 삭제
	const FPhaseEditData& PhaseToDelete = ActivePhaseData[PhaseID];
	for (const FName& EntryID : PhaseToDelete.ContainEntries)
	{
		ActiveEntryData.Remove(EntryID);

		OnEntryRemoved.Broadcast(PhaseID, EntryID);
	}

	// 삭제하려는 페이즈를 참조하는 다른 페이즈들의 연결 초기화
	for (auto& Pair : ActivePhaseData)
	{
		FPhaseEditData& CurrentPhaseEditData = Pair.Value;
		bool bConnectionChanged = false;

		if (CurrentPhaseEditData.PhaseData.NextFailurePhaseName == PhaseID)
		{
			CurrentPhaseEditData.PhaseData.NextFailurePhaseName = NAME_None;
			bConnectionChanged = true;
		}

		if (CurrentPhaseEditData.PhaseData.NextSuccessPhaseName == PhaseID)
		{
			CurrentPhaseEditData.PhaseData.NextSuccessPhaseName = NAME_None;
			bConnectionChanged = true;
		}

		// 선이 끊어졌으므로 해당 페이즈 UI를 업데이트하라고 브로드캐스트
		if (bConnectionChanged)
		{
			OnPhaseUpdated.Broadcast(Pair.Key);
		}
	}

	// 맵에서 삭제
	ActivePhaseData.Remove(PhaseID);

	OnPhaseRemoved.Broadcast(PhaseID);

	return true;
}

bool UScenarioBuildSubsystem::SetPhaseName(FName TargetPhaseID, FName NewPhaseName)
{
	// 맵의 Key(TargetPhaseID)를 통해 O(1)로 데이터를 즉시 찾습니다.
	if (!ActivePhaseData.Contains(TargetPhaseID)) return false;

	FPhaseEditData& TargetPhase = ActivePhaseData[TargetPhaseID];
	FName OldPhaseName = TargetPhase.PhaseData.PhaseName;

	// 기존 이름과 동일하면 무시
	if (OldPhaseName == NewPhaseName) return true;

	// 1. 데이터 업데이트 (맵의 키는 건드리지 않고, 내부 사용자 이름만 변경)
	TargetPhase.PhaseData.PhaseName = NewPhaseName;

	// 2. [핵심] 다른 페이즈들이 기존 '사용자 이름'으로 연결되어 있었다면, 새 이름으로 갱신해 줍니다.
	for (auto& Pair : ActivePhaseData)
	{
		FPhaseEditData& CurrentPhase = Pair.Value;
		bool bConnectionChanged = false;

		if (CurrentPhase.PhaseData.NextFailurePhaseName == OldPhaseName)
		{
			CurrentPhase.PhaseData.NextFailurePhaseName = NewPhaseName;
			bConnectionChanged = true;
		}
		if (CurrentPhase.PhaseData.NextSuccessPhaseName == OldPhaseName)
		{
			CurrentPhase.PhaseData.NextSuccessPhaseName = NewPhaseName;
			bConnectionChanged = true;
		}

		if (bConnectionChanged)
		{
			// [UI 갱신] 참조가 바뀐 다른 페이즈들도 새로고침
			OnPhaseUpdated.Broadcast(Pair.Key);
		}
	}

	return true;
}

void UScenarioBuildSubsystem::SetPhaseDuration(FName TargetPhaseID, float NewDuration)
{
	if (FPhaseEditData* FoundPhase = ActivePhaseData.Find(TargetPhaseID))
	{
		FoundPhase->PhaseData.TimeLimit = NewDuration;
		OnPhaseUpdated.Broadcast(TargetPhaseID);
	}
}

void UScenarioBuildSubsystem::SetNextPhase_Success(FName TargetPhaseID, FName NextPhaseID)
{
	if (FPhaseEditData* FoundTarget = ActivePhaseData.Find(TargetPhaseID))
	{
		FName NextPhaseNameToSave = NAME_None;

		// 1. NextPhaseID가 None이 아니라면, 맵을 뒤져서 실제 PhaseName을 추출합니다.
		if (NextPhaseID != NAME_None)
		{
			if (const FPhaseEditData* FoundNext = ActivePhaseData.Find(NextPhaseID))
			{
				NextPhaseNameToSave = FoundNext->PhaseData.PhaseName;
			}
		}

		// 2. 찾아낸 실제 PhaseName으로 타겟 데이터의 변수를 덮어씌웁니다.
		FoundTarget->PhaseData.NextSuccessPhaseName = NextPhaseNameToSave;

		// 3. UI 갱신 알림 (출발지 노드 아웃풋 핀 갱신)
		OnPhaseConnectionChanged.Broadcast(TargetPhaseID);

		// 4. 새 도착지 노드 인풋 핀 갱신
		if (NextPhaseID != NAME_None)
		{
			OnPhaseConnectionChanged.Broadcast(NextPhaseID);
		}
	}
}

void UScenarioBuildSubsystem::SetNextPhase_Fail(FName TargetPhaseID, FName NextPhaseID)
{
	if (FPhaseEditData* FoundTarget = ActivePhaseData.Find(TargetPhaseID))
	{
		FName NextPhaseNameToSave = NAME_None;

		// 1. NextPhaseID가 None이 아니라면, 맵을 뒤져서 실제 PhaseName을 추출합니다.
		if (NextPhaseID != NAME_None)
		{
			if (const FPhaseEditData* FoundNext = ActivePhaseData.Find(NextPhaseID))
			{
				NextPhaseNameToSave = FoundNext->PhaseData.PhaseName;
			}
		}

		// 2. 찾아낸 실제 PhaseName으로 타겟 데이터의 변수를 덮어씌웁니다.
		FoundTarget->PhaseData.NextFailurePhaseName = NextPhaseNameToSave;

		// 3. UI 갱신 알림 (출발지 노드 아웃풋 핀 갱신)
		OnPhaseConnectionChanged.Broadcast(TargetPhaseID);

		// 4. 새 도착지 노드 인풋 핀 갱신
		if (NextPhaseID != NAME_None)
		{
			OnPhaseConnectionChanged.Broadcast(NextPhaseID);
		}
	}
}

void UScenarioBuildSubsystem::SetNextPhase_End(FName TargetPhaseID, bool bIsSuccessPin)
{
	if (FPhaseEditData* FoundTarget = ActivePhaseData.Find(TargetPhaseID))
	{
		// (선택 사항) 기존에 연결된 일반 페이즈가 있었다면, 해당 페이즈의 선이 끊어졌음을 알리는 방어 로직 추가 가능

		// 1. 어떤 핀에서 왔는지에 따라 종료 예약어(이름)를 바로 꽂아줍니다.
		if (bIsSuccessPin)
		{
			FoundTarget->PhaseData.NextSuccessPhaseName = FName("End");
		}
		else
		{
			FoundTarget->PhaseData.NextFailurePhaseName = FName("End");
		}

		// 2. 출발지 노드(TargetPhaseID) UI 갱신 (아웃풋 핀 상태 켜기/선 그리기 갱신)
		OnPhaseConnectionChanged.Broadcast(TargetPhaseID);

		// * 주의: End 노드는 ActivePhaseData에 없으므로 인풋 핀 UI 갱신을 위해 Broadcast(NextPhaseID)를 하지 않습니다.
	}
}

void UScenarioBuildSubsystem::SetStartPhaseID(FName NewStartPhaseID)
{
	FName OldStartPhase = StartPhaseID;

	// 1. 에디터 런타임 변수 갱신
	StartPhaseID = NewStartPhaseID;

	// 2. 기존 타겟 연결 해제 알림 (과거 노드의 인풋 핀 끄기)
	if (OldStartPhase != NAME_None && OldStartPhase != NewStartPhaseID)
	{
		OnPhaseConnectionChanged.Broadcast(OldStartPhase);
	}

	// 3. 새 타겟 연결 알림 (새 노드의 인풋 핀 켜기)
	if (NewStartPhaseID != NAME_None)
	{
		OnPhaseConnectionChanged.Broadcast(NewStartPhaseID);
	}
}

void UScenarioBuildSubsystem::SetPhaseVSModOp(FName TargetPhaseID, const FScenarioVitalModifier& VSModOp)
{
	if (FPhaseEditData* FoundPhase = ActivePhaseData.Find(TargetPhaseID))
	{
		FoundPhase->PhaseData.VitalModifier = VSModOp;
		OnPhaseUpdated.Broadcast(TargetPhaseID); // [UI 갱신]
	}
}

void UScenarioBuildSubsystem::ClearFailureConnection(FName PhaseID)
{
	if (FPhaseEditData* PhaseEditData = ActivePhaseData.Find(PhaseID))
	{
		// Fail 경로를 끊어버림 (NAME_None 설정)
		PhaseEditData->PhaseData.NextFailurePhaseName = NAME_None;
		OnPhaseConnectionChanged.Broadcast(PhaseID);
	}
}

void UScenarioBuildSubsystem::GetPhaseConnectionStates(FName PhaseID, bool& bOutInputConnected, bool& bOutSuccessConnected, bool& bOutFailConnected) const
{
	// 기본값 초기화
	bOutInputConnected = false;
	bOutSuccessConnected = false;
	bOutFailConnected = false;

	const FPhaseEditData* TargetData = ActivePhaseData.Find(PhaseID);
	if (!TargetData)
	{
		return;
	}

	FName TargetPhaseName = TargetData->PhaseData.PhaseName;

	// 1. 출력 핀(Output) 연결 상태 확인
	// NAME_None이 아니라면 유효한 대상(다른 페이즈 또는 "End")과 연결된 것으로 간주
	bOutSuccessConnected = (TargetData->PhaseData.NextSuccessPhaseName != NAME_None);
	bOutFailConnected = (TargetData->PhaseData.NextFailurePhaseName != NAME_None);

	// 2. 입력 핀(Input) 연결 상태 확인 (역추적)

	// 2-1. 이 노드가 전체 시작(Start) 노드에 직접 연결되어 있는지 확인
	if (StartPhaseID == PhaseID)
	{
		bOutInputConnected = true;
		return; // 인풋이 연결된 걸 확인했으니 더 순회할 필요 없음
	}

	// 2-2. 다른 일반 노드들 중 누군가 '내 이름'을 가리키고 있는지 순회하며 확인
	for (const auto& Pair : ActivePhaseData)
	{
		if (Pair.Key == PhaseID) continue; // 자기 자신은 건너뜀

		if (Pair.Value.PhaseData.NextSuccessPhaseName == TargetPhaseName ||
			Pair.Value.PhaseData.NextFailurePhaseName == TargetPhaseName)
		{
			bOutInputConnected = true;
			break; // 하나라도 나를 가리키는 걸 찾았다면 즉시 순회 종료
		}
	}
}

bool UScenarioBuildSubsystem::IsValidStartPhase() const
{
	// 1. 이름이 비어있지 않은지 검사
	if (StartPhaseID == NAME_None)
	{
		return false;
	}

	// 2. [무결성 핵심] 해당 ID가 삭제되지 않고 맵에 실제로 존재하는 유효한 페이즈인지 검사
	return ActivePhaseData.Contains(StartPhaseID);
}

bool UScenarioBuildSubsystem::IsEndConnected() const
{
	// 등록된 전체 페이즈 중 하나라도 NextPhase가 "End"로 설정되어 있는지 순회 검사
	for (const auto& Pair : ActivePhaseData)
	{
		if (Pair.Value.PhaseData.NextSuccessPhaseName == FName("End") ||
			Pair.Value.PhaseData.NextFailurePhaseName == FName("End"))
		{
			return true;
		}
	}
	return false;
}

bool UScenarioBuildSubsystem::HasMandatoryEntries(FName PhaseID) const
{
	// 해당 페이즈가 존재하는지 검사
	if (!ActivePhaseData.Contains(PhaseID))
	{
		return false;
	}

	// 페이즈가 가지고 있는 엔트리 ID 리스트 순회
	for (FName EntryID : ActivePhaseData[PhaseID].ContainEntries)
	{
		// 실제 엔트리 데이터에서 필수(Mandatory) 여부 확인
		if (const FEntrySaveData* EntryData = ActiveEntryData.Find(EntryID))
		{
			if (EntryData->bIsMandatory)
			{
				// 필수가 단 하나라도 발견되면 즉시 true 반환
				return true;
			}
		}
	}

	// 끝까지 확인했는데 없으면 false
	return false;
}

TArray<FValidationResult> UScenarioBuildSubsystem::ValidateScenario()
{
	TArray<FValidationResult> CurrentErrors;

	// 1. 각 헬퍼 함수들을 차례대로 호출하여 CurrentErrors 배열을 채워 넣습니다.
	CheckFlowErrors(CurrentErrors);
	CheckPhaseErrors(CurrentErrors);
	CheckEntryErrors(CurrentErrors);
	CheckMetaDataErrors(CurrentErrors);

	// 2. 검사가 끝났으므로, 에러 목록 UI(이슈 트래커)와 노드 위젯들에게 결과를 방송합니다.
	OnValidationUpdated.Broadcast(CurrentErrors);

	return CurrentErrors;
}

void UScenarioBuildSubsystem::ResetScenario()
{
	// 1. 내부 메모리 데이터 완벽 초기화
	ScenarioSaveData = FScenarioSaveData(); // 구조체를 빈 상태(기본값)로 덮어씌움

	ActivePhaseData.Empty();
	ActiveEntryData.Empty();

	StartPhaseID = NAME_None;
	CurrentSaveSlotName = TEXT(""); // 슬롯 이름을 비워서 다음 저장 시 새 GUID를 발급받도록 유도

	// 새 페이즈/엔트리 생성 시 번호가 다시 0번부터 시작하도록 카운터 초기화
	PhaseIdCounter = 0;
	EntryIdCounter = 0;

	// 그래프 위젯 갱신 (바탕화면에 깔려있는 기존 WBP_Node 위젯들 및 연결선 싹 지우기)
	OnScenarioReset.Broadcast();
}

bool UScenarioBuildSubsystem::SaveScenario(const TMap<FName, FVector2D>& NodePositions, FVector2D InStartNodePos, FVector2D InEndNodePos)
{
	// 1. 세이브 게임 객체 생성
	UScenarioBuilderSaveGame* SaveObject = Cast<UScenarioBuilderSaveGame>(UGameplayStatics::CreateSaveGameObject(UScenarioBuilderSaveGame::StaticClass()));

	if (!SaveObject)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveScenario: SaveGame 객체 생성에 실패했습니다."));
		return false;
	}

	// 2. 세이브슬롯 이름 설정
	// 설정이 안되어 있다면 고유이름 생성
	if (CurrentSaveSlotName.IsEmpty())
	{
		CurrentSaveSlotName = TEXT("Scenario_") + FGuid::NewGuid().ToString();
	}
	SaveObject->SaveSlotName = CurrentSaveSlotName;

	// 3. 기본 시나리오 데이터 저장 (BaseScenarioData)
	// 서브시스템에 캐싱되어 있는 기본 설정들을 덮어씌웁니다.
	SaveObject->BaseScenarioData.ScenarioID = ScenarioSaveData.ScenarioID;
	SaveObject->BaseScenarioData.Description = ScenarioSaveData.Description;
	SaveObject->BaseScenarioData.InitVitalSign = ScenarioSaveData.InitVitalSign;
	SaveObject->BaseScenarioData.PatientInfoConfig = ScenarioSaveData.PatientInfoConfig;
	SaveObject->BaseScenarioData.PatientPartState = ScenarioSaveData.PatientPartState;
	// 시작페이즈의 경우 PhaseID를 캐싱했기 때문에 PhaseSaveData를 참조하여 
	// 해당 데이터의 PhaseName을 참조
	FPhaseSaveData StartPhaseData;
	if (GetPhaseData(StartPhaseID, StartPhaseData))
	{
		SaveObject->BaseScenarioData.StartPhaseName = StartPhaseData.PhaseName;
	}
	else
	{
		// StartPhaseID를 설정하지 않았을 경우 None으로 설정
		SaveObject->BaseScenarioData.StartPhaseName = NAME_None;
	}

	// 4. 페이즈 데이터와 노드 위치 데이터 병합 및 저장
	for (const auto& Pair : ActivePhaseData)
	{
		FName PhaseID = Pair.Key;
		FPhaseNodeSaveData NewNodeData;
		// 실제 PhaseData 복사
		NewNodeData.PhaseData = Pair.Value.PhaseData;
		// PhaseData에는 기본정보만 들어있고 엔트리정보는 ActiveEntryData로 따로 관리하기때문에
		// 실제 데이터를 담아주는 작업이 필요
		NewNodeData.PhaseData.Entries.Empty();
		for (FName EntryID : Pair.Value.ContainEntries) // 에디터의 관계 리스트 순회
		{
			if (const FEntrySaveData* FoundEntry = ActiveEntryData.Find(EntryID))
			{
				NewNodeData.PhaseData.Entries.Add(*FoundEntry); // 배열에 복사본 쏙쏙 집어넣기
			}
		}
		// 위젯용 위치값 복사
		if (const FVector2D* FoundPos = NodePositions.Find(PhaseID))
		{
			NewNodeData.NodePosition = *FoundPos;
		}
		else
		{
			// UI에서 위치를 넘겨주지 않은 경우 방어 코드 (0, 0)
			NewNodeData.NodePosition = FVector2D::ZeroVector;
			UE_LOG(LogTemp, Warning, TEXT("SaveScenario: 노드 위치 데이터를 찾지 못했습니다. ID: %s"), *PhaseID.ToString());
		}

		// 세이브데이터 변수에 추가
		SaveObject->SavedNodes.Add(PhaseID, NewNodeData);
	}

	SaveObject->StartNodePosition = InStartNodePos;
	SaveObject->EndNodePosition = InEndNodePos;

	// 5. 실제 디스크에 저장(직렬화)
	bool bIsSaved = UGameplayStatics::SaveGameToSlot(SaveObject, CurrentSaveSlotName, 0);

	if (bIsSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("SaveScenario: 시나리오가 [%s.sav] 슬롯에 성공적으로 저장/덮어쓰기 되었습니다."), *CurrentSaveSlotName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SaveScenario: 디스크 쓰기에 실패했습니다."));
	}

	return bIsSaved;
}

TArray<class UScenarioBuilderSaveGame*> UScenarioBuildSubsystem::GetAllScenarioSaves()
{
	TArray<UScenarioBuilderSaveGame*> ValidSaves;

	// Saved/SaveGames 폴더 경로 탐색
	FString SaveDirectory = FPaths::ProjectSavedDir() / TEXT("SaveGames");
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *(SaveDirectory / TEXT("*.sav")), true, false);

	for (const FString& FileName : FoundFiles)
	{
		FString SlotName = FPaths::GetBaseFilename(FileName);

		// 파일을 로드하여 시나리오 빌더용 세이브 객체인지 필터링
		if (USaveGame* LoadedGame = UGameplayStatics::LoadGameFromSlot(SlotName, 0))
		{
			if (UScenarioBuilderSaveGame* ScenarioSave = Cast<UScenarioBuilderSaveGame>(LoadedGame))
			{
				ValidSaves.Add(ScenarioSave);
			}
		}
	}
	return ValidSaves;
}

bool UScenarioBuildSubsystem::LoadScenario(FString SlotNameToLoad)
{
	UScenarioBuilderSaveGame* LoadedObject = Cast<UScenarioBuilderSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotNameToLoad, 0));
	if (!LoadedObject)
	{
		return false;
	}

	// [1] 기존 서브시스템 데이터 싹 비우기 (초기화)
	ActivePhaseData.Empty();
	ActiveEntryData.Empty();
	StartPhaseID = NAME_None;

	// 카운터 세팅 (다음 페이즈/엔트리 추가 시 ID가 중복되지 않도록)
	PhaseIdCounter = LoadedObject->SavedNodes.Num();
	EntryIdCounter = 0;

	// [2] 기본 설정 데이터 및 슬롯 이름 복원
	CurrentSaveSlotName = LoadedObject->SaveSlotName;
	ScenarioSaveData = LoadedObject->BaseScenarioData;

	// [3] 방송(Broadcast)에 실어 보낼 로컬 변수 및 구조체 준비
	FLoadedNodePositionData LoadedNodeData;
	FVector2D LoadedStartNodePos = LoadedObject->StartNodePosition;
	FVector2D LoadedEndNodePos = LoadedObject->EndNodePosition;

	// [4] 페이즈 노드 데이터 및 엔트리 언패킹 복원
	for (const auto& Pair : LoadedObject->SavedNodes)
	{
		FName PhaseID = Pair.Key;
		const FPhaseNodeSaveData& NodeData = Pair.Value;

		// 논리 데이터 맵에 적재
		FPhaseEditData EditData;
		EditData.PhaseData = NodeData.PhaseData;

		// 런타임 배열(Entries)에 뭉쳐있던 엔트리들을 에디터 맵(ActiveEntryData)으로 풀어놓기
		EditData.ContainEntries.Empty();
		for (const FEntrySaveData& SavedEntry : NodeData.PhaseData.Entries)
		{
			// 새로운 에디터용 가상 ID 발급
			FName NewEntryID = FName(*FString::Printf(TEXT("Entry%d"), EntryIdCounter++));

			// 서브시스템 맵에 등록 및 관계 리스트에 추가
			ActiveEntryData.Add(NewEntryID, SavedEntry);
			EditData.ContainEntries.Add(NewEntryID);
		}

		ActivePhaseData.Add(PhaseID, EditData);

		// 방송용 구조체의 맵에 좌표 저장
		LoadedNodeData.Positions.Add(PhaseID, NodeData.NodePosition);

		// 시작 페이즈 ID 복구 (PhaseName 비교)
		if (ScenarioSaveData.StartPhaseName != NAME_None &&
			NodeData.PhaseData.PhaseName == ScenarioSaveData.StartPhaseName)
		{
			StartPhaseID = PhaseID;
		}
	}

	// [6] 그래프 UI 갱신 방송 (구조체로 포장된 좌표 데이터 전달)
	OnScenarioLoaded.Broadcast(LoadedNodeData, LoadedStartNodePos, LoadedEndNodePos);

	return true;
}

FName UScenarioBuildSubsystem::MakeUniqueEntryID()
{
	FName UniqueName;
	do
	{
		UniqueName = FName(*FString::Printf(TEXT("Entry%d"), EntryIdCounter++));
	} while (ActiveEntryData.Contains(UniqueName));

	return UniqueName;
}

void UScenarioBuildSubsystem::CheckFlowErrors(TArray<FValidationResult>& OutErrors) const
{
	// 1. 시작 노드 유효성 검사
	bool bHasStart = (StartPhaseID != NAME_None && ActivePhaseData.Contains(StartPhaseID));
	if (!bHasStart)
	{
		// 시작 노드가 없으면 치명적 오류 추가 (타겟 노드가 없으므로 NAME_None 전달)
		OutErrors.Add(FValidationResult(EValidationSeverity::Error, NAME_None, TEXT("시나리오의 시작 노드가 설정되지 않았습니다.")));
	}

	// DFS 탐색을 위한 변수 셋업
	bool bCanReachEnd = false;
	TSet<FName> ReachablePhaseNames; // 시작 노드로부터 도달 가능한 페이즈 '이름' 모음

	// 2. 도달 가능성 탐색 (DFS) - 시작 노드가 있을 때만 수행
	if (bHasStart)
	{
		TArray<FName> Stack;
		FName StartName = ActivePhaseData[StartPhaseID].PhaseData.PhaseName;
		Stack.Push(StartName);

		while (Stack.Num() > 0)
		{
			FName CurrentName = Stack.Pop();

			// 목적지(End)를 찾은 경우
			if (CurrentName == FName("End"))
			{
				bCanReachEnd = true;
				continue;
			}

			// 끊겨있거나 이미 방문한 노드면 패스
			if (CurrentName == NAME_None || ReachablePhaseNames.Contains(CurrentName))
			{
				continue;
			}

			// 방문 기록 남기기
			ReachablePhaseNames.Add(CurrentName);

			// 다음 목적지들을 스택에 추가
			for (const auto& Pair : ActivePhaseData)
			{
				if (Pair.Value.PhaseData.PhaseName == CurrentName)
				{
					Stack.Push(Pair.Value.PhaseData.NextSuccessPhaseName);
					Stack.Push(Pair.Value.PhaseData.NextFailurePhaseName);
					break;
				}
			}
		}

		// 탐색을 다 마쳤는데 End를 못 찾았다면 오류 추가
		if (!bCanReachEnd)
		{
			OutErrors.Add(FValidationResult(EValidationSeverity::Error, StartPhaseID, TEXT("시작 노드에서 End 노드로 도달할 수 있는 경로가 없습니다. (진행 불가)")));
		}
	}

	// 3. 개별 노드 상태 순회 검사 (고립, 데드엔드, Fail 핀 누락)
	for (const auto& Pair : ActivePhaseData)
	{
		FName PhaseID = Pair.Key;
		const FPhaseSaveData& PhaseData = Pair.Value.PhaseData;

		// [검사 A] 고립된 노드 (Orphan Node)
		// 시작 노드가 존재하는데, 내 이름이 ReachablePhaseNames에 없다면? -> 시작 노드랑 안 이어져 있는 잉여 노드
		if (bHasStart && PhaseID != StartPhaseID && !ReachablePhaseNames.Contains(PhaseData.PhaseName))
		{
			OutErrors.Add(FValidationResult(EValidationSeverity::Warning, PhaseID, TEXT("시작 노드로부터 연결되지 않아 실행되지 않는 고립된 노드입니다.")));
		}

		// [검사 B] 데드엔드 (Success 핀 누락)
		if (PhaseData.NextSuccessPhaseName == NAME_None)
		{
			OutErrors.Add(FValidationResult(EValidationSeverity::Error, PhaseID, TEXT("성공(Success) 시 이동할 다음 노드가 연결되지 않았습니다.")));
		}

		// [검사 C] 필수 엔트리가 있는데 Fail 핀이 누락된 경우
		if (HasMandatoryEntries(PhaseID) && PhaseData.NextFailurePhaseName == NAME_None)
		{
			OutErrors.Add(FValidationResult(EValidationSeverity::Error, PhaseID, TEXT("필수 엔트리가 포함되어 있으나, 실패(Fail) 시 이동할 노드가 연결되지 않았습니다.")));
		}
	}
}

void UScenarioBuildSubsystem::CheckPhaseErrors(TArray<FValidationResult>& OutErrors) const
{
}

void UScenarioBuildSubsystem::CheckEntryErrors(TArray<FValidationResult>& OutErrors) const
{
}

void UScenarioBuildSubsystem::CheckMetaDataErrors(TArray<FValidationResult>& OutErrors) const
{
}

FName UScenarioBuildSubsystem::GetParentPhaseID(FName EntryID) const
{
	// 모든 페이즈를 순회하면서
	for (const auto& Pair : ActivePhaseData)
	{
		// 해당 페이즈의 엔트리 목록(ContainEntries)에 이 EntryID가 있다면
		if (Pair.Value.ContainEntries.Contains(EntryID))
		{
			return Pair.Key; // 부모 페이즈 ID 반환
		}
	}
	return NAME_None; // 못 찾으면 None
}

FName UScenarioBuildSubsystem::AddNewEntry(FName OwnPhaseID, FName EntryRowName)
{
	// 유효하지 않은 페이즈에 엔트리 추가 시도 시 방어
	if (!ActivePhaseData.Contains(OwnPhaseID)) return NAME_None;

	// 1. 고유 인스턴스 ID 생성
	FName NewEntryID = MakeUniqueEntryID();

	// 2. 엔트리 초기 데이터 세팅
	FEntrySaveData NewEntry;
	NewEntry.EntryRowName = EntryRowName;
	NewEntry.bIsMandatory = false; // 기본값

	// 3. 데이터 맵 및 부모 페이즈의 관계 리스트에 추가
	ActiveEntryData.Add(NewEntryID, NewEntry);
	ActivePhaseData[OwnPhaseID].ContainEntries.Add(NewEntryID);

	OnEntryAdded.Broadcast(OwnPhaseID, NewEntryID);

	// 위젯에서 이 ID를 캐싱하여 사용할 수 있도록 반환
	return NewEntryID;
}

bool UScenarioBuildSubsystem::RemoveEntry(FName EntryID)
{
	if (!ActiveEntryData.Contains(EntryID)) return false;

	FName FoundParentPhaseID = NAME_None;
	for (auto& Pair : ActivePhaseData)
	{
		if (Pair.Value.ContainEntries.Contains(EntryID))
		{
			Pair.Value.ContainEntries.Remove(EntryID);
			FoundParentPhaseID = Pair.Key;
			break;
		}
	}

	ActiveEntryData.Remove(EntryID);

	// [UI 갱신] 삭제 처리 브로드캐스트
	if (FoundParentPhaseID != NAME_None)
	{
		OnEntryRemoved.Broadcast(FoundParentPhaseID, EntryID);
	}

	return true;
}

bool UScenarioBuildSubsystem::SetEntryRowName(FName TargetEntryID, FName NewEntryRowName)
{
	if (FEntrySaveData* FoundEntry = ActiveEntryData.Find(TargetEntryID))
	{
		if (FoundEntry->EntryRowName == NewEntryRowName) return true;

		FoundEntry->EntryRowName = NewEntryRowName;
		OnEntryUpdated.Broadcast(TargetEntryID);
		return true;
	}
	return false;
}

void UScenarioBuildSubsystem::SetEntryMandatory(FName TargetEntryID, bool bIsMandatory)
{
	// Find를 사용하여 포인터로 접근하면 맵 내부의 원본 데이터를 즉시 수정 가능합니다.
	if (FEntrySaveData* FoundEntry = ActiveEntryData.Find(TargetEntryID))
	{
		FoundEntry->bIsMandatory = bIsMandatory;
	}

	// 부모 페이즈의 필수 엔트리 보유여부를 확인하여 없을 경우 연결해제 함수를 호출
	FName ParentPhaseID = GetParentPhaseID(TargetEntryID);
	if (ParentPhaseID != NAME_None)
	{
		// 3. 만약 부모 페이즈에 필수 엔트리가 하나도 안 남았다면?
		if (!HasMandatoryEntries(ParentPhaseID))
		{
			// 서브시스템이 알아서 선을 끊고, 델리게이트(OnPhaseConnectionChanged)를 방송합니다.
			ClearFailureConnection(ParentPhaseID);
		}

		// 부모 페이즈 상태변경 방송
		OnPhaseUpdated.Broadcast(ParentPhaseID);
	}

	// 4. 원래 쏘던 엔트리 업데이트 방송
	OnEntryUpdated.Broadcast(TargetEntryID);
}

void UScenarioBuildSubsystem::SetEntryVSModOp(FName TargetEntryID, const FScenarioVitalModifier& VSModOp)
{
	if (FEntrySaveData* FoundEntry = ActiveEntryData.Find(TargetEntryID))
	{
		FoundEntry->VitalModifier = VSModOp;
		OnEntryUpdated.Broadcast(TargetEntryID);
	}
}

bool UScenarioBuildSubsystem::MoveEntryUp(FName PhaseID, FName EntryID)
{
	if (FPhaseEditData* FoundPhase = ActivePhaseData.Find(PhaseID))
	{
		// 배열에서 해당 엔트리의 현재 인덱스를 찾습니다.
		int32 CurrentIndex = FoundPhase->ContainEntries.Find(EntryID);

		// 엔트리가 존재하고, 이미 맨 앞(0번 인덱스)이 아닐 때만 이동 가능
		if (CurrentIndex > 0)
		{
			// 현재 인덱스와 그 앞의 인덱스(CurrentIndex - 1)의 위치를 맞바꿉니다.
			FoundPhase->ContainEntries.SwapMemory(CurrentIndex, CurrentIndex - 1);

			// [UI 갱신] 순서가 변경되었음을 방송합니다.
			OnEntryOrderChanged.Broadcast(PhaseID);
			return true;
		}
	}
	return false;
}

bool UScenarioBuildSubsystem::MoveEntryDown(FName PhaseID, FName EntryID)
{
	if (FPhaseEditData* FoundPhase = ActivePhaseData.Find(PhaseID))
	{
		int32 CurrentIndex = FoundPhase->ContainEntries.Find(EntryID);

		// 엔트리가 존재하고(INDEX_NONE이 아님), 배열의 맨 끝이 아닐 때만 이동 가능
		if (CurrentIndex != INDEX_NONE && CurrentIndex < FoundPhase->ContainEntries.Num() - 1)
		{
			// 현재 인덱스와 그 뒤의 인덱스(CurrentIndex + 1)의 위치를 맞바꿉니다.
			FoundPhase->ContainEntries.SwapMemory(CurrentIndex, CurrentIndex + 1);

			// [UI 갱신] 순서가 변경되었음을 방송합니다.
			OnEntryOrderChanged.Broadcast(PhaseID);
			return true;
		}
	}
	return false;
}

