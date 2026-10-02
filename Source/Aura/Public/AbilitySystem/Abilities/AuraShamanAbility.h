// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AuraShamanAbility.generated.h"

class APawn;

/**
 * 
 */
UCLASS()
class AURA_API UAuraShamanAbility : public UAuraGameplayAbility
{
	GENERATED_BODY()

public:
	// 根据角色朝向和召唤扇形，计算所有随从的生成位置。
	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetSpawnLocations();

	// 从配置的随从类型中随机选择一个；没有配置时返回空类。
	UFUNCTION(BlueprintPure)
	TSubclassOf<APawn> GetRandomMinionClass() const;

protected:
	// 一次技能计划召唤的随从数量。
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	int32 NumMinions = 5;

	// 可以随机生成的随从类型。
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	TArray<TSubclassOf<APawn>> MinionClasses;

	// 生成点与施法者之间的最小距离。
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float MinSpawnDistance = 50.f;

	// 生成点与施法者之间的最大距离。
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float MaxSpawnDistance = 250.f;

	// 所有生成点在角色前方分布的扇形总角度。
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float SpawnSpread = 90.f;
};
