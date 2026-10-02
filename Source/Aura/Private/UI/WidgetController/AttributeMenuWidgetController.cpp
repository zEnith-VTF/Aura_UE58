// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/AttributeMenuWidgetController.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/AttributeInfo.h"
#include "Input/AuraPlayerState.h"



class UAuraAttributeSet;
void UAttributeMenuWidgetController::BroadcastInitialValues()
{
	check(AttributeInfo);

	for (auto &Pair : GetAuraAS()->TagToAttributeMap)
	{
		BroadcastAttributeInfo(Pair.Key, Pair.Value().GetNumericValue(GetAuraAS()));
	}

	OnAttributePointsChanged(GetAuraPS()->GetAttributePoints());
	OnSpellPointsChanged(GetAuraPS()->GetSpellPoints());

}

void UAttributeMenuWidgetController::BindCallbacksToDependencies()
{
	if (bCallbacksBound) return;

	GetAuraPS()->OnAttributePointsChangedDelegate.AddUObject(
		this,
		&UAttributeMenuWidgetController::OnAttributePointsChanged);
	GetAuraPS()->OnSpellPointsChangedDelegate.AddUObject(
		this,
		&UAttributeMenuWidgetController::OnSpellPointsChanged);

	for (auto &Pair : GetAuraAS()->TagToAttributeMap)
	{
		GetAuraASC()->GetGameplayAttributeValueChangeDelegate(Pair.Value()).AddLambda
		(
        	[this,Pair](const FOnAttributeChangeData& Data)
        	{
        		BroadcastAttributeInfo(Pair.Key, Data.NewValue);
        	}
		);
	}

	bCallbacksBound = true;
}

void UAttributeMenuWidgetController::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
	GetAuraASC()->UpgradeAttribute(AttributeTag);
}

void UAttributeMenuWidgetController::BroadcastAttributeInfo(const FGameplayTag& AttributeTag, float AttributeValue)
{
	FAuraAttributeInfo Info=AttributeInfo->FindAttributeInfo(AttributeTag, true);
	Info.AttributeValue=AttributeValue;
	AttributeInfoDelegate.Broadcast(Info);
}

void UAttributeMenuWidgetController::OnAttributePointsChanged(int32 NewValue)
{
	OnAttributePointsChangedDelegate.Broadcast(NewValue);
}

void UAttributeMenuWidgetController::OnSpellPointsChanged(int32 NewValue)
{
	OnSpellPointsChangedDelegate.Broadcast(NewValue);
}

