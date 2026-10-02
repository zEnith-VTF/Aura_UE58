// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AuraProjectileSpell.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/AuraProjectile.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaction/CombatInterface.h"
#include "Tags/AuraGameplayTags.h"

void UAuraProjectileSpell::SpawnProjectile(const FVector& ProjectileTargetLocation, FGameplayTag SocketTag)
{
	if (IsValid(SpawnProjectileWithResult(ProjectileTargetLocation, SocketTag)))
	{
		ReportSuccessfulCastForVoice();
	}
}

AAuraProjectile* UAuraProjectileSpell::SpawnProjectileWithResult(const FVector& ProjectileTargetLocation,
	FGameplayTag SocketTag)const
{
	AActor* const AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || !AvatarActor->HasAuthority()) return nullptr;
	if (!ensureMsgf(AvatarActor->Implements<UCombatInterface>(),
		TEXT("Projectile ability avatar [%s] must implement CombatInterface"), *GetNameSafe(AvatarActor)))
	{
		return nullptr;
	}

	UWorld* const World = GetWorld();
	UAbilitySystemComponent* const SourceASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AvatarActor);
	APawn* const InstigatorPawn = Cast<APawn>(AvatarActor);

	if (!ensureMsgf(IsValid(World), TEXT("Cannot spawn projectile without a valid World")) ||
		!ensureMsgf(ProjectileClass, TEXT("ProjectileClass is not configured on ability [%s]"), *GetNameSafe(this)) ||
		!ensureMsgf(DamageEffectClass, TEXT("DamageEffectClass is not configured on ability [%s]"), *GetNameSafe(this)) ||
		!ensureMsgf(IsValid(SourceASC), TEXT("Avatar [%s] has no AbilitySystemComponent"), *GetNameSafe(AvatarActor)) ||
		!ensureMsgf(IsValid(InstigatorPawn), TEXT("Projectile ability avatar [%s] must be a Pawn"), *GetNameSafe(AvatarActor)))
	{
		return nullptr;
	}

	if (!SocketTag.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("Projectile ability [%s] received an invalid combat socket tag for avatar [%s]"),
			*GetNameSafe(this),
			*GetNameSafe(AvatarActor));
		return nullptr;
	}

	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		AvatarActor,
		SocketTag);
	const FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();
	
	//构造生成变换信息
	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SocketLocation);
	SpawnTransform.SetRotation(Rotation.Quaternion());
	
	AActor* const OwnerActor = IsValid(GetOwningActorFromActorInfo())
		? GetOwningActorFromActorInfo()
		: AvatarActor;
	AAuraProjectile* Projectile = World->SpawnActorDeferred<AAuraProjectile>(
		ProjectileClass,
		SpawnTransform,
		OwnerActor,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!ensureMsgf(IsValid(Projectile), TEXT("Failed to spawn projectile for ability [%s]"), *GetNameSafe(this)))
	{
		return nullptr;
	}
	
	Projectile->DamageEffectParams=MakeDamageEffectParamsFromClassDefaults();
	
	Projectile->FinishSpawning(SpawnTransform);
	if (IsValid(Projectile))return Projectile;
	return nullptr;
}

void UAuraProjectileSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                           const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                           const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}
