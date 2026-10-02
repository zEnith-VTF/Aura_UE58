// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"
#include "AuraBeamSpell.generated.h"

/**
 * 光束技能基类（课程版）：锁定光标命中的首个目标，并向其周围连锁最多 MaxNumShockTargets 个敌人。
 * 激活流程、光束生成与伤害 tick 均由派生蓝图（如 GA_Electrocute）驱动。
 */
UCLASS()
class AURA_API UAuraBeamSpell : public UAuraDamageGameplayAbility
{
	GENERATED_BODY()
public:
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	// 存储鼠标命中数据；无阻挡命中则取消能力。
	UFUNCTION(BlueprintCallable)
	void StoreMouseDataInfo(const FHitResult& HitResult);

	// 缓存拥有者的 PlayerController 与 Character。
	UFUNCTION(BlueprintCallable)
	void StoreOwnerVariables();

	// 从武器 TipSocket 向目标点做球形追踪，锁定首个命中的 Actor 并绑定其死亡委托。
	UFUNCTION(BlueprintCallable)
	void TraceFirstTarget(const FVector& BeamTargetLocation);

	// 在主目标周围收集最多 MaxNumShockTargets 个存活敌人作为连锁目标，并绑定死亡委托。
	UFUNCTION(BlueprintCallable)
	void StoreAdditionalTargets(TArray<AActor*>& OutAdditionalTargets);

	// 主目标死亡事件（蓝图实现）：清理主光束并结束能力。
	UFUNCTION(BlueprintImplementableEvent)
	void PrimaryTargetDied(AActor* DeadActor);

	// 连锁目标死亡事件（蓝图实现）：清理该目标的光束。
	UFUNCTION(BlueprintImplementableEvent)
	void AdditionalTargetDied(AActor* DeadActor);
protected:

	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	FVector MouseHitLocation;

	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	TObjectPtr<AActor> MouseHitActor;

	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	TObjectPtr<APlayerController> OwnerPlayerController;

	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	TObjectPtr<ACharacter> OwnerCharacter;

	// 连锁的最大目标数；实际连锁数 = Min(技能等级-1, 该值)。
	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	int32 MaxNumShockTargets = 5;
};
