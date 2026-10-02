// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Data/AttributeInfo.h"

#include "Aura/AuraLogChannels.h"

FAuraAttributeInfo UAttributeInfo::FindAttributeInfo(const FGameplayTag& AttributeTag, bool bNotFound) const
{
	for (const auto& AttributeInfo : AttributeInformation)
	{
		if (AttributeInfo.AttributeTag == AttributeTag)
		{
			return AttributeInfo;
		}	
	}
	if (bNotFound)
	{
		UE_LOG(LogAura,Error,TEXT("Can you find the info for AttributeTag [%s] on AttributeInfo [%s]"),*AttributeTag.ToString(),*GetNameSafe(this));
	}
	return FAuraAttributeInfo();
}
 