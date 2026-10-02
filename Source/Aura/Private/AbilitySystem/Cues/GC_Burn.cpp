// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Cues/GC_Burn.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Interfaction/CombatInterface.h"
#include "UObject/ConstructorHelpers.h"

AGC_Burn::AGC_Burn()
{
	// Cue 被移除时自动销毁自身；不按 Instigator / SourceObject 唯一化，同一目标允许叠加多个实例。
	bAutoDestroyOnRemove = true;
	bUniqueInstancePerInstigator = false;
	bUniqueInstancePerSourceObject = false;
	// 默认加载燃烧 Niagara 资产；加载失败时保持为空，可在蓝图子类中配置。
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BurnSystemFinder(TEXT("/Game/Assets/Effects/Fire/NS_Fire"));
	if (BurnSystemFinder.Succeeded())
	{
		BurnNiagaraSystem = BurnSystemFinder.Object;
	}
}

bool AGC_Burn::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// Cue 激活：先确保燃烧特效存在，再交回父类。
	EnsureBurnVfx(MyTarget);
	return Super::OnActive_Implementation(MyTarget, Parameters);
}

bool AGC_Burn::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// Cue 重新处于激活状态：同样确保特效存在。
	EnsureBurnVfx(MyTarget);
	return Super::WhileActive_Implementation(MyTarget, Parameters);
}

bool AGC_Burn::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// Cue 移除：停止特效后交回父类。
	StopBurnVfx();
	return Super::OnRemove_Implementation(MyTarget, Parameters);
}

void AGC_Burn::EnsureBurnVfx(AActor* MyTarget)
{
	// 目标无效或已死亡：停掉特效并返回。
	if (!IsValid(MyTarget) || (MyTarget->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(MyTarget)))
	{
		StopBurnVfx();
		return;
	}

	// 缺少 Niagara 资产或目标没有根组件：本次不生成。
	if (!IsValid(BurnNiagaraSystem) || !MyTarget->GetRootComponent())
	{
		return;
	}

	// 组件已挂在同一目标上：失活时重新激活即可。
	if (IsValid(SpawnedBurnComponent) && SpawnedBurnComponent->GetAttachParent() == MyTarget->GetRootComponent())
	{
		if (!SpawnedBurnComponent->IsActive())
		{
			SpawnedBurnComponent->Activate(true);
		}
		return;
	}

	// 目标变化或组件已失效：销毁旧组件，重新生成并挂到目标根组件上。
	StopBurnVfx();
	SpawnedBurnComponent = NewObject<UNiagaraComponent>(MyTarget);
	SpawnedBurnComponent->SetAsset(BurnNiagaraSystem);
	SpawnedBurnComponent->SetupAttachment(MyTarget->GetRootComponent());
	SpawnedBurnComponent->RegisterComponent();
	SpawnedBurnComponent->Activate(true);
}

void AGC_Burn::StopBurnVfx()
{
	// 立即停用并销毁组件，避免特效残留。
	if (IsValid(SpawnedBurnComponent))
	{
		SpawnedBurnComponent->DeactivateImmediate();
		SpawnedBurnComponent->DestroyComponent();
	}
	SpawnedBurnComponent = nullptr;
}
