// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/SpellMenuWigdetController.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "Input/AuraPlayerState.h"
#include "Tags/AuraGameplayTags.h"

namespace
{
	void DisableDashButtons(const FGameplayTag& AbilityTag, bool& bSpendPoints, bool& bEquip)
	{
		if (AbilityTag.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Movement_Dash))
		{
			bSpendPoints = false;
			bEquip = false;
		}
	}
}

void USpellMenuWigdetController::BroadcastInitialValues()
{
	BroadcastAbilityInfo();
	CurrentSpellPoints = GetAuraPS()->GetSpellPoints();
	OnSpellPointsChangedDelegate.Broadcast(CurrentSpellPoints);
}

void USpellMenuWigdetController::BindCallbacksToDependencies()
{
	if (bCallbacksBound) return;

	if (!GetAuraASC()->bStartUpAbilitiesGiven)
	{
		GetAuraASC()->AbilitiesGivenDelegate.AddUObject(this, &USpellMenuWigdetController::BroadcastAbilityInfo);
	}
	GetAuraASC()->AbilityStatusChanged.AddLambda(
		[this](const FGameplayTag& AbilityTag, const FGameplayTag& StatusTag,int32 NewLevel)
	{
		if (SelectedAbility.Ability.MatchesTagExact(AbilityTag))
		{
			SelectedAbility.Status = StatusTag;

			bool bShouldEnableSpellPointsButton = false;
			bool bShouldEnableEquipButton = false;
			ShouldEnableButtons(
				SelectedAbility.Status,
				CurrentSpellPoints,
				bShouldEnableSpellPointsButton,
				bShouldEnableEquipButton);
			DisableDashButtons(SelectedAbility.Ability, bShouldEnableSpellPointsButton, bShouldEnableEquipButton);
			
			FString Description;
			FString NextLevelDescription;
			GetAuraASC()->GetDescriptionsByAbilityTag(AbilityTag,AbilityInfo,Description,NextLevelDescription);
			SpellGlobeSelectedDelegate.Broadcast(
				bShouldEnableSpellPointsButton,
				bShouldEnableEquipButton,Description,NextLevelDescription);
		}

		if (!IsValid(AbilityInfo)) return;

		FAuraAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(AbilityTag);
		Info.StatusTag = StatusTag;
		AbilityInfoDelegate.Broadcast(Info);
	});
	
	GetAuraASC()->AbilityEquipped.AddUObject(this,&USpellMenuWigdetController::OnAbilityEquipped);
	
	GetAuraPS()->OnSpellPointsChangedDelegate.AddLambda(
		[this](int32 NewValue)
		{
			OnSpellPointsChangedDelegate.Broadcast(NewValue);
			CurrentSpellPoints = NewValue;

			bool bShouldEnableSpellPointsButton = false;
			bool bShouldEnableEquipButton = false;
			ShouldEnableButtons(
				SelectedAbility.Status,
				CurrentSpellPoints,
				bShouldEnableSpellPointsButton,
				bShouldEnableEquipButton);
			DisableDashButtons(SelectedAbility.Ability, bShouldEnableSpellPointsButton, bShouldEnableEquipButton);
			FString Description;
			FString NextLevelDescription;
			GetAuraASC()->GetDescriptionsByAbilityTag(SelectedAbility.Ability,AbilityInfo,Description,NextLevelDescription);
			SpellGlobeSelectedDelegate.Broadcast(
				bShouldEnableSpellPointsButton,
				bShouldEnableEquipButton,Description,NextLevelDescription);
		});

	bCallbacksBound = true;
}

void USpellMenuWigdetController::OnAbilityEquipped(const FGameplayTag& AbilityTag, const FGameplayTag& Status,
	const FGameplayTag& Slot, const FGameplayTag& PreviousSlot)
{
	bWaitForEquipSelection = false;

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	FAuraAbilityInfo LastSlotInfo;
	LastSlotInfo.StatusTag = GameplayTags.Abilities_Status_Unlocked;
	LastSlotInfo.InputTag = PreviousSlot;
	LastSlotInfo.AbilityTag = GameplayTags.Abilities_None;
	// Broadcast empty info if PreviousSlot is a valid slot. Only if equipping an already-equipped spell.
	AbilityInfoDelegate.Broadcast(LastSlotInfo);

	FAuraAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(AbilityTag);
	Info.StatusTag = Status;
	Info.InputTag = Slot;
	AbilityInfoDelegate.Broadcast(Info);

	StopEquipButtonPressedSignature.Broadcast(AbilityInfo->FindAbilityInfoForTag(AbilityTag).AbilityTypeTag);
}

