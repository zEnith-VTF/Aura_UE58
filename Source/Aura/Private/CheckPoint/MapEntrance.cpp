// Fill out your copyright notice in the Description page of Project Settings.


#include "CheckPoint/MapEntrance.h"

#include "Aura/Aura.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "GameMode/AURAGameModeBase.h"
#include "GameMode/AuraGameInstance.h"
#include "GameMode/LoadScreenSaveGame.h"
#include "HAL/PlatformTime.h"
#include "Interfaction/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

AMapEntrance::AMapEntrance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->SetupAttachment(SceneRoot);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	MoveToComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MoveToComponent"));
	MoveToComponent->SetupAttachment(SceneRoot);

	ChildActor = CreateDefaultSubobject<UChildActorComponent>(TEXT("GatewayVisual"));
	ChildActor->SetupAttachment(SceneRoot);
}

void AMapEntrance::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AMapEntrance::OnSphereOverlap);
}

void AMapEntrance::HighlightActor_Implementation()
{
	SetEntranceHighlighted(true);
}

void AMapEntrance::UnHighlightActor_Implementation()
{
	SetEntranceHighlighted(false);
}

void AMapEntrance::SetEntranceHighlighted(bool bHighlighted)
{
	if (!IsValid(ChildActor)) return;

	AActor* VisualActor = ChildActor->GetChildActor();
	if (!IsValid(VisualActor) || HighlightComponentTag.IsNone()) return;

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	VisualActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents, true);
	for (UPrimitiveComponent* Component : PrimitiveComponents)
	{
		if (IsValid(Component) && Component->ComponentHasTag(HighlightComponentTag))
		{
			if (bHighlighted)
			{
				Component->SetCustomDepthStencilValue(CUSTOM_DEPTH_TAN);
			}
			Component->SetRenderCustomDepth(bHighlighted);
		}
	}
}

void AMapEntrance::SetMoveToLocation_Implementation(FVector& OutDestination)
{
	OutDestination = MoveToComponent->GetComponentLocation();
}

bool AMapEntrance::ShouldLoadTransform_Implementation()
{
	return false;
}

void AMapEntrance::LoadActor_Implementation()
{
	// Progress state does not change overlap collision or selection highlighting.
}

bool AMapEntrance::QueryInteraction_Implementation(APawn* Interactor, FAuraInteractionInfo& OutInfo)
{
	// 只读查询：只报告站位与半径，不写档、不置 bReached、不切关卡。
	OutInfo.bCanInteract = !DestinationMap.IsNull() && !DestinationPlayerStartTag.IsNone();
	OutInfo.ApproachLocation = IsValid(MoveToComponent) ? MoveToComponent->GetComponentLocation() : GetActorLocation();
	OutInfo.InteractionRange = IsValid(Sphere) ? Sphere->GetScaledSphereRadius() : 150.f;
	OutInfo.bAllowApproach = true;
	return true;
}

