// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/AuraOcclusionFadeComponent.h"

#include "Aura/AuraLogChannels.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace
{
	constexpr float OcclusionFadeSettleTolerance = 0.001f;

	struct FMeshSlotId
	{
		UStaticMeshComponent* Mesh = nullptr;
		int32 SlotIndex = INDEX_NONE;

		bool operator==(const FMeshSlotId& Other) const
		{
			return Mesh == Other.Mesh && SlotIndex == Other.SlotIndex;
		}

		friend uint32 GetTypeHash(const FMeshSlotId& Id)
		{
			return HashCombine(PointerHash(Id.Mesh), GetTypeHash(Id.SlotIndex));
		}
	};

	void AppendMaterialChain(const UMaterialInterface* Material, TArray<const UMaterialInterface*>& OutChain)
	{
		const UMaterialInterface* Current = Material;
		TSet<const UMaterialInterface*> Visited;
		while (IsValid(Current) && !Visited.Contains(Current))
		{
			Visited.Add(Current);
			OutChain.Add(Current);

			const UMaterialInstance* const MaterialInstance = Cast<UMaterialInstance>(Current);
			Current = MaterialInstance ? MaterialInstance->Parent.Get() : nullptr;
		}
	}
}

UAuraOcclusionFadeComponent::UAuraOcclusionFadeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
}

void UAuraOcclusionFadeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsDedicatedServerWorld())
	{
		SetComponentTickEnabled(false);
		return;
	}

	UWorld* const World = GetWorld();
	if (!World || !World->IsGameWorld() || World->bIsTearingDown)
	{
		return;
	}
	if (!DetectionBoxComponentName.IsNone())
	{
		DetectionBox = nullptr;
		if (AActor* const Owner = GetOwner())
		{
			TArray<UBoxComponent*> BoxComponents;
			Owner->GetComponents<UBoxComponent>(BoxComponents);
			for (UBoxComponent* const BoxComponent : BoxComponents)
			{
				if (IsValid(BoxComponent) && BoxComponent->GetFName() == DetectionBoxComponentName)
				{
					DetectionBox = BoxComponent;
					break;
				}
			}
		}
		if (!IsValid(DetectionBox))
		{
			UE_LOG(LogAura, Warning, TEXT("OcclusionFade: %s has no BoxComponent named %s"),
				*GetNameSafe(GetOwner()), *DetectionBoxComponentName.ToString());
		}
	}

	const float Interval = QueryInterval > 0.f ? QueryInterval : 0.08f;
	World->GetTimerManager().SetTimer(
		QueryTimerHandle,
		this,
		&UAuraOcclusionFadeComponent::HandleOcclusionQuery,
		Interval,
		true,
		0.f);
}

void UAuraOcclusionFadeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QueryTimerHandle);
	}

	RestoreAllControlledSlots();
	RestoreAllMeshVisibilityResponses();
	Super::EndPlay(EndPlayReason);
}

void UAuraOcclusionFadeComponent::BeginDestroy()
{
	RestoreAllControlledSlots();
	RestoreAllMeshVisibilityResponses();
	Super::BeginDestroy();
}

void UAuraOcclusionFadeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsDedicatedServerWorld() || !CanRunLocalFade())
	{
		RestoreAllControlledSlots();
		RestoreAllMeshVisibilityResponses();
		return;
	}

	UpdateTransitions(DeltaTime);
}

bool UAuraOcclusionFadeComponent::IsDedicatedServerWorld() const
{
	return GetNetMode() == NM_DedicatedServer;
}

bool UAuraOcclusionFadeComponent::CanRunLocalFade() const
{
	if (IsDedicatedServerWorld())
	{
		return false;
	}

	const UWorld* const World = GetWorld();
	if (!World || !World->IsGameWorld() || World->bIsTearingDown)
	{
		return false;
	}

	const APawn* const PawnOwner = Cast<APawn>(GetOwner());
	return IsValid(PawnOwner) && PawnOwner->IsLocallyControlled();
}