void USpellMenuWigdetController::SpellGlobeSelected(const FGameplayTag& AbilityTag)
{
	if (bWaitForEquipSelection)
	{
		const FGameplayTag TypeTag=AbilityInfo->FindAbilityInfoForTag(AbilityTag).AbilityTypeTag;
		StopEquipButtonPressedSignature.Broadcast(TypeTag);
		bWaitForEquipSelection=false;
	}
	
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	SelectedAbility.Ability = AbilityTag;
	SelectedAbility.Status = GameplayTags.Abilities_Status_Locked;

	if (AbilityTag.IsValid() && !AbilityTag.MatchesTagExact(GameplayTags.Abilities_None))
	{
		if (const FGameplayAbilitySpec* AbilitySpec = GetAuraASC()->GetSpecFromAbilityTag(AbilityTag))
		{
			SelectedAbility.Status = UAuraAbilitySystemComponent::GetStatusFromSpec(*AbilitySpec);
		}
	}

	bool bShouldEnableSpellPointsButton = false;
	bool bShouldEnableEquipButton = false;
	ShouldEnableButtons(
		SelectedAbility.Status,
		CurrentSpellPoints,
		bShouldEnableSpellPointsButton,
		bShouldEnableEquipButton);
	DisableDashButtons(SelectedAbility.Ability, bShouldEnableSpellPointsButton, bShouldEnableEquipButton);
	FString Description;
	FString NextLevelDescription;
	GetAuraASC()->GetDescriptionsByAbilityTag(AbilityTag,AbilityInfo,Description,NextLevelDescription);
	SpellGlobeSelectedDelegate.Broadcast(
		bShouldEnableSpellPointsButton,
		bShouldEnableEquipButton,Description,NextLevelDescription);
}

void USpellMenuWigdetController::GlobeDeselect()
{
	if (bWaitForEquipSelection)
	{
		const FGameplayTag TypeTag=AbilityInfo->FindAbilityInfoForTag(SelectedAbility.Ability).AbilityTypeTag;
		StopEquipButtonPressedSignature.Broadcast(TypeTag);
		bWaitForEquipSelection=false;
	}
	
	SelectedAbility.Ability = FAuraGameplayTags::Get().Abilities_None;
	SelectedAbility.Status = FAuraGameplayTags::Get().Abilities_Status_Locked;

	SpellGlobeSelectedDelegate.Broadcast(false, false, FString(), FString());
}

void USpellMenuWigdetController::EquipButtonPressed()
{
	if (SelectedAbility.Ability.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Movement_Dash)) return;

	const FGameplayTag TypeTag=AbilityInfo->FindAbilityInfoForTag(SelectedAbility.Ability).AbilityTypeTag;
	OnEquipButtonPressedSignature.Broadcast(TypeTag);
	
	bWaitForEquipSelection=true;
	const FGameplayTag StatusTag=GetAuraASC()->GetStatusFromAbilityTag(SelectedAbility.Ability);
	if (StatusTag.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Equipped))
	{
		SelectedSlot=GetAuraASC()->GetInputTagFromAbilityTag(SelectedAbility.Ability);
	}
}

void USpellMenuWigdetController::SpellRowGlobePressed(const FGameplayTag& SlotTag, const FGameplayTag& AbilityType)
{
	if (!bWaitForEquipSelection) return;
	if (SlotTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_Passive_1)) return;
	// Check selected ability against the slot's ability type.
	// (don't equip an offensive spell in a passive slot and vice versa)
	const FGameplayTag& SelectedAbilityType = AbilityInfo->FindAbilityInfoForTag(SelectedAbility.Ability).AbilityTypeTag;
	if (!SelectedAbilityType.MatchesTagExact(AbilityType)) return;

	GetAuraASC()->ServerEquipAbility(SelectedAbility.Ability, SlotTag);
}

void USpellMenuWigdetController::SpendPointButtonPressed()
{
	if (SelectedAbility.Ability.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Movement_Dash)) return;

	if (GetAuraASC())
	{
		GetAuraASC()->ServerSpendSpellPoint(SelectedAbility.Ability);
	}
}

void USpellMenuWigdetController::ShouldEnableButtons(const FGameplayTag& AbilityStatus, int32 SpellPoints,
                                                     bool& bShouldEnableSpellPointsButton, bool& bShouldEnableEquipButton)
{
	bShouldEnableSpellPointsButton = false;
	bShouldEnableEquipButton = false;

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	if (AbilityStatus.MatchesTagExact(GameplayTags.Abilities_Status_Equipped))
	{
		bShouldEnableEquipButton = true;
		bShouldEnableSpellPointsButton = SpellPoints > 0;
	}
	else if (AbilityStatus.MatchesTagExact(GameplayTags.Abilities_Status_Eligible))
	{
		bShouldEnableSpellPointsButton = SpellPoints > 0;
	}
	else if (AbilityStatus.MatchesTagExact(GameplayTags.Abilities_Status_Unlocked))
	{
		bShouldEnableEquipButton = true;
		bShouldEnableSpellPointsButton = SpellPoints > 0;
	}
}
