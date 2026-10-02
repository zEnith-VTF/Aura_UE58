// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AuraGameplayAbility.h"

#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "Characters/AuraCharacter.h"
#include "Interfaction/CombatInterface.h"
#include "Tags/AuraGameplayTags.h"

void UAuraGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	VoiceCastCharacter.Reset();
	VoiceCastASC.Reset();
	VoiceCastAbilityTag = FGameplayTag();
	VoiceCastGeneration = 0;
	bVoiceCastCommitted = false;
	bVoiceCastReported = false;

	// Capture before Super dispatches Blueprint activation, which can commit synchronously.
	if (ActorInfo && ActorInfo->IsNetAuthority() &&
		InstancingPolicy == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		UAbilitySystemComponent* const SourceASC = ActorInfo->AbilitySystemComponent.Get();
		AAuraCharacter* const Character = Cast<AAuraCharacter>(ActorInfo->AvatarActor.Get());
		if (IsValid(SourceASC) && IsValid(Character) && Character->HasAuthority() &&
			!ICombatInterface::Execute_IsDead(Character) && SourceASC->GetAvatarActor() == Character)
		{
			if (const FGameplayAbilitySpec* const Spec = SourceASC->FindAbilitySpecFromHandle(Handle))
			{
				const FGameplayTag AbilityTag = UAuraAbilitySystemComponent::GetAbilityTagFromSpec(*Spec);
				if (Character->SupportsAbilityVoice(AbilityTag))
				{
					VoiceCastCharacter = Character;
					VoiceCastASC = SourceASC;
					VoiceCastAbilityTag = AbilityTag;
					VoiceCastGeneration = Character->GetAbilityVoiceGeneration();
				}
			}
		}
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UAuraGameplayAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	FGameplayTagContainer* OptionalRelevantTags)
{
	const bool bSucceeded = Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);
	RecordVoiceCastCommitResult(bSucceeded);
	return bSucceeded;
}

void UAuraGameplayAbility::RecordVoiceCastCommitResult(const bool bSucceeded)
{
	if (VoiceCastASC.IsValid() && !bVoiceCastReported)
	{
		bVoiceCastCommitted = bSucceeded;
	}
}

void UAuraGameplayAbility::ReportSuccessfulCastForVoice()
{
	AAuraCharacter* const Character = VoiceCastCharacter.Get();
	UAbilitySystemComponent* const SourceASC = VoiceCastASC.Get();
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (bVoiceCastReported || !bVoiceCastCommitted || !IsActive() ||
		!ActorInfo || !ActorInfo->IsNetAuthority() || !IsValid(Character) || !Character->HasAuthority() ||
		Character->IsActorBeingDestroyed() || ICombatInterface::Execute_IsDead(Character) ||
		!IsValid(SourceASC) || GetAbilitySystemComponentFromActorInfo() != SourceASC ||
		SourceASC->GetAvatarActor() != Character || GetAvatarActorFromActorInfo() != Character ||
		Character->GetAbilityVoiceGeneration() != VoiceCastGeneration)
	{
		return;
	}

	// Set before the notification path can invoke user Blueprint code on a local owner.
	bVoiceCastReported = true;
	Character->RecordSuccessfulAbilityVoiceCast(VoiceCastAbilityTag, VoiceEverySuccessfulCasts, VoiceCastGeneration);
}

FString UAuraGameplayAbility::GetDescription(int32 Level)
{
	return FString::Printf(TEXT("<Default>默认技能名称 - 技能描述占位文本，当前等级：</><Level>%d 级</>"), Level);
}

FString UAuraGameplayAbility::GetNextLevelDescription(int32 Level)
{
	return FString::Printf(TEXT("<Default>下一等级：</><Level>%d </>\n<Default>级造成更多伤害。</>"), Level);
}

FString UAuraGameplayAbility::GetLockedDescription(int32 Level)
{
	return FString::Printf(TEXT("<Default>技能将在角色等级达到 </><Level>%d </><Default> 级时解锁。</>"), Level);
}

float UAuraGameplayAbility::GetManaCost(float InLevel) const
{
	float ManaCost = 0.f;
	if (const UGameplayEffect* CostEffect = GetCostGameplayEffect())
	{
		for (FGameplayModifierInfo Mod : CostEffect->Modifiers)
		{
			if (Mod.Attribute == UAuraAttributeSet::GetManaAttribute())
			{
				Mod.ModifierMagnitude.GetStaticMagnitudeIfPossible(InLevel, ManaCost);
				break;
			}
		}
	}
	return ManaCost;
}

float UAuraGameplayAbility::GetCooldown(float InLevel) const
{
	float Cooldown = 0.f;
	if (const UGameplayEffect* CooldownEffect = GetCooldownGameplayEffect())
	{
		CooldownEffect->DurationMagnitude.GetStaticMagnitudeIfPossible(InLevel, Cooldown);
	}
	return Cooldown;
}
