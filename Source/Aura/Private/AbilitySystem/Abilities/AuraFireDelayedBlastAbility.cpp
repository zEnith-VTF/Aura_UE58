// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/AuraFireDelayedBlastAbility.h"

FString UAuraFireDelayedBlastAbility::GetDescription(int32 Level)
{
	return BuildDescription(Level, false);
}

FString UAuraFireDelayedBlastAbility::GetNextLevelDescription(int32 Level)
{
	return BuildDescription(Level, true);
}

FString UAuraFireDelayedBlastAbility::BuildDescription(int32 Level, bool bNextLevel) const
{
	const int32 ScaledDamage = static_cast<int32>(Damage.GetValueAtLevel(Level));
	const float ManaCost = FMath::Abs(GetManaCost(static_cast<float>(Level)));
	const float Cooldown = GetCooldown(static_cast<float>(Level));
	const TCHAR* const Title = bNextLevel ? TEXT("<Title>下一等级：</>") : TEXT("<Title>延时爆破</>");

	FString Description = FString::Printf(
		TEXT("%s\n\n<Small>技能等级：</><Level>%d</>\n<Small>法力消耗：</><ManaCost>%.1f</>\n<Small>冷却时间：</><Cooldown>%.1f</><Default> 秒</>\n\n"),
		Title,
		Level,
		ManaCost,
		Cooldown);

	float WarningDelay = 0.f;
	float BlastRadius = 0.f;
	if (TryGetBlastPresentationValues(WarningDelay, BlastRadius))
	{
		Description += FString::Printf(
			TEXT("<Default>在指定地面位置布置爆破区域，</><Cooldown>%.1f</><Default> 秒后引爆，爆破半径 </><Level>%.1f</><Default> 米，对范围内的敌人造成 </><Damage>%d</><Default> 点基础火焰伤害。</>"),
			WarningDelay,
			BlastRadius / 100.f,
			ScaledDamage);
	}
	else
	{
		Description += FString::Printf(
			TEXT("<Default>在指定地面位置布置爆破区域，引爆后对范围内的敌人造成 </><Damage>%d</><Default> 点基础火焰伤害。</>"),
			ScaledDamage);
	}

	return Description;
}
