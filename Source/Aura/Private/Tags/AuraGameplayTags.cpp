// Fill out your copyright notice in the Description page of Project Settings.


#include "Tags/AuraGameplayTags.h"

#include "GameplayTagsManager.h"

void FAuraGameplayTags::InitializedNativeGameplayTags()
{
	/*
	 *Primary Attributes
	 */
	
	GameplayTags.Attributes_Primary_Strength = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Primary.Strength")),
		FString(TEXT("力量,提高物理伤害")));

	GameplayTags.Attributes_Primary_Intelligence = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Primary.Intelligence")),
		FString(TEXT("智力,提高法术伤害和最大法力值")));

	GameplayTags.Attributes_Primary_Resilience = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Primary.Resilience")),
		FString(TEXT("韧性,提高护甲和护甲穿透")));

	GameplayTags.Attributes_Primary_Vigor = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Primary.Vigor")),
		FString(TEXT("活力,提高最大生命值")));

	/*
	 *Secondary Attributes
	 */
	
	GameplayTags.Attributes_Secondary_Armor = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.Armor")),
		FString(TEXT("护甲,减少伤害,提高格挡几率")));

	GameplayTags.Attributes_Secondary_ArmorPenetration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.ArmorPenetration")),
		FString(TEXT("护甲穿透,无视目标部分护甲并提高暴击几率")));

	GameplayTags.Attributes_Secondary_BlockChance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.BlockChance")),
		FString(TEXT("格挡几率,成功格挡时减少受到的伤害")));

	GameplayTags.Attributes_Secondary_CriticalHitChance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.CriticalHitChance")),
		FString(TEXT("暴击几率,决定攻击造成暴击的概率")));

	GameplayTags.Attributes_Secondary_CriticalHitDamage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.CriticalHitDamage")),
		FString(TEXT("暴击伤害,暴击时造成的额外伤害")));

	GameplayTags.Attributes_Secondary_CriticalHitResistance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.CriticalHitResistance")),
		FString(TEXT("暴击抗性,降低被暴击的概率")));

	GameplayTags.Attributes_Secondary_HealthRegeneration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.HealthRegeneration")),
		FString(TEXT("生命回复,每秒恢复的生命值")));

	GameplayTags.Attributes_Secondary_ManaRegeneration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.ManaRegeneration")),
		FString(TEXT("法力回复,每秒恢复的法力值")));

	GameplayTags.Attributes_Secondary_MaxHealth = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.MaxHealth")),
		FString(TEXT("最大生命值,决定生命值上限")));

	GameplayTags.Attributes_Secondary_MaxMana = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Secondary.MaxMana")),
		FString(TEXT("最大法力值,决定法力值上限")));

	GameplayTags.Attributes_Resistance_Fire = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Resistance.Fire")),
		FString(TEXT("火焰抗性")));

	GameplayTags.Attributes_Resistance_Lightning = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Resistance.Lightning")),
		FString(TEXT("闪电抗性")));

	GameplayTags.Attributes_Resistance_Arcane = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Resistance.Arcane")),
		FString(TEXT("奥术抗性")));

	GameplayTags.Attributes_Resistance_Physical = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Resistance.Physical")),
		FString(TEXT("物理抗性")));

	GameplayTags.Attributes_Resistance_Debuff = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Resistance.Debuff")),
		FString(TEXT("异常状态抗性")));

	GameplayTags.Attributes_Meta_IncomingXP = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Attributes.Meta.IncomingXP")),
		FString(TEXT("传入经验值元属性")));

	/*
	 *Input Tags
	 */
	
	GameplayTags.InputTag_RMB = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.RMB")),
		FString(TEXT("鼠标右键输入标签")));

	GameplayTags.InputTag_LMB = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.LMB")),
		FString(TEXT("鼠标左键输入标签")));

	GameplayTags.InputTag_1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.1")),
		FString(TEXT("数字键 1 输入标签")));

	GameplayTags.InputTag_2 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.2")),
		FString(TEXT("数字键 2 输入标签")));

	GameplayTags.InputTag_3 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.3")),
		FString(TEXT("数字键 3 输入标签")));

	GameplayTags.InputTag_4 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.4")),
		FString(TEXT("数字键 4 输入标签")));

	GameplayTags.InputTag_Passive_1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.Passive.1")),
		FString(TEXT("被动技能 1 输入标签")));

	GameplayTags.InputTag_Passive_2 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.Passive.2")),
		FString(TEXT("被动技能 2 输入标签")));

	GameplayTags.InputTag_Space = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("InputTag.Space")),
		FString(TEXT("空格 Dash 输入标签")));
	
	GameplayTags.Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Damage")),
		FString(TEXT("伤害标签")));

	GameplayTags.Damage_Fire = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Damage.Fire")),
		FString(TEXT("火焰伤害标签")));

	GameplayTags.Damage_Lightning = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Damage.Lightning")),
		FString(TEXT("闪电伤害标签")));

	GameplayTags.Damage_Arcane = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Damage.Arcane")),
		FString(TEXT("奥术伤害标签")));

	GameplayTags.Damage_Physical = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Damage.Physical")),
		FString(TEXT("物理伤害标签")));

	GameplayTags.Debuff_Burn = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Debuff.Burn")),
		FString(TEXT("燃烧减益标签")));

	GameplayTags.Debuff_Stun = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Debuff.Stun")),
		FString(TEXT("眩晕减益标签")));

	GameplayTags.Debuff_Arcane = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Debuff.Arcane")),
		FString(TEXT("奥术减益标签")));

	GameplayTags.Debuff_Physical = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Debuff.Physical")),
		FString(TEXT("物理减益标签")));

	GameplayTags.Debuff_Chance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Debuff.Chance")),
		FString(TEXT("减益触发概率标签")));

	GameplayTags.Debuff_Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Debuff.Damage")),
		FString(TEXT("减益伤害标签")));

	GameplayTags.Debuff_Duration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Debuff.Duration")),
		FString(TEXT("减益持续时间标签")));

	GameplayTags.Debuff_Frequency = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Debuff.Frequency")),
		FString(TEXT("减益触发频率标签")));

	GameplayTags.Abilities_None = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.None")),
		FString(TEXT("无技能标签")));

	GameplayTags.Abilities_Attack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Attack")),
		FString(TEXT("攻击技能标签")));

	GameplayTags.Abilities_Summon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Summon")),
		FString(TEXT("召唤技能标签")));

	GameplayTags.Abilities_Fire_FireBolt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Fire.FireBolt")),
		FString(TEXT("火球技能标签")));

	GameplayTags.Abilities_Movement_Dash = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Movement.Dash")),
		FString(TEXT("冲刺技能标签")));

	GameplayTags.Abilities_Fire_DelayedBlast = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Fire.DelayedBlast")),
		FString(TEXT("延迟爆炸技能标签")));

	GameplayTags.Abilities_Fire_FireNado = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Fire.FireNado")),
		FString(TEXT("烈火飓风技能标签")));

	GameplayTags.Abilities_Lightning_Electrocute = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Lightning.Electrocute")),
		FString(TEXT("闪电电击技能标签")));

	GameplayTags.Abilities_HitReact = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.HitReact")),
		FString(TEXT("技能受击反应标签")));

	GameplayTags.Abilities_Status_Locked = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Status.Locked")),
		FString(TEXT("技能锁定状态标签")));

	GameplayTags.Abilities_Status_Eligible = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Status.Eligible")),
		FString(TEXT("技能可解锁状态标签")));

	GameplayTags.Abilities_Status_Unlocked = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Status.Unlocked")),
		FString(TEXT("技能已解锁状态标签")));

	GameplayTags.Abilities_Status_Equipped = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Status.Equipped")),
		FString(TEXT("技能已装备状态标签")));

	GameplayTags.Abilities_Type_Offensive = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Type.Offensive")),
		FString(TEXT("攻击型技能类型标签")));

	GameplayTags.Abilities_Type_Passive = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Type.Passive")),
		FString(TEXT("被动型技能类型标签")));

	GameplayTags.Abilities_Type_None = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Abilities.Type.None")),
		FString(TEXT("无技能类型标签")));

	GameplayTags.Cooldown_Fire_FireBolt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Cooldown.Fire.FireBolt")),
		FString(TEXT("火球技能冷却标签")));

	GameplayTags.Cooldown_Fire_ChargedShot = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Cooldown.Fire.ChargedShot")),
		FString(TEXT("蓄力射击技能冷却标签")));

	GameplayTags.Cooldown_Fire_DelayedBlast = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Cooldown.Fire.DelayedBlast")),
		FString(TEXT("延迟爆破技能冷却标签")));

	GameplayTags.Cooldown_Fire_FireNado = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Cooldown.Fire.FireNado")),
		FString(TEXT("烈火飓风技能冷却标签")));

	GameplayTags.Cooldown_Lightning_Electrocute = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Cooldown.Lightning.Electrocute")),
		FString(TEXT("电击技能冷却标签")));

	GameplayTags.Cooldown_Movement_Dash = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Cooldown.Movement.Dash")),
		FString(TEXT("冲刺技能冷却标签")));

	/*
	 * 攻击蒙太奇序号标签
	 */
	GameplayTags.Montage_Attack_1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Montage.Attack.1")),
		FString(TEXT("攻击蒙太奇 1")));

	GameplayTags.Montage_Attack_2 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Montage.Attack.2")),
		FString(TEXT("攻击蒙太奇 2")));

	GameplayTags.Montage_Attack_3 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Montage.Attack.3")),
		FString(TEXT("攻击蒙太奇 3")));

	GameplayTags.Montage_Attack_4 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Montage.Attack.4")),
		FString(TEXT("攻击蒙太奇 4")));

	GameplayTags.Event_Montage_DelayBlast = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Event.Montage.DelayBlast")),
		FString(TEXT("延迟爆炸蒙太奇生成事件")));

	GameplayTags.Event_Montage_FireNado = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Event.Montage.FireNado")),
		FString(TEXT("烈火飓风蒙太奇生成事件")));

	GameplayTags.Event_Montage_Dash_Start = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Event.Montage.Dash.Start")),
		FString(TEXT("冲刺蒙太奇起手事件")));

	GameplayTags.State_Dash_Active = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("State.Dash.Active")),
		FString(TEXT("冲刺进行中状态标签")));

	/*
	 * 战斗插槽标签
	 */
	
	GameplayTags.CombatSocket_Weapon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("CombatSocket.Weapon")),
		FString(TEXT("使用武器攻击的战斗插槽标签")));

	GameplayTags.CombatSocket_RightHand = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("CombatSocket.RightHand")),
		FString(TEXT("使用右手攻击的战斗插槽标签")));

	GameplayTags.CombatSocket_LeftHand = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("CombatSocket.LeftHand")),
		FString(TEXT("使用左手攻击的战斗插槽标签")));

	GameplayTags.CombatSocket_Tail = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("CombatSocket.Tail")),
		FString(TEXT("使用尾巴攻击的战斗插槽标签")));

	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Fire, GameplayTags.Attributes_Resistance_Fire);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Lightning, GameplayTags.Attributes_Resistance_Lightning);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Arcane, GameplayTags.Attributes_Resistance_Arcane);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Physical, GameplayTags.Attributes_Resistance_Physical);

	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Fire, GameplayTags.Debuff_Burn);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Lightning, GameplayTags.Debuff_Stun);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Arcane, GameplayTags.Debuff_Arcane);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Physical, GameplayTags.Debuff_Physical);
	
	GameplayTags.Effects_HitReact = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("Effects.HitReact")),
		FString(TEXT("受击反应标签")));

	GameplayTags.GameplayCue_Debuff_Burn = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("GameplayCue.Debuff.Burn")),
		FString(TEXT("燃烧异常状态表现")));

	GameplayTags.GameplayCue_Dash_Start = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("GameplayCue.Dash.Start")),
		FString(TEXT("冲刺起手表现")));

	GameplayTags.GameplayCue_Dash_Trail = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("GameplayCue.Dash.Trail")),
		FString(TEXT("冲刺拖尾表现")));

	GameplayTags.GameplayCue_Dash_End = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("GameplayCue.Dash.End")),
		FString(TEXT("冲刺收尾表现")));

	GameplayTags.GameplayCue_Blink_Start = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("GameplayCue.Blink.Start")),
		FString(TEXT("闪现起点表现")));

	GameplayTags.GameplayCue_Blink_End = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TEXT("GameplayCue.Blink.End")),
		FString(TEXT("闪现落点表现")));
	
	
}

//真正创建这份全局唯一的 GameplayTags 对象
FAuraGameplayTags FAuraGameplayTags::GameplayTags;
