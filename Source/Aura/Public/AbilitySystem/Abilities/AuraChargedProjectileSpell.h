// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraProjectileSpell.h"
#include "AuraChargedProjectileSpell.generated.h"

/**
 * 
 */
UCLASS()
class AURA_API UAuraChargedProjectileSpell : public UAuraProjectileSpell
{
	GENERATED_BODY()
public:
	UAuraChargedProjectileSpell();
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
protected:
	UFUNCTION()
	void OnChargedReleased(float TimeHeld);
	
	void BeginReleaseTargetData();
	
	UFUNCTION()
	void OnReleaseTargetData(const FGameplayAbilityTargetDataHandle& Data);
private:
	double ChargeStartTime=0;
	bool bHasValidCharge=false;
	bool bChargeQualifiedForRelease=false;
};
