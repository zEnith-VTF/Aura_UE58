// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/AURAGameModeBase.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameMode/AuraGameInstance.h"
#include "GameFramework/PlayerStart.h"
#include "GameMode/LoadScreenSaveGame.h"
#include "HAL/PlatformTime.h"
#include "Interfaction/SaveInterface.h"
#include "Kismet/GameplayStatics.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UI/ViewModel/MVVM_LoadSlot.h"

void AAURAGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	Maps.Add(DefaultMapName, DefaultMap);
}

void AAURAGameModeBase::SaveSlotData(UMVVM_LoadSlot* LoadSlot, int32 SlotIndex)
{
	if (!IsValid(LoadSlot) || !LoadScreenSaveGameClass)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveSlotData requires a load slot and LoadScreenSaveGameClass"));
		return;
	}
	ULoadScreenSaveGame* LoadScreenSaveGame = Cast<ULoadScreenSaveGame>(
		UGameplayStatics::CreateSaveGameObject(LoadScreenSaveGameClass));
	if (!LoadScreenSaveGame)
	{
		UE_LOG(LogTemp, Error, TEXT("SaveSlotData could not create a LoadScreenSaveGame from %s"),
			*GetNameSafe(LoadScreenSaveGameClass.Get()));
		return;
	}
	if (UGameplayStatics::DoesSaveGameExist(LoadSlot->LoadSlotName, SlotIndex))
	{
		UGameplayStatics::DeleteGameInSlot(LoadSlot->LoadSlotName, SlotIndex);
	}
	LoadScreenSaveGame->PlayerName = LoadSlot->GetPlayerName();
	LoadScreenSaveGame->MapName = LoadSlot->GetMapName();
	LoadScreenSaveGame->PlayerStartTag = LoadSlot->PlayerStartTag;
	LoadScreenSaveGame->PlayerLevel = LoadSlot->GetPlayerLevel();
	LoadScreenSaveGame->SaveSlotStatus = Taken;

	if (!UGameplayStatics::SaveGameToSlot(LoadScreenSaveGame, LoadSlot->LoadSlotName, SlotIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("SaveSlotData failed to write slot %s (%d)"),
			*LoadSlot->LoadSlotName, SlotIndex);
	}
}

ULoadScreenSaveGame* AAURAGameModeBase::GetSaveSlotData(const FString& SlotName, int32 SlotIndex) const
{
	USaveGame* SaveGameObject = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SlotIndex))
	{
		SaveGameObject = UGameplayStatics::LoadGameFromSlot(SlotName, SlotIndex);
	}
	else
	{
		SaveGameObject = UGameplayStatics::CreateSaveGameObject(LoadScreenSaveGameClass);
	}
	ULoadScreenSaveGame* LoadScreenSaveGame = Cast<ULoadScreenSaveGame>(SaveGameObject);
	if (!LoadScreenSaveGame)
	{
		UE_LOG(LogTemp, Error, TEXT("GetSaveSlotData could not read slot %s (%d); check LoadScreenSaveGameClass or save file"),
			*SlotName, SlotIndex);
	}
	return LoadScreenSaveGame;
}

void AAURAGameModeBase::DeleteSlot(const FString& SlotName, int32 SlotIndex)
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SlotIndex))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, SlotIndex);
	}
}

void AAURAGameModeBase::TravelToMap(UMVVM_LoadSlot* Slot)
{
	UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance());
	AuraGameInstance->PlayerStartTag = Slot->PlayerStartTag;
	AuraGameInstance->LoadSlotName = Slot->LoadSlotName;
	AuraGameInstance->LoadSlotIndex = Slot->SlotIndex;

	UGameplayStatics::OpenLevelBySoftObjectPtr(Slot, Maps.FindChecked(Slot->GetMapName()));
}

AActor* AAURAGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance());

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Actors);
	if (Actors.Num() > 0)
	{
		AActor* SelectedActor = Actors[0];
		for (AActor* Actor : Actors)
		{
			if (APlayerStart* PlayerStart = Cast<APlayerStart>(Actor))
			{
				if (PlayerStart->PlayerStartTag == AuraGameInstance->PlayerStartTag)
				{
					SelectedActor = PlayerStart;
					break;
				}
			}
		}
		return SelectedActor;
	}
	return nullptr;
}

ULoadScreenSaveGame* AAURAGameModeBase::RetrieveInGameSaveData()
{
	UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance());
	if (!AuraGameInstance || AuraGameInstance->LoadSlotName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("RetrieveInGameSaveData requires an active save slot in AuraGameInstance"));
		return nullptr;
	}

	const FString InGameLoadSlotName = AuraGameInstance->LoadSlotName;
	const int32 InGameLoadSlotIndex = AuraGameInstance->LoadSlotIndex;

	return GetSaveSlotData(InGameLoadSlotName, InGameLoadSlotIndex);
}

