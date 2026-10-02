// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AuraFireBolt.h"

#include "Tags/AuraGameplayTags.h"


FString UAuraFireBolt::GetDescription(int32 Level)
{
	// 描述文本只展示整数伤害，这里显式收窄以避免隐式 float 转换警告。
	const int32 ScaleDamage = static_cast<int32>(Damage.GetValueAtLevel(Level));

	const float ManaCost = FMath::Abs(GetManaCost(Level));
	const float Cooldown = GetCooldown(Level);
	if (Level > 1)
	{
		return FString::Printf(TEXT(
			// Title
			"<Title>火焰弹</>\n\n"

			// Level
			"<Small>等级：</><Level>%d</>\n"
			// ManaCost
			"<Small>法力消耗：</><ManaCost>%.1f</>\n"
			// Cooldown
			"<Small>冷却时间：</><Cooldown>%.1f</>\n\n"
			
			"<Default>发射一枚火焰弹，"
			"命中后爆炸并造成：</>"

			// Damage
			"<Damage>%d</><Default> 点火焰伤害，并有一定概率"
			"使目标燃烧。</>"),

			// Values
			Level,
			ManaCost,
			Cooldown,
			ScaleDamage);
	}

	return FString::Printf(TEXT(
			// Title
			"<Title>火焰弹</>\n\n"

			// Level
			"<Small>等级：</><Level>%d</>\n"
			// ManaCost
			"<Small>法力消耗：</><ManaCost>%.1f</>\n"
			// Cooldown
			"<Small>冷却时间：</><Cooldown>%.1f</>\n\n"

			// Number of FireBolts
			"<Default>发射 %d 枚火焰弹，"
			"命中后爆炸并造成：</>"

			// Damage
			"<Damage>%d</><Default> 点火焰伤害，并有一定概率"
			"使目标燃烧。</>"),

			// Values
			Level,
			ManaCost,
			Cooldown,
			FMath::Min(Level, NewProjectiles),
			ScaleDamage);		
}

FString UAuraFireBolt::GetNextLevelDescription(int32 Level)
{
	const int32 ScaleDamage = static_cast<int32>(Damage.GetValueAtLevel(Level));
	
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

				// The current FireBolt Blueprint spawns one projectile per cast.
				"<Default>发射一枚火焰弹，"
				"命中后爆炸并造成：</>"

				// Damage
				"<Damage>%d</><Default> 点火焰伤害，并有一定概率"
				"使目标燃烧。</>"),

				// Values
				Level,
				ManaCost,
				Cooldown,
				ScaleDamage);
}
