// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Data/CharacterClassInfo.h"

FCharacterClassDefaultInfo UCharacterClassInfo::GetClassDefaultInfo(ECharacterClass CharacterClass)
{
	if (const FCharacterClassDefaultInfo* ClassDefaultInfo = CharacterClassInformation.Find(CharacterClass))
	{
		return *ClassDefaultInfo;
	}

	ensureMsgf(false, TEXT("CharacterClassInformation has no entry for character class [%d]"),
		static_cast<int32>(CharacterClass));
	return FCharacterClassDefaultInfo();
}