bool UAuraOcclusionFadeComponent::ImplementsExcludedInterface(const AActor* Actor) const
{
	if (!IsValid(Actor) || !ExcludedInterface)
	{
		return false;
	}

	const UClass* const InterfaceClass = ExcludedInterface.Get();
	if (!InterfaceClass || !InterfaceClass->HasAnyClassFlags(CLASS_Interface))
	{
		return false;
	}

	const UClass* const ActorClass = Actor->GetClass();
	return ActorClass && ActorClass->ImplementsInterface(InterfaceClass);
}

bool UAuraOcclusionFadeComponent::IsActorHiddenForFade(const AActor* Actor) const
{
	return IsValid(Actor) && Actor->IsHidden();
}

bool UAuraOcclusionFadeComponent::MaterialChainHasFadeScalar(const UMaterialInterface* Material) const
{
	if (FadeParameterName.IsNone() || !IsValid(Material))
	{
		return false;
	}

	TArray<const UMaterialInterface*> Chain;
	AppendMaterialChain(Material, Chain);

	const FHashedMaterialParameterInfo ParameterInfo(FadeParameterName);
	for (const UMaterialInterface* const ChainMaterial : Chain)
	{
		float UnusedValue = 0.f;
		if (ChainMaterial->GetScalarParameterValue(ParameterInfo, UnusedValue, false))
		{
			return true;
		}
	}

	return false;
}

UMaterialInterface* UAuraOcclusionFadeComponent::FindMappedFadeMaterial(const UMaterialInterface* Material) const
{
	TArray<const UMaterialInterface*> Chain;
	AppendMaterialChain(Material, Chain);

	for (const UMaterialInterface* const ChainMaterial : Chain)
	{
		for (const FAuraOcclusionFadeMaterialMapping& Mapping : MaterialMappings)
		{
			if (Mapping.SourceMaterial == ChainMaterial && IsValid(Mapping.FadeMaterial))
			{
				return Mapping.FadeMaterial;
			}
		}
	}

	return nullptr;
}

FString UAuraOcclusionFadeComponent::GetMaterialWarningKey(const UMaterialInterface* Material) const
{
	if (!IsValid(Material))
	{
		return TEXT("None");
	}

	TArray<const UMaterialInterface*> Chain;
	AppendMaterialChain(Material, Chain);
	for (const UMaterialInterface* const ChainMaterial : Chain)
	{
		if (!ChainMaterial->HasAnyFlags(RF_Transient))
		{
			return ChainMaterial->GetPathName();
		}
	}

	return Material->GetPathName();
}

void UAuraOcclusionFadeComponent::WarnUnmappedMaterialOnce(const UMaterialInterface* Material)
{
	const FString Key = GetMaterialWarningKey(Material);
	if (WarnedMissingMaterialKeys.Contains(Key))
	{
		return;
	}

	WarnedMissingMaterialKeys.Add(Key);
	UE_LOG(LogAura, Warning, TEXT("OcclusionFade: material '%s' has no Fade scalar and no mapping. Slot left unchanged."), *GetPathNameSafe(Material));
}

void UAuraOcclusionFadeComponent::WarnSlotReplacedOnce(const UStaticMeshComponent* Mesh, const FAuraOcclusionFadeSlotState& State)
{
	const UMaterialInterface* CurrentMaterial = (IsValid(Mesh) && State.MaterialSlot >= 0 && State.MaterialSlot < Mesh->GetNumMaterials())
		? Mesh->GetMaterial(State.MaterialSlot)
		: nullptr;
	const FString Key = FString::Printf(TEXT("%s|%d|%s"), *GetPathNameSafe(Mesh), State.MaterialSlot, *GetMaterialWarningKey(CurrentMaterial));
	if (WarnedReplacedSlotKeys.Contains(Key))
	{
		return;
	}

	WarnedReplacedSlotKeys.Add(Key);
	UE_LOG(
		LogAura,
		Warning,
		TEXT("OcclusionFade: %s slot %d is no longer this component's MID ('%s'). Control released without overwrite."),
		*GetNameSafe(IsValid(Mesh) ? Mesh->GetOwner() : nullptr),
		State.MaterialSlot,
		*GetPathNameSafe(CurrentMaterial));
}

