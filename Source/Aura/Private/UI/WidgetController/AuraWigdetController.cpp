// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/AuraWigdetController.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Input/AuraPlayerController.h"
#include "Input/AuraPlayerState.h"
#include "Tags/AuraGameplayTags.h"

AAuraPlayerController* UAuraWigdetController::GetAuraPC()
{
	if (!AuraPlayerController)
	{
		AuraPlayerController = CastChecked<AAuraPlayerController>(PlayerController);
	}
	return AuraPlayerController;
}

AAuraPlayerState* UAuraWigdetController::GetAuraPS()
{
	if (!AuraPlayerState)
	{
		AuraPlayerState = CastChecked<AAuraPlayerState>(PlayerState);
	}
	return AuraPlayerState;
}

UAuraAbilitySystemComponent* UAuraWigdetController::GetAuraASC()
{
	if (!AuraAbilitySystemComponent)
	{
		AuraAbilitySystemComponent = CastChecked<UAuraAbilitySystemComponent>(AbilitySystemComponent);
	}
	return AuraAbilitySystemComponent;
}

UAuraAttributeSet* UAuraWigdetController::GetAuraAS()
{
	if (!AuraAttributeSet)
	{
		AuraAttributeSet = CastChecked<UAuraAttributeSet>(AttributeSet);
	}
	return AuraAttributeSet;
}

void UAuraWigdetController::BroadcastAbilityInfo()
{
	if (!GetAuraASC()->bStartUpAbilitiesGiven) return;

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	FAuraAbilityInfo DashDisplayInfo;
	bool bHasEquippedDash = false;
	FForEachAbility BroadcastDelegate;
	BroadcastDelegate.BindLambda([this, &Tags, &DashDisplayInfo, &bHasEquippedDash](const FGameplayAbilitySpec& AbilitySpec)
	{
		FAuraAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(GetAuraASC()->GetAbilityTagFromSpec(AbilitySpec));
		Info.InputTag = GetAuraASC()->GetInputTagFromSpec(AbilitySpec);
		Info.StatusTag = GetAuraASC()->GetStatusFromSpec(AbilitySpec);
		AbilityInfoDelegate.Broadcast(Info);
		if (Info.AbilityTag.MatchesTagExact(Tags.Abilities_Movement_Dash)
			&& Info.InputTag.MatchesTagExact(Tags.InputTag_Space)
			&& Info.StatusTag.MatchesTagExact(Tags.Abilities_Status_Equipped))
		{
			DashDisplayInfo = Info;
			bHasEquippedDash = true;
		}
	});
	GetAuraASC()->ForEachAbility(BroadcastDelegate);
	if (bHasEquippedDash)
	{
		// Only the UI copy uses Passive.1; the ASC spec keeps Space for input and saving.
		DashDisplayInfo.InputTag = Tags.InputTag_Passive_1;
		AbilityInfoDelegate.Broadcast(DashDisplayInfo);
	}
}

void UAuraWigdetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	PlayerController=WCParams.PC;
	PlayerState=WCParams.PS;
	AttributeSet=WCParams.AS;
	AbilitySystemComponent=WCParams.ASC;
}

void UAuraWigdetController::BroadcastInitialValues()
{
}

void UAuraWigdetController::BindCallbacksToDependencies()
{
}
