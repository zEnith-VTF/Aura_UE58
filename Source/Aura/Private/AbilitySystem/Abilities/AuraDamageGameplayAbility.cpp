// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Data/AuraDebuffConfig.h"

void UAuraDamageGameplayAbility::CauseDamage(AActor* TargetActor)
{
	if (!IsValid(TargetActor) || !DamageEffectClass) return;

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!SourceASC || !TargetASC) return;

	FGameplayEffectSpecHandle DamageSpecHandle =
		MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());
	if (!DamageSpecHandle.IsValid()) return;

	const float ScaledDamage =Damage.GetValueAtLevel(GetAbilityLevel());
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		DamageSpecHandle,
		DamageType,
		ScaledDamage);

	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
}

FDamageEffectParams UAuraDamageGameplayAbility::MakeDamageEffectParamsFromClassDefaults(AActor* TargetActor) const
{
	FDamageEffectParams Params;
	Params.WorldContextObject = GetAvatarActorFromActorInfo();
	Params.DamageGameplayEffectClass = DamageEffectClass;
	Params.SourceAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	Params.TargetAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	Params.BaseDamage = Damage.GetValueAtLevel(GetAbilityLevel());
	Params.AbilityLevel = GetAbilityLevel();
	Params.DamageType = DamageType;
	Params.bEnableDebuff = false;
	Params.DebuffChance = 0.f;
	Params.DebuffDamage = 0.f;
	Params.DebuffDuration = 0.f;
	Params.DebuffFrequency = 0.f;

	if (const FAuraDebuffConfigRow* const DebuffRow = DebuffConfigRow.GetRow<FAuraDebuffConfigRow>(TEXT("DebuffConfig")))
	{
		if (DebuffRow->IsValidConfig())
		{
			Params.bEnableDebuff = true;
			Params.DebuffChance = DebuffRow->Chance;
			Params.DebuffDamage = DebuffRow->Damage;
			Params.DebuffDuration = DebuffRow->Duration;
			Params.DebuffFrequency = DebuffRow->Frequency;
		}
	}
	return Params;
}

FTaggedMontage UAuraDamageGameplayAbility::GetRandomTaggedMontageFromArray(
	const TArray<FTaggedMontage>& TaggedMontages) const
{
	if (TaggedMontages.Num() > 0)
	{
		const int32 Selection = FMath::RandRange(0, TaggedMontages.Num() - 1);
		return TaggedMontages[Selection];
	}

	return FTaggedMontage();
}
