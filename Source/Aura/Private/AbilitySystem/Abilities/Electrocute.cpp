// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Electrocute.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

void UElectrocute::RemoveElectrocuteCostEffect()
{
	UAbilitySystemComponent* const AuraASC = GetAbilitySystemComponentFromActorInfo();
	if (!IsValid(AuraASC))
	{
		return;
	}

	const UGameplayEffect* const CostEffect = GetCostGameplayEffect();
	if (!IsValid(CostEffect))
	{
		return;
	}

	// 按 Cost GE 定义精确匹配，只回收本次周期扣蓝，不影响其它 ActiveEffect。
	FGameplayEffectQuery CostQuery;
	CostQuery.CustomMatchDelegate.BindLambda([CostEffect](const FActiveGameplayEffect& ActiveEffect)
	{
		return ActiveEffect.Spec.Def == CostEffect;
	});
	AuraASC->RemoveActiveEffects(CostQuery);
}

bool UElectrocute::CommitAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	FGameplayTagContainer* OptionalRelevantTags)
{
	// 持续施法技能：Cost 仍在激活时提交（周期扣蓝），Cooldown 延后到 EndAbility 正常结束时提交。
	// 这里保留"冷却期间不能再次施法"的门槛：冷却 GE 仍在生效时直接拒绝本次提交。
	UAbilitySystemComponent* const AuraASC = GetAbilitySystemComponentFromActorInfo();
	const UGameplayEffect* const CooldownEffect = GetCooldownGameplayEffect();
	if (IsValid(AuraASC) && IsValid(CooldownEffect))
	{
		FGameplayEffectQuery CooldownQuery;
		CooldownQuery.CustomMatchDelegate.BindLambda([CooldownEffect](const FActiveGameplayEffect& ActiveEffect)
		{
			return ActiveEffect.Spec.Def == CooldownEffect;
		});
		for (const float TimeRemaining : AuraASC->GetActiveEffectsTimeRemaining(CooldownQuery))
		{
			if (TimeRemaining > 0.f)
			{
				RecordVoiceCastCommitResult(false);
				return false;
			}
		}
	}

	const bool bSucceeded = CommitAbilityCost(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags);
	RecordVoiceCastCommitResult(bSucceeded);
	return bSucceeded;
}

void UElectrocute::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// 父类负责补发 Listen Server 的 InputReleased 复制事件，必须先调用。
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	RemoveElectrocuteCostEffect();
}

void UElectrocute::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 松键、取消与目标死亡最终都会走到这里，统一回收周期 Cost GE。
	RemoveElectrocuteCostEffect();

	// 持续施法期间不进入冷却：只有非取消的正常结束（松键 / 目标死亡）才在这里提交冷却，每个实例只提交一次。
	if (!bWasCancelled && !bCooldownCommitted)
	{
		bCooldownCommitted = true;
		CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FString UElectrocute::GetDescription(int32 Level)
{
	const int32 ScaledDamage = Damage.GetValueAtLevel(Level);
	const float ManaCost = FMath::Abs(GetManaCost(Level));
	const float Cooldown = GetCooldown(Level);
	if (Level == 1)
	{
		return FString::Printf(TEXT(
			// Title
			"<Title>电击</>\n\n"

			// Level
			"<Small>等级：</><Level>%d</>\n"
			// ManaCost
			"<Small>法力消耗：</><ManaCost>%.1f</>\n"
			// Cooldown
			"<Small>冷却时间：</><Cooldown>%.1f</>\n\n"

			"<Default>发射一道闪电光束，"
			"连接目标并持续造成 </>"

			// Damage
			"<Damage>%d</><Default> 点闪电伤害，并有一定概率"
			"使目标眩晕。</>"),

			// Values
			Level,
			ManaCost,
			Cooldown,
			ScaledDamage);
	}
	else
	{
		return FString::Printf(TEXT(
			// Title
			"<Title>电击</>\n\n"

			// Level
			"<Small>等级：</><Level>%d</>\n"
			// ManaCost
			"<Small>法力消耗：</><ManaCost>%.1f</>\n"
			// Cooldown
			"<Small>冷却时间：</><Cooldown>%.1f</>\n\n"

			// Addition Number of Shock Targets
			"<Default>发射一道闪电光束，"
			"向附近额外 %d 个目标传导，造成 </>"

			// Damage
			"<Damage>%d</><Default> 点闪电伤害，并有一定概率"
			"使目标眩晕。</>"),

			// Values
			Level,
			ManaCost,
			Cooldown,
			FMath::Min(Level, MaxNumShockTargets - 1),
			ScaledDamage);
	}
}

FString UElectrocute::GetNextLevelDescription(int32 Level)
{
	const int32 ScaledDamage = Damage.GetValueAtLevel(Level);
	const float ManaCost = FMath::Abs(GetManaCost(Level));
	const float Cooldown = GetCooldown(Level);
	return FString::Printf(TEXT(
			// Title
			"<Title>下一等级：</>\n\n"

			// Level
			"<Small>等级：</><Level>%d</>\n"
			// ManaCost
			"<Small>法力消耗：</><ManaCost>%.1f</>\n"
			// Cooldown
			"<Small>冷却时间：</><Cooldown>%.1f</>\n\n"

			// Addition Number of Shock Targets
			"<Default>发射一道闪电光束，"
			"向附近额外 %d 个目标传导，造成 </>"

			// Damage
			"<Damage>%d</><Default> 点闪电伤害，并有一定概率"
			"使目标眩晕。</>"),

			// Values
			Level,
			ManaCost,
			Cooldown,
			FMath::Min(Level, MaxNumShockTargets - 1),
			ScaledDamage);
}