void UAuraOcclusionFadeComponent::CollectStaticMeshes(AActor* Actor, TArray<UStaticMeshComponent*>& OutMeshes, TSet<const AActor*>& VisitedActors) const
{
	if (!IsValid(Actor) || VisitedActors.Contains(Actor))
	{
		return;
	}

	VisitedActors.Add(Actor);
	if (ImplementsExcludedInterface(Actor))
	{
		return;
	}

	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	if (!IsActorHiddenForFade(Actor))
	{
		Actor->GetComponents(Meshes);
	}
	for (UStaticMeshComponent* const Mesh : Meshes)
	{
		// HISM derives from UInstancedStaticMeshComponent and is skipped with it.
		if (!IsValid(Mesh) || Mesh->IsA<UInstancedStaticMeshComponent>())
		{
			continue;
		}

		OutMeshes.Add(Mesh);
	}

	TInlineComponentArray<UChildActorComponent*> ChildActorComponents;
	Actor->GetComponents(ChildActorComponents);
	for (UChildActorComponent* const ChildActorComponent : ChildActorComponents)
	{
		if (!IsValid(ChildActorComponent))
		{
			continue;
		}

		CollectStaticMeshes(ChildActorComponent->GetChildActor(), OutMeshes, VisitedActors);
	}
}

void UAuraOcclusionFadeComponent::HandleOcclusionQuery()
{
	// Local control is checked on every query, not only in BeginPlay.
	if (IsDedicatedServerWorld() || !CanRunLocalFade())
	{
		RestoreAllControlledSlots();
		RestoreAllMeshVisibilityResponses();
		return;
	}

	TArray<FDesiredOcclusionSlot> DesiredSlots;
	GatherDesiredSlots(DesiredSlots);
	SynchronizeSlots(DesiredSlots);
	RefreshPendingVisibilityRestores();
}

void UAuraOcclusionFadeComponent::GatherDesiredSlots(TArray<FDesiredOcclusionSlot>& OutSlots) const
{
	OutSlots.Reset();
	if (!IsValid(DetectionBox))
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	if (bIncludeWorldDynamic)
	{
		ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AuraOcclusionFade), false);
	if (AActor* const Owner = GetOwner())
	{
		QueryParams.AddIgnoredActor(Owner);
	}

	const FVector BoxExtent = DetectionBox->GetScaledBoxExtent().GetAbs();
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		DetectionBox->GetComponentLocation(),
		DetectionBox->GetComponentQuat(),
		ObjectQuery,
		FCollisionShape::MakeBox(BoxExtent),
		QueryParams);

	TSet<AActor*> SeenActors;
	TSet<FMeshSlotId> SeenSlots;
	SeenActors.Reserve(Overlaps.Num());

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Actor = Overlap.GetActor();
		if (!IsValid(Actor) || SeenActors.Contains(Actor))
		{
			continue;
		}

		SeenActors.Add(Actor);
		if (Actor == GetOwner() || Actor->IsHidden() || !Actor->ActorHasTag(OccluderTag) || ImplementsExcludedInterface(Actor))
		{
			continue;
		}

		TArray<UStaticMeshComponent*> Meshes;
		TSet<const AActor*> VisitedActors;
		CollectStaticMeshes(Actor, Meshes, VisitedActors);

		for (UStaticMeshComponent* const Mesh : Meshes)
		{
			if (!IsValid(Mesh))
			{
				continue;
			}

			const int32 NumMaterials = Mesh->GetNumMaterials();
			for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
			{
				const FMeshSlotId SlotId{Mesh, SlotIndex};
				if (SeenSlots.Contains(SlotId))
				{
					continue;
				}

				SeenSlots.Add(SlotId);
				FDesiredOcclusionSlot& Desired = OutSlots.AddDefaulted_GetRef();
				Desired.Mesh = Mesh;
				Desired.SlotIndex = SlotIndex;
				Desired.Occluder = Actor;
			}
		}
	}
}