bool AAURAGameModeBase::SaveInGameProgressData(ULoadScreenSaveGame* SaveObject)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Aura_SaveInGameProgressData);
	UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance());
	if (!AuraGameInstance || !IsValid(SaveObject) || AuraGameInstance->LoadSlotName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveInGameProgressData requires an active save slot and save object"));
		return false;
	}

	const FString InGameLoadSlotName = AuraGameInstance->LoadSlotName;
	const int32 InGameLoadSlotIndex = AuraGameInstance->LoadSlotIndex;

	const double WriteSlotStartTime = FPlatformTime::Seconds();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Aura_SaveInGameProgressData_WriteSlot);
		const bool bSavedToSlot = UGameplayStatics::SaveGameToSlot(SaveObject, InGameLoadSlotName, InGameLoadSlotIndex);
		const double WriteSlotMs = (FPlatformTime::Seconds() - WriteSlotStartTime) * 1000.0;
		UE_LOG(LogTemp, Log, TEXT("Aura timing: SaveInGameProgressData WriteSlot=%.3fms Slot=%s Result=%s"),
			WriteSlotMs, *InGameLoadSlotName, bSavedToSlot ? TEXT("Success") : TEXT("Failure"));
		if (!bSavedToSlot)
		{
			UE_LOG(LogTemp, Error, TEXT("SaveInGameProgressData failed to write slot %s (%d)"),
				*InGameLoadSlotName, InGameLoadSlotIndex);
			return false;
		}
	}
	AuraGameInstance->PlayerStartTag = SaveObject->PlayerStartTag;
	return true;
}

FString AAURAGameModeBase::GetMapNameFromMapAssetName(const FString& MapAssetName) const
{
	for (const TPair<FString, TSoftObjectPtr<UWorld>>& Map : Maps)
	{
		if (Map.Value.ToSoftObjectPath().GetAssetName() == MapAssetName)
		{
			return Map.Key;
		}
	}

	return FString();
}

bool AAURAGameModeBase::SaveWorldState(UWorld* World, const FString& DestinationMapAssetName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Aura_SaveWorldState);
	if (!IsValid(World)) return false;

	FString WorldName = World->GetMapName();
	WorldName.RemoveFromStart(World->StreamingLevelsPrefix);

	UAuraGameInstance* AuraGI = Cast<UAuraGameInstance>(GetGameInstance());
	if (!AuraGI || AuraGI->LoadSlotName.IsEmpty() ||
		!UGameplayStatics::DoesSaveGameExist(AuraGI->LoadSlotName, AuraGI->LoadSlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveWorldState skipped for %s: no active existing save slot"), *WorldName);
		return false;
	}

	const FString LoadSlotName = AuraGI->LoadSlotName;
	const int32 LoadSlotIndex = AuraGI->LoadSlotIndex;

	const double ReadSlotStartTime = FPlatformTime::Seconds();
	ULoadScreenSaveGame* SaveGame = nullptr;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Aura_SaveWorldState_ReadSlot);
		SaveGame = GetSaveSlotData(LoadSlotName, LoadSlotIndex);
	}
	const double ReadSlotMs = (FPlatformTime::Seconds() - ReadSlotStartTime) * 1000.0;

	if (SaveGame)
	{
		double SerializeMs = 0.0;
		int32 SerializedActorCount = 0;

		if (!DestinationMapAssetName.IsEmpty())
		{
			SaveGame->MapAssetName = DestinationMapAssetName;
			SaveGame->MapName = GetMapNameFromMapAssetName(DestinationMapAssetName);
		}

		if (!SaveGame->HasMap(WorldName))
		{
			FSavedMap NewSavedMap;
			NewSavedMap.MapAssetName = WorldName;
			SaveGame->SavedMaps.Add(NewSavedMap);
		}

		FSavedMap SavedMap = SaveGame->GetSavedMapWithMapName(WorldName);
		SavedMap.SavedActors.Empty(); // clear it out, we'll fill it in with "actors"

		const double SerializeStartTime = FPlatformTime::Seconds();
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Aura_SaveWorldState_Serialize);
			for (FActorIterator It(World); It; ++It)
			{
				AActor* Actor = *It;

				if (!IsValid(Actor) || !Actor->Implements<USaveInterface>()) continue;

				FSavedActor SavedActor;
				SavedActor.ActorName = Actor->GetFName();
				SavedActor.Transform = Actor->GetTransform();

				FMemoryWriter MemoryWriter(SavedActor.Bytes);

				FObjectAndNameAsStringProxyArchive Archive(MemoryWriter, true);
				Archive.ArIsSaveGame = true;

				Actor->Serialize(Archive);

				SavedMap.SavedActors.AddUnique(SavedActor);
				SerializedActorCount++;
			}
		}
		SerializeMs = (FPlatformTime::Seconds() - SerializeStartTime) * 1000.0;

		for (FSavedMap& MapToReplace : SaveGame->SavedMaps)
		{
			if (MapToReplace.MapAssetName == WorldName)
			{
				MapToReplace = SavedMap;
			}
		}

		const double WriteSlotStartTime = FPlatformTime::Seconds();
		bool bSavedToSlot = false;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Aura_SaveWorldState_WriteSlot);
			bSavedToSlot = UGameplayStatics::SaveGameToSlot(SaveGame, LoadSlotName, LoadSlotIndex);
		}
		const double WriteSlotMs = (FPlatformTime::Seconds() - WriteSlotStartTime) * 1000.0;

		UE_LOG(LogTemp, Log,
			TEXT("Aura timing: SaveWorldState World=%s ReadSlot=%.3fms Serialize=%.3fms WriteSlot=%.3fms Actors=%d WriteResult=%s"),
			*WorldName, ReadSlotMs, SerializeMs, WriteSlotMs, SerializedActorCount,
			bSavedToSlot ? TEXT("Success") : TEXT("Failure"));

		if (!bSavedToSlot)
		{
			UE_LOG(LogTemp, Error, TEXT("SaveWorldState failed to write slot %s (%d) for %s"),
				*LoadSlotName, LoadSlotIndex, *WorldName);
			return false;
		}
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SaveWorldState could not load slot %s (%d)"),
			*LoadSlotName, LoadSlotIndex);
		return false;
	}
}

