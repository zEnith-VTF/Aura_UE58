// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AuraDebuffConfig.generated.h"

USTRUCT(BlueprintType)
struct FAuraDebuffConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debuff")
	float Chance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debuff")
	float Damage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debuff")
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debuff")
	float Frequency = 0.f;

	bool IsValidConfig() const
	{
		return FMath::IsFinite(Chance) && Chance >= 0.f && Chance <= 100.f
			&& FMath::IsFinite(Damage) && Damage >= 0.f
			&& FMath::IsFinite(Duration) && Duration > 0.f
			&& FMath::IsFinite(Frequency) && Frequency > 0.f;
	}
};