void UAuraOcclusionFadeComponent::SynchronizeSlots(const TArray<FDesiredOcclusionSlot>& DesiredSlots)
{
	TSet<FMeshSlotId> DesiredIds;
	DesiredIds.Reserve(DesiredSlots.Num());
	for (const FDesiredOcclusionSlot& Desired : DesiredSlots)
	{
		DesiredIds.Add({Desired.Mesh, Desired.SlotIndex});
	}

	bool bNeedTick = false;
	for (int32 Index = SlotStates.Num() - 1; Index >= 0; --Index)
	{
		FAuraOcclusionFadeSlotState& State = SlotStates[Index];
		UStaticMeshComponent* const Mesh = State.MeshComponent.Get();
		AActor* const Occluder = State.OccluderActor.Get();
		const bool bMeshUsable = IsMeshSlotUsable(Mesh, State.MaterialSlot);
		const bool bHidden = IsActorHiddenForFade(Occluder) || (IsValid(Mesh) && IsActorHiddenForFade(Mesh->GetOwner()));

		if (!bMeshUsable || !IsValid(Occluder) || bHidden)
		{
			if (State.bControlledByComponent && bMeshUsable)
			{
				RestoreControlledSlot(State);
			}

			SlotStates.RemoveAtSwap(Index);
			continue;
		}

		const FMeshSlotId SlotId{Mesh, State.MaterialSlot};
		if (!DesiredIds.Contains(SlotId))
		{
			if (!State.bControlledByComponent)
			{
				SlotStates.RemoveAtSwap(Index);
				continue;
			}

			if (!SlotStillHasOurMID(Mesh, State))
			{
				WarnSlotReplacedOnce(Mesh, State);
				State.bControlledByComponent = false;
				State.FadeMID = nullptr;
				SlotStates.RemoveAtSwap(Index);
				continue;
			}

			State.TargetFade = VisibleFade;
			if (FadeDuration <= KINDA_SMALL_NUMBER || FMath::IsNearlyEqual(State.CurrentFade, State.TargetFade, OcclusionFadeSettleTolerance))
			{
				State.CurrentFade = State.TargetFade;
				RestoreControlledSlot(State);
				SlotStates.RemoveAtSwap(Index);
				continue;
			}

			bNeedTick = true;
			continue;
		}

		if (!State.bControlledByComponent)
		{
			continue;
		}

		if (!SlotStillHasOurMID(Mesh, State))
		{
			WarnSlotReplacedOnce(Mesh, State);
			State.bControlledByComponent = false;
			State.FadeMID = nullptr;
			continue;
		}

		State.TargetFade = OccludedFade;
		State.OccluderActor = Occluder;
		if (FadeDuration <= KINDA_SMALL_NUMBER)
		{
			if (!FMath::IsNearlyEqual(State.CurrentFade, State.TargetFade, OcclusionFadeSettleTolerance))
			{
				State.CurrentFade = State.TargetFade;
				ApplyFadeValue(State);
			}
		}
		else if (!FMath::IsNearlyEqual(State.CurrentFade, State.TargetFade, OcclusionFadeSettleTolerance))
		{
			bNeedTick = true;
		}
	}

	for (const FDesiredOcclusionSlot& Desired : DesiredSlots)
	{
		if (FindSlotStateIndex(Desired.Mesh, Desired.SlotIndex) != INDEX_NONE)
		{
			continue;
		}

		if (TryBeginSlot(Desired.Mesh, Desired.SlotIndex, Desired.Occluder))
		{
			const FAuraOcclusionFadeSlotState& NewState = SlotStates.Last();
			if (NewState.bControlledByComponent
				&& !FMath::IsNearlyEqual(NewState.CurrentFade, NewState.TargetFade, OcclusionFadeSettleTolerance))
			{
				bNeedTick = true;
			}
		}
	}

	if (bNeedTick)
	{
		RequestFadeTick();
	}
	else if (!HasAnyFlags(RF_BeginDestroyed))
	{
		SetComponentTickEnabled(false);
	}
}