bool AMapEntrance::TryInteract_Implementation(APawn* Interactor)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Aura_MapEntrance_TravelRequest);
	if (bTravelRequested || !HasAuthority() || !IsValid(Interactor) ||
		!Interactor->Implements<UPlayerInterface>()) return false;
	if (DestinationMap.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("MapEntrance %s has no DestinationMap"), *GetName());
		return false;
	}
	if (DestinationPlayerStartTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("MapEntrance %s has no DestinationPlayerStartTag"), *GetName());
		return false;
	}

	AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(this));
	if (!AuraGameMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("MapEntrance %s requires AURAGameModeBase"), *GetName());
		return false;
	}
	UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(AuraGameMode->GetGameInstance());
	if (!AuraGameInstance || AuraGameInstance->LoadSlotName.IsEmpty() ||
		!UGameplayStatics::DoesSaveGameExist(AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("MapEntrance %s requires an active save slot before travel"), *GetName());
		return false;
	}
	const FString DestinationMapAssetName = DestinationMap.ToSoftObjectPath().GetAssetName();
	if (AuraGameMode->GetMapNameFromMapAssetName(DestinationMapAssetName).IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("MapEntrance %s has no registered destination map for %s"),
			*GetName(), *DestinationMapAssetName);
		return false;
	}
	FString PreviousMapName;
	FString PreviousMapAssetName;
	FName PreviousPlayerStartTag;
	{
		const ULoadScreenSaveGame* PreviousProgress = AuraGameMode->RetrieveInGameSaveData();
		if (!IsValid(PreviousProgress)) return false;
		PreviousMapName = PreviousProgress->MapName;
		PreviousMapAssetName = PreviousProgress->MapAssetName;
		PreviousPlayerStartTag = PreviousProgress->PlayerStartTag;
	}

	const bool bPreviouslyReached = bReached;
	bReached = true;
	bool bWorldSaved = false;
	const double SaveWorldStartTime = FPlatformTime::Seconds();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Aura_MapEntrance_SaveWorld);
		bWorldSaved = AuraGameMode->SaveWorldState(GetWorld(), DestinationMapAssetName);
	}
	const double SaveWorldMs = (FPlatformTime::Seconds() - SaveWorldStartTime) * 1000.0;
	if (!bWorldSaved)
	{
		bReached = bPreviouslyReached;
		UE_LOG(LogTemp, Error, TEXT("MapEntrance %s aborted travel because world state could not be saved"), *GetName());
		return false;
	}

	const double SaveProgressStartTime = FPlatformTime::Seconds();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Aura_MapEntrance_SaveProgress);
		IPlayerInterface::Execute_SaveProgress(Interactor, DestinationPlayerStartTag);
	}
	const double SaveProgressMs = (FPlatformTime::Seconds() - SaveProgressStartTime) * 1000.0;

	const double VerifyStartTime = FPlatformTime::Seconds();
	const ULoadScreenSaveGame* SavedProgress = nullptr;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Aura_MapEntrance_Verify);
		SavedProgress = AuraGameMode->GetSaveSlotData(
			AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex);
	}
	const double VerifyMs = (FPlatformTime::Seconds() - VerifyStartTime) * 1000.0;
	if (!SavedProgress || SavedProgress->bFirstTimeLoadIn ||
		SavedProgress->MapAssetName != DestinationMapAssetName ||
		SavedProgress->PlayerStartTag != DestinationPlayerStartTag)
	{
		UE_LOG(LogTemp, Log,
			TEXT("Aura timing: MapEntrance %s TravelRequest SaveWorld=%.3fms SaveProgress=%.3fms Verify=%.3fms OpenLevel=not-run"),
			*GetName(), SaveWorldMs, SaveProgressMs, VerifyMs);
		UE_LOG(LogTemp, Error, TEXT("MapEntrance %s could not verify destination progress in the active save slot"), *GetName());
		bReached = bPreviouslyReached;
		// Keep the latest world snapshot, but undo the destination intent of this aborted travel.
		// This is a best-effort failure compensation, not an atomic save transaction.
		ULoadScreenSaveGame* RestoredProgress = AuraGameMode->RetrieveInGameSaveData();
		if (IsValid(RestoredProgress))
		{
			RestoredProgress->MapName = PreviousMapName;
			RestoredProgress->MapAssetName = PreviousMapAssetName;
			RestoredProgress->PlayerStartTag = PreviousPlayerStartTag;
		}
		if (!IsValid(RestoredProgress) || !AuraGameMode->SaveInGameProgressData(RestoredProgress))
		{
			UE_LOG(LogTemp, Error, TEXT("MapEntrance %s could not restore the original map and spawn anchor after aborted travel"), *GetName());
		}
		return false;
	}

	const double OpenLevelStartTime = FPlatformTime::Seconds();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Aura_MapEntrance_OpenLevel);
		AuraGameInstance->PlayerStartTag = DestinationPlayerStartTag;
		bTravelRequested = true;
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, DestinationMap);
	}
	const double OpenLevelMs = (FPlatformTime::Seconds() - OpenLevelStartTime) * 1000.0;

	UE_LOG(LogTemp, Log,
		TEXT("Aura timing: MapEntrance %s TravelRequest SaveWorld=%.3fms SaveProgress=%.3fms Verify=%.3fms OpenLevel=%.3fms"),
		*GetName(), SaveWorldMs, SaveProgressMs, VerifyMs, OpenLevelMs);
	return true;
}

void AMapEntrance::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	// 传送改由点击交互触发；保留该绑定，避免 BeginPlay 里的委托悬空。
}

