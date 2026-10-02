// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/AuraInputConfig.h"

const UInputAction* UAuraInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bNotFound) const
{
	for (const auto& Action : InputActions)
	{
		if (Action.InputAction&&Action.ActionTag == InputTag)
		{
			
			return Action.InputAction.Get();
		}
		
	}
	if (bNotFound)
	{
		UE_LOG(LogTemp,Error,TEXT("Can you find the input action for InputTag [%s] on UAuraInputConfig [%s]"),*InputTag.ToString(),*GetNameSafe(this));
	}
	return nullptr;
}
