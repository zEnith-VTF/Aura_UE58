// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AuraInputConfig.h"
#include "EnhancedInputComponent.h"
#include "AuraInputComponent.generated.h"

/**
 * 
 */
UCLASS()
class AURA_API UAuraInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
public:
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(
	const UAuraInputConfig* InputConfig,
	UserClass* Object,
	PressedFuncType PressedFunc,
	ReleasedFuncType ReleasedFunc,
	HeldFuncType HeldFunc
);	
};

template <class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
void UAuraInputComponent::BindAbilityActions(const UAuraInputConfig* InputConfig, UserClass* Object,
	PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc)
{
	check(InputConfig);
	
	for (const auto& Action : InputConfig->InputActions)
	{
		if (Action.InputAction&& Action.ActionTag.IsValid())
		{
			if (PressedFunc)
			{
				//BindBindAction接收不定量的参数，多余的会传给绑定的函数
				BindAction(Action.InputAction,ETriggerEvent::Started,Object,PressedFunc,Action.ActionTag);
			}
			
			if (ReleasedFunc)
			{
				//BindAction接收不定量的参数，多余的会传给绑定的函数
				BindAction(Action.InputAction,ETriggerEvent::Completed,Object,ReleasedFunc,Action.ActionTag);
			}
			
			if (HeldFunc)
			{
				//BindAction接收不定量的参数，多余的会传给HeldFuc
				BindAction(Action.InputAction,ETriggerEvent::Triggered,Object,HeldFunc,Action.ActionTag);
			}
		}
	}
}