void AAURAGameModeBase::LoadWorldState(UWorld* World)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Aura_LoadWorldState);
	if (!IsValid(World)) return;

	const UAuraGameInstance* AuraGI = Cast<UAuraGameInstance>(GetGameInstance());
	if (!AuraGI || AuraGI->LoadSlotName.IsEmpty() ||
		!UGameplayStatics::DoesSaveGameExist(AuraGI->LoadSlotName, AuraGI->LoadSlotIndex)) return;

	const FString LoadSlotName = AuraGI->LoadSlotName;
	const int32 LoadSlotIndex = AuraGI->LoadSlotIndex;

	const double ReadSlotStartTime = FPlatformTime::Seconds();
	ULoadScreenSaveGame* SaveGame = nullptr;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Aura_LoadWorldState_ReadSlot);
		SaveGame = Cast<ULoadScreenSaveGame>(
			UGameplayStatics::LoadGameFromSlot(LoadSlotName, LoadSlotIndex));
	}
	const double ReadSlotMs = (FPlatformTime::Seconds() - ReadSlotStartTime) * 1000.0;
	if (!SaveGame)
	{
		UE_LOG(LogTemp, Log, TEXT("Aura timing: LoadWorldState ReadSlot=%.3fms Slot=%s ReadResult=Failure"),
			ReadSlotMs, *LoadSlotName);
		return;
	}

	FString WorldName = World->GetMapName();
	WorldName.RemoveFromStart(World->StreamingLevelsPrefix);
	FSavedMap* SavedMap = SaveGame->SavedMaps.FindByPredicate(
		[&WorldName](const FSavedMap& Map) { return Map.MapAssetName == WorldName; });
	if (!SavedMap)
	{
		UE_LOG(LogTemp, Log, TEXT("Aura timing: LoadWorldState World=%s ReadSlot=%.3fms Restore=not-run SavedMap=not-found"),
			*WorldName, ReadSlotMs);
		return;
	}

	double RestoreMs = 0.0;
	int32 RestoredActorCount = 0;
	const double RestoreStartTime = FPlatformTime::Seconds();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Aura_LoadWorldState_Restore);
		for (FActorIterator It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor) || !Actor->Implements<USaveInterface>()) continue;

			FSavedActor* SavedActor = SavedMap->SavedActors.FindByPredicate(
				[Actor](const FSavedActor& Entry) { return Entry.ActorName == Actor->GetFName(); });
			if (!SavedActor) continue;

			if (ISaveInterface::Execute_ShouldLoadTransform(Actor))
			{
				Actor->SetActorTransform(SavedActor->Transform);
			}

			if (!SavedActor->Bytes.IsEmpty())
			{
				FMemoryReader MemoryReader(SavedActor->Bytes);
				FObjectAndNameAsStringProxyArchive Archive(MemoryReader, true);
				Archive.ArIsSaveGame = true;
				Actor->Serialize(Archive);
			}

			ISaveInterface::Execute_LoadActor(Actor);
			RestoredActorCount++;
		}
	}
	RestoreMs = (FPlatformTime::Seconds() - RestoreStartTime) * 1000.0;

	UE_LOG(LogTemp, Log, TEXT("Aura timing: LoadWorldState World=%s ReadSlot=%.3fms Restore=%.3fms Actors=%d"),
		*WorldName, ReadSlotMs, RestoreMs, RestoredActorCount);
}

