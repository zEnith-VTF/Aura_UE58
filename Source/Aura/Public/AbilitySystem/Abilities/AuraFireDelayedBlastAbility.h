// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraDelayedBlastAbility.h"
#include "AuraFireDelayedBlastAbility.generated.h"

UCLASS()
class AURA_API UAuraFireDelayedBlastAbility : public UAuraDelayedBlastAbility
{
	GENERATED_BODY()

public:
	virtual FString GetDescription(int32 Level) override;
	virtual FString GetNextLevelDescription(int32 Level) override;

private:
	FString BuildDescription(int32 Level, bool bNextLevel) const;
};
