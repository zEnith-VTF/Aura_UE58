// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"
#include "AuraDelayedBlastAbility.generated.h"

class AAuraDelayedBlastActor;
class UAnimMontage;

UCLASS()
class AURA_API UAuraDelayedBlastAbility : public UAuraDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UAuraDelayedBlastAbility();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	/** Reads warning delay and blast radius from BlastActorClass CDO. Does not spawn or mutate. */
	bool TryGetBlastPresentationValues(float& OutWarningDelay, float& OutBlastRadius) const;

private:
	bool TryGetValidatedTargetLocation(
		const FGameplayAbilityTargetDataHandle& TargetData,
		FVector& OutTargetLocation) const;
	bool SpawnBlast(const FVector& TargetLocation);

	/** Starts the cast montage together with the Event.Montage.DelayBlast listener. */
	void StartCastTaskFlow();
	/** Single exit for both montage completion and interruption; never spawns on its own. */
	void EndCastFlow(bool bWasCancelled);

	UFUNCTION()
	void OnDelayBlastNotify(FGameplayEventData Payload);

	UFUNCTION()
	void OnCastMontageCompleted();

	UFUNCTION()
	void OnCastMontageBlendOut();

	UFUNCTION()
	void OnCastMontageInterrupted();

	UFUNCTION()
	void OnCastMontageCancelled();

	/** Writes the cast point into the avatar's Motion Warping FacingTarget; no-op without CombatInterface. */
	void UpdateAvatarFacingTarget(const FVector& FacingLocation);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delayed Blast", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AAuraDelayedBlastActor> BlastActorClass;

	/** Optional. When set, the blast is created by the montage notify instead of the target data callback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delayed Blast", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> CastMontage;

	/** Prevents a duplicate target-data callback from ever attempting a second commit. */
	bool bServerResolvedTarget = false;

	/** Server-only: set once SpawnBlast succeeded, so a repeated notify can never spawn twice. */
	bool bServerBlastSpawned = false;

	/** Client-only: keeps a duplicated target-data payload from starting a second cast montage. */
	bool bClientCastStarted = false;

	/** Server-only: the validated ground point the notify will spawn at. */
	FVector PendingTargetLocation = FVector::ZeroVector;
};