void UAuraOcclusionFadeComponent::UpdateTransitions(float DeltaTime)
{
	bool bNeedTick = false;
	for (int32 Index = SlotStates.Num() - 1; Index >= 0; --Index)
	{
		FAuraOcclusionFadeSlotState& State = SlotStates[Index];
		UStaticMeshComponent* const Mesh = State.MeshComponent.Get();
		AActor* const Occluder = State.OccluderActor.Get();
		const bool bMeshUsable = IsMeshSlotUsable(Mesh, State.MaterialSlot);
		const bool bHidden = IsActorHiddenForFade(Occluder) || (bMeshUsable && IsActorHiddenForFade(Mesh->GetOwner()));

		if (!bMeshUsable || !IsValid(Occluder) || bHidden)
		{
			if (State.bControlledByComponent && bMeshUsable)
			{
				RestoreControlledSlot(State);
			}

			SlotStates.RemoveAtSwap(Index);
			continue;
		}

		if (!State.bControlledByComponent)
		{
			continue;
		}

		if (!SlotStillHasOurMID(Mesh, State))
		{
			// Keep the entry until the occluder leaves, so the next query does not write over the other material.
			WarnSlotReplacedOnce(Mesh, State);
			State.bControlledByComponent = false;
			State.FadeMID = nullptr;
			continue;
		}

		if (FMath::IsNearlyEqual(State.CurrentFade, State.TargetFade, OcclusionFadeSettleTolerance))
		{
			State.CurrentFade = State.TargetFade;
			if (FMath::IsNearlyEqual(State.TargetFade, VisibleFade, OcclusionFadeSettleTolerance))
			{
				RestoreControlledSlot(State);
				SlotStates.RemoveAtSwap(Index);
			}
			continue;
		}

		if (FadeDuration <= KINDA_SMALL_NUMBER)
		{
			State.CurrentFade = State.TargetFade;
		}
		else
		{
			const float Range = FMath::Max(FMath::Abs(VisibleFade - OccludedFade), KINDA_SMALL_NUMBER);
			const float Speed = Range / FadeDuration;
			State.CurrentFade = FMath::FInterpConstantTo(State.CurrentFade, State.TargetFade, DeltaTime, Speed);
			if (FMath::IsNearlyEqual(State.CurrentFade, State.TargetFade, OcclusionFadeSettleTolerance))
			{
				State.CurrentFade = State.TargetFade;
			}
		}

		if (FMath::IsNearlyEqual(State.TargetFade, VisibleFade, OcclusionFadeSettleTolerance)
			&& FMath::IsNearlyEqual(State.CurrentFade, State.TargetFade, OcclusionFadeSettleTolerance))
		{
			RestoreControlledSlot(State);
			SlotStates.RemoveAtSwap(Index);
			continue;
		}

		ApplyFadeValue(State);
		if (!FMath::IsNearlyEqual(State.CurrentFade, State.TargetFade, OcclusionFadeSettleTolerance))
		{
			bNeedTick = true;
		}
	}

	if (!HasAnyFlags(RF_BeginDestroyed))
	{
		SetComponentTickEnabled(bNeedTick);
	}

	// 淡入结束后才恢复；淡回期间 IsActorStillFadeControlled 为真，Visibility 保持 Ignore。
	RefreshPendingVisibilityRestores();
}

bool UAuraOcclusionFadeComponent::IsMeshSlotUsable(const UStaticMeshComponent* Mesh, int32 MaterialSlot) const
{
	return IsValid(Mesh) && MaterialSlot >= 0 && MaterialSlot < Mesh->GetNumMaterials();
}

