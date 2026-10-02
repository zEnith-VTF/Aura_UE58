#include "Actor/AuraFireNadoField.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/AuraFireNadoArea.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Interfaction/CombatInterface.h"
#include "TimerManager.h"

AAuraFireNadoField::AAuraFireNadoField()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void AAuraFireNadoField::InitializeField(const FDamageEffectParams& InDamageParams, const float InDamagePeriod)
{
	if (!HasAuthority()) return;
	DamageParams = InDamageParams;
	DamagePeriod = InDamagePeriod;
}

void AAuraFireNadoField::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && DamagePeriod > 0.f)
	{
		GetWorldTimerManager().SetTimer(DamageTimer, this, &AAuraFireNadoField::DamagePulse, DamagePeriod, true);
	}
}

void AAuraFireNadoField::AddArea(AAuraFireNadoArea* Area)
{
	if (HasAuthority() && IsValid(Area)) Areas.Add(Area);
}

void AAuraFireNadoField::NotifyTornadoFinished()
{
	if (!HasAuthority()) return;
	bTornadoActive = false;
	Areas.RemoveAll([](const TWeakObjectPtr<AAuraFireNadoArea>& Area) { return !Area.IsValid(); });
	if (Areas.IsEmpty()) Destroy();
}

void AAuraFireNadoField::DamagePulse()
{
	if (!HasAuthority()) return;
	Areas.RemoveAll([](const TWeakObjectPtr<AAuraFireNadoArea>& Area) { return !Area.IsValid(); });
	if (Areas.IsEmpty())
	{
		if (!bTornadoActive) Destroy();
		return;
	}

	UAbilitySystemComponent* const SourceASC = DamageParams.SourceAbilitySystemComponent;
	AActor* const SourceAvatar = Cast<AActor>(DamageParams.WorldContextObject.Get());
	UWorld* const World = GetWorld();
	if (!IsValid(SourceASC) || !IsValid(SourceAvatar) || SourceASC->GetAvatarActor() != SourceAvatar ||
		!IsValid(World) || !DamageParams.DamageGameplayEffectClass) return;

	TSet<TWeakObjectPtr<AActor>> Targets;
	for (const TWeakObjectPtr<AAuraFireNadoArea>& WeakArea : Areas)
	{
		const AAuraFireNadoArea* const Area = WeakArea.Get();
		if (!IsValid(Area)) continue;

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FireNadoArea), false);
		QueryParams.AddIgnoredActor(Area);
		World->OverlapMultiByObjectType(
			Overlaps, Area->GetActorLocation(), FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeCapsule(Area->GetAreaRadius(), FMath::Max(Area->GetAreaRadius(), 150.f)),
			QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* const Candidate = Overlap.GetActor();
			if (!IsValid(Candidate) || !Candidate->Implements<UCombatInterface>() ||
				ICombatInterface::Execute_IsDead(Candidate)) continue;

			AActor* const Avatar = ICombatInterface::Execute_GetAvatar(Candidate);
			if (!IsValid(Avatar) || Avatar == SourceAvatar ||
				UAuraAbilitySystemLibrary::AreActorsFriends(SourceAvatar, Avatar) ||
				(Avatar->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Avatar))) continue;

			if (FVector::DistSquared2D(Avatar->GetActorLocation(), Area->GetActorLocation()) <=
				FMath::Square(Area->GetAreaRadius())) Targets.Add(Avatar);
		}
	}

	for (const TWeakObjectPtr<AActor>& WeakTarget : Targets)
	{
		// A previous hit can kill another target or replace the ASC avatar synchronously.
		if (IsActorBeingDestroyed() || !IsValid(SourceASC) || !IsValid(SourceAvatar) ||
			SourceASC->GetAvatarActor() != SourceAvatar) return;
		AActor* const Target = WeakTarget.Get();
		if (!IsValid(Target) || Target == SourceAvatar ||
			UAuraAbilitySystemLibrary::AreActorsFriends(SourceAvatar, Target) ||
			(Target->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Target))) continue;
		UAbilitySystemComponent* const TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		if (!IsValid(TargetASC)) continue;
		if (IsActorBeingDestroyed() || !IsValid(SourceASC) || !IsValid(SourceAvatar) ||
			SourceASC->GetAvatarActor() != SourceAvatar) return;
		if (!IsValid(Target) || UAuraAbilitySystemLibrary::AreActorsFriends(SourceAvatar, Target) ||
			(Target->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Target))) continue;
		FDamageEffectParams TargetParams = DamageParams;
		TargetParams.TargetAbilitySystemComponent = TargetASC;
		UAuraAbilitySystemLibrary::ApplyDamageEffect(TargetParams);
	}
}

void AAuraFireNadoField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DamageTimer);
	Areas.Empty();
	Super::EndPlay(EndPlayReason);
}
