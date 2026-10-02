// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/AuraDelayedBlastActor.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Interfaction/CombatInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AAuraDelayedBlastActor::AAuraDelayedBlastActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	WarningDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("WarningDecal"));
	WarningDecal->SetupAttachment(SceneRoot);
	WarningDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	WarningDecal->SetVisibility(true);
}

void AAuraDelayedBlastActor::InitializeBlast(
	const FVector& InBlastCenter,
	const FDamageEffectParams& InDamageEffectParams)
{
	if (!HasAuthority())
	{
		return;
	}

	BlastCenter = InBlastCenter;
	DamageEffectParams = InDamageEffectParams;
	SetActorLocation(InBlastCenter);
}

void AAuraDelayedBlastActor::BeginPlay()
{
	Super::BeginPlay();

	UpdateWarningDecal();
	if (bHasExploded)
	{
		PlayExplosionPresentation();
		return;
	}

	OnWarningStarted();

	if (HasAuthority())
	{
		UWorld* const World = GetWorld();
		if (!IsValid(World))
		{
			Destroy();
			return;
		}

		if (WarningDelay <= 0.f)
		{
			Explode();
		}
		else
		{
			World->GetTimerManager().SetTimer(
				ExplosionTimerHandle,
				this,
				&AAuraDelayedBlastActor::Explode,
				WarningDelay,
				false);
		}
	}
}

void AAuraDelayedBlastActor::Destroyed()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExplosionTimerHandle);
	}

	if (WarningDecal)
	{
		WarningDecal->SetVisibility(false);
	}

	Super::Destroyed();
}

void AAuraDelayedBlastActor::Explode()
{
	if (!HasAuthority() || bHasExploded)
	{
		return;
	}

	bHasExploded = true;
	ApplyDamageToOverlaps();
	MulticastPlayExplosionPresentation();
	Destroy();
}

void AAuraDelayedBlastActor::ApplyDamageToOverlaps()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* const World = GetWorld();
	UAbilitySystemComponent* const SourceASC = DamageEffectParams.SourceAbilitySystemComponent;
	if (!IsValid(World) || !IsValid(SourceASC))
	{
		return;
	}

	AActor* const SourceAvatar = SourceASC->GetAvatarActor();
	if (!IsValid(SourceAvatar))
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DelayedBlastOverlap), true);
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		BlastCenter,
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects),
		// The center is on the ground while character actor locations are usually near capsule center.
		// A box keeps the candidate query bounded without excluding valid edge targets by 3D distance.
		FCollisionShape::MakeBox(FVector(BlastRadius, BlastRadius, BlastRadius)),
		QueryParams);

	const float RadiusSquared = FMath::Square(BlastRadius);
	TSet<AActor*> DamagedAvatars;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const OverlapActor = Overlap.GetActor();
		if (!IsValid(OverlapActor) || OverlapActor == this || !OverlapActor->Implements<UCombatInterface>())
		{
			continue;
		}

		if (ICombatInterface::Execute_IsDead(OverlapActor))
		{
			continue;
		}

		AActor* const AvatarActor = ICombatInterface::Execute_GetAvatar(OverlapActor);
		if (!IsValid(AvatarActor) || DamagedAvatars.Contains(AvatarActor))
		{
			continue;
		}

		if (AvatarActor->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(AvatarActor))
		{
			continue;
		}

		if (AvatarActor == SourceAvatar || UAuraAbilitySystemLibrary::AreActorsFriends(SourceAvatar, AvatarActor))
		{
			continue;
		}

		// The overlap is only a same-layer bounded candidate query. Damage uses horizontal XY
		// distance, so normal capsule-center height does not shrink the requested radius.
		if (FVector::DistSquared2D(AvatarActor->GetActorLocation(), BlastCenter) > RadiusSquared)
		{
			continue;
		}

		UAbilitySystemComponent* const TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AvatarActor);
		if (!IsValid(TargetASC))
		{
			continue;
		}

		DamagedAvatars.Add(AvatarActor);
		FDamageEffectParams TargetDamageParams = DamageEffectParams;
		TargetDamageParams.TargetAbilitySystemComponent = TargetASC;
		UAuraAbilitySystemLibrary::ApplyDamageEffect(TargetDamageParams);
	}
}

void AAuraDelayedBlastActor::PlayExplosionPresentation()
{
	if (bExplosionPresentationPlayed)
	{
		return;
	}

	bExplosionPresentationPlayed = true;
	if (WarningDecal)
	{
		WarningDecal->SetVisibility(false);
	}

	OnBlastExploded();
}

void AAuraDelayedBlastActor::UpdateWarningDecal()
{
	if (!WarningDecal)
	{
		return;
	}

	if (WarningDecalMaterial)
	{
		WarningDecal->SetDecalMaterial(WarningDecalMaterial);
	}

	// Decal Y/Z cover the warning footprint; Blueprint subclasses may replace the visual.
	WarningDecal->DecalSize = FVector(64.f, BlastRadius, BlastRadius);
	WarningDecal->SetVisibility(!bHasExploded);
}

void AAuraDelayedBlastActor::MulticastPlayExplosionPresentation_Implementation()
{
	PlayExplosionPresentation();
}

void AAuraDelayedBlastActor::OnRep_BlastCenter()
{
	SetActorLocation(BlastCenter);
}

void AAuraDelayedBlastActor::OnRep_BlastRadius()
{
	UpdateWarningDecal();
}

void AAuraDelayedBlastActor::OnRep_HasExploded()
{
	if (bHasExploded)
	{
		PlayExplosionPresentation();
	}
}

void AAuraDelayedBlastActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAuraDelayedBlastActor, BlastCenter);
	DOREPLIFETIME(AAuraDelayedBlastActor, BlastRadius);
	DOREPLIFETIME(AAuraDelayedBlastActor, WarningDelay);
	DOREPLIFETIME(AAuraDelayedBlastActor, bHasExploded);
}
