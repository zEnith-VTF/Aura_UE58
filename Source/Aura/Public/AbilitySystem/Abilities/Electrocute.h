// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraBeamSpell.h"
#include "Electrocute.generated.h"

/**
 * 雷电技能（课程版）：对锁定的首个目标持续造成闪电伤害并可能眩晕，
 * 等级提升后向周围最多 MaxNumShockTargets 个敌人连锁。
 */
UCLASS()
class AURA_API UElectrocute : public UAuraBeamSpell
{
	GENERATED_BODY()
public:
	virtual FString GetDescription(int32 Level) override;
	virtual FString GetNextLevelDescription(int32 Level) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	// 持续施法技能：拦截提交，冷却期间拒绝再次施法，但只提交 Cost，Cooldown 延后到 EndAbility。
	virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override;

protected:
	// 移除本次激活 Apply 的 Infinite 周期 Cost GE；未配置 Cost GE 时为空操作。
	void RemoveElectrocuteCostEffect();

private:
	// 每个执行实例只提交一次冷却，防止重复 EndAbility 刷新冷却。
	bool bCooldownCommitted = false;
};