bool UAuraOcclusionFadeComponent::SlotStillHasOurMID(const UStaticMeshComponent* Mesh, const FAuraOcclusionFadeSlotState& State) const
{
	if (!IsMeshSlotUsable(Mesh, State.MaterialSlot) || !State.FadeMID)
	{
		return false;
	}

	return Mesh->GetMaterial(State.MaterialSlot) == State.FadeMID;
}

void UAuraOcclusionFadeComponent::ApplyFadeValue(const FAuraOcclusionFadeSlotState& State) const
{
	if (UMaterialInstanceDynamic* const MID = State.FadeMID)
	{
		MID->SetScalarParameterValue(FadeParameterName, State.CurrentFade);
	}
}

void UAuraOcclusionFadeComponent::RestoreControlledSlot(FAuraOcclusionFadeSlotState& State)
{
	UStaticMeshComponent* const Mesh = State.MeshComponent.Get();
	if (!State.bControlledByComponent)
	{
		State.FadeMID = nullptr;
		return;
	}

	if (!IsMeshSlotUsable(Mesh, State.MaterialSlot) || !SlotStillHasOurMID(Mesh, State))
	{
		if (IsMeshSlotUsable(Mesh, State.MaterialSlot))
		{
			WarnSlotReplacedOnce(Mesh, State);
		}

		State.bControlledByComponent = false;
		State.FadeMID = nullptr;
		return;
	}

	Mesh->SetMaterial(State.MaterialSlot, State.OriginalMaterial);
	State.bControlledByComponent = false;
	State.FadeMID = nullptr;
}

void UAuraOcclusionFadeComponent::RestoreAllControlledSlots()
{
	for (FAuraOcclusionFadeSlotState& State : SlotStates)
	{
		if (State.bControlledByComponent)
		{
			RestoreControlledSlot(State);
		}
	}

	SlotStates.Reset();
	if (!HasAnyFlags(RF_BeginDestroyed))
	{
		SetComponentTickEnabled(false);
	}
}

