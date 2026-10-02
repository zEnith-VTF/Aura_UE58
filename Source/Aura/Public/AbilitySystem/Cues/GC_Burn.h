// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "GC_Burn.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 燃烧减益（GameplayCue.Debuff.Burn）的表现 Cue：在目标 Actor 的根组件上生成并维持燃烧 Niagara 特效。
 * 激活 / 持续激活时确保特效存在；移除或目标已死亡时停掉特效。
 */
UCLASS()
class AURA_API AGC_Burn : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AGC_Burn();

	/** Cue 激活：确保燃烧特效存在后交回父类处理。 */
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	/** Cue 重新处于激活状态：同样确保特效存在。 */
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	/** Cue 移除：停止并清理燃烧特效后交回父类处理。 */
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	/** 燃烧特效使用的 Niagara 系统；构造函数默认加载 /Game/Assets/Effects/Fire/NS_Fire。 */
	UPROPERTY(EditDefaultsOnly, Category = "Burn")
	TObjectPtr<UNiagaraSystem> BurnNiagaraSystem;

	/** 当前生成在目标身上的燃烧 Niagara 组件。 */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> SpawnedBurnComponent;

	/** 确保目标身上有燃烧特效：目标无效或已死亡时停止特效；否则按需生成或重新激活。 */
	void EnsureBurnVfx(AActor* MyTarget);

	/** 立即停用并销毁已生成的燃烧组件。 */
	void StopBurnVfx();
};
