// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Effects/AuraBurnEffect.h"

#include "AbilitySystem/AuraAttributeSet.h"
#include "Tags/AuraGameplayTags.h"

UAuraBurnEffect::UAuraBurnEffect()
{
	FAuraGameplayTags::InitializedNativeGameplayTags();
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();

	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f);
	Period = 1.f;
	bExecutePeriodicEffectOnApplication = false;

	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	bDenyOverflowApplication = false;
	bClearStackOnOverflow = false;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UAuraAttributeSet::GetIncomingDamageAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = Tags.Debuff_Damage;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);

	InheritableOwnedTagsContainer.Added.AddTag(Tags.Debuff_Burn);

	FGameplayEffectCue Cue;
	Cue.GameplayCueTags.AddTag(Tags.GameplayCue_Debuff_Burn);
	GameplayCues.Add(Cue);
}