int32 UAuraOcclusionFadeComponent::FindSlotStateIndex(const UStaticMeshComponent* Mesh, int32 MaterialSlot) const
{
	for (int32 Index = 0; Index < SlotStates.Num(); ++Index)
	{
		const FAuraOcclusionFadeSlotState& State = SlotStates[Index];
		if (State.MaterialSlot == MaterialSlot && State.MeshComponent.Get() == Mesh)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void UAuraOcclusionFadeComponent::CacheMeshVisibilityResponseIfNeeded(UStaticMeshComponent* Mesh, AActor* Occluder)
{
	if (!IsValid(Mesh) || !IsValid(Occluder))
	{
		return;
	}

	// 按组件缓存：同一网格的后续槽位不再把已改成 Ignore 的值写进缓存。
	for (const FAuraOcclusionFadeMeshVisibilityState& Pending : PendingVisibilityRestores)
	{
		if (Pending.MeshComponent.Get() == Mesh)
		{
			return;
		}
	}

	FAuraOcclusionFadeMeshVisibilityState& Pending = PendingVisibilityRestores.AddDefaulted_GetRef();
	Pending.MeshComponent = Mesh;
	Pending.OccluderActor = Occluder;
	Pending.OriginalResponse = Mesh->GetCollisionResponseToChannel(ECC_Visibility);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}

bool UAuraOcclusionFadeComponent::IsActorStillFadeControlled(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	for (const FAuraOcclusionFadeSlotState& State : SlotStates)
	{
		if (State.bControlledByComponent && State.OccluderActor.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

void UAuraOcclusionFadeComponent::RefreshPendingVisibilityRestores()
{
	for (int32 Index = PendingVisibilityRestores.Num() - 1; Index >= 0; --Index)
	{
		const FAuraOcclusionFadeMeshVisibilityState& Pending = PendingVisibilityRestores[Index];
		UStaticMeshComponent* const Mesh = Pending.MeshComponent.Get();
		if (!IsValid(Mesh))
		{
			// 组件已销毁：跳过恢复，直接丢弃挂起项。
			PendingVisibilityRestores.RemoveAtSwap(Index);
			continue;
		}

		// 只要该遮挡 Actor 还有本组件控制的槽（淡出中或尚未回到 VisibleFade）就不恢复；
		// 网格所在的 Child Actor 与命中 Actor 不同，故用缓存的目标而不是 Mesh->GetOwner()。
		if (IsActorStillFadeControlled(Pending.OccluderActor.Get()))
		{
			continue;
		}

		Mesh->SetCollisionResponseToChannel(ECC_Visibility, Pending.OriginalResponse);
		PendingVisibilityRestores.RemoveAtSwap(Index);
	}
}

void UAuraOcclusionFadeComponent::RestoreAllMeshVisibilityResponses()
{
	// 中断兜底：不判断淡入是否完成，按缓存值恢复每个组件的 Visibility 响应，避免永久 Ignore。
	for (const FAuraOcclusionFadeMeshVisibilityState& Pending : PendingVisibilityRestores)
	{
		UStaticMeshComponent* const Mesh = Pending.MeshComponent.Get();
		// 组件已销毁时跳过恢复，不崩。
		if (IsValid(Mesh))
		{
			Mesh->SetCollisionResponseToChannel(ECC_Visibility, Pending.OriginalResponse);
		}
	}

	PendingVisibilityRestores.Reset();
}

bool UAuraOcclusionFadeComponent::TryBeginSlot(UStaticMeshComponent* Mesh, int32 MaterialSlot, AActor* Occluder)
{
	if (!IsMeshSlotUsable(Mesh, MaterialSlot) || !IsValid(Occluder))
	{
		return false;
	}

	UMaterialInterface* const CurrentMaterial = Mesh->GetMaterial(MaterialSlot);
	UMaterialInterface* MidParent = nullptr;
	if (MaterialChainHasFadeScalar(CurrentMaterial))
	{
		MidParent = CurrentMaterial;
	}
	else
	{
		MidParent = FindMappedFadeMaterial(CurrentMaterial);
	}

	if (!MidParent)
	{
		WarnUnmappedMaterialOnce(CurrentMaterial);
		return false;
	}

	UMaterialInstanceDynamic* const MID = UMaterialInstanceDynamic::Create(MidParent, this);
	if (!MID)
	{
		WarnUnmappedMaterialOnce(CurrentMaterial);
		return false;
	}

	float InitialFade = VisibleFade;
	if (MidParent == CurrentMaterial && !FadeParameterName.IsNone())
	{
		MID->GetScalarParameterValue(FHashedMaterialParameterInfo(FadeParameterName), InitialFade, false);
	}

	FAuraOcclusionFadeSlotState& State = SlotStates.AddDefaulted_GetRef();
	State.MeshComponent = Mesh;
	State.MaterialSlot = MaterialSlot;
	State.OriginalMaterial = CurrentMaterial;
	State.FadeMID = MID;
	State.CurrentFade = InitialFade;
	State.TargetFade = OccludedFade;
	State.bControlledByComponent = true;
	State.OccluderActor = Occluder;

	if (FadeDuration <= KINDA_SMALL_NUMBER)
	{
		State.CurrentFade = OccludedFade;
	}

	Mesh->SetMaterial(MaterialSlot, MID);
	if (Mesh->GetMaterial(MaterialSlot) != MID)
	{
		State.bControlledByComponent = false;
		State.FadeMID = nullptr;
		SlotStates.RemoveAt(SlotStates.Num() - 1);
		return false;
	}

	// 淡出真正开始（材质已换成本组件的 MID）后才临时忽略 Visibility，其它通道与碰撞开关不动。
	CacheMeshVisibilityResponseIfNeeded(Mesh, Occluder);

	ApplyFadeValue(State);
	return true;
}

void UAuraOcclusionFadeComponent::RequestFadeTick()
{
	if (!HasAnyFlags(RF_BeginDestroyed))
	{
		SetComponentTickEnabled(true);
	}
}
