// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * 
 */
struct FAuraGameplayTags
{
public:
	static const FAuraGameplayTags& Get(){return  GameplayTags;}
	static  void InitializedNativeGameplayTags();
//主要属性
	FGameplayTag Attributes_Primary_Strength;
	FGameplayTag Attributes_Primary_Intelligence;
	FGameplayTag Attributes_Primary_Resilience;
	FGameplayTag Attributes_Primary_Vigor;
//次要属性
	FGameplayTag Attributes_Secondary_Armor;
	FGameplayTag Attributes_Secondary_ArmorPenetration;
	FGameplayTag Attributes_Secondary_BlockChance;
	FGameplayTag Attributes_Secondary_CriticalHitChance;
	FGameplayTag Attributes_Secondary_CriticalHitDamage;
	FGameplayTag Attributes_Secondary_CriticalHitResistance;
	FGameplayTag Attributes_Secondary_HealthRegeneration;
	FGameplayTag Attributes_Secondary_ManaRegeneration;
	FGameplayTag Attributes_Secondary_MaxHealth;
	FGameplayTag Attributes_Secondary_MaxMana;

	FGameplayTag Attributes_Resistance_Fire;
	FGameplayTag Attributes_Resistance_Lightning;
	FGameplayTag Attributes_Resistance_Arcane;
	FGameplayTag Attributes_Resistance_Physical;
	FGameplayTag Attributes_Resistance_Debuff;

	FGameplayTag Attributes_Meta_IncomingXP;
	
//输入属性
	
	FGameplayTag InputTag_RMB;
	FGameplayTag InputTag_LMB;
	FGameplayTag InputTag_1;
	FGameplayTag InputTag_2;
	FGameplayTag InputTag_3;
	FGameplayTag InputTag_4;
	FGameplayTag InputTag_Passive_1;
	FGameplayTag InputTag_Passive_2;
	FGameplayTag InputTag_Space;
	
	FGameplayTag Damage;
	FGameplayTag Damage_Fire;
	FGameplayTag Damage_Lightning;
	FGameplayTag Damage_Arcane;
	FGameplayTag Damage_Physical;

	FGameplayTag Debuff_Burn;
	// Reserved: no consumer implements the Stun / Arcane / Physical debuffs yet.
	FGameplayTag Debuff_Stun;
	FGameplayTag Debuff_Arcane;
	FGameplayTag Debuff_Physical;
	FGameplayTag Debuff_Chance;
	FGameplayTag Debuff_Damage;
	FGameplayTag Debuff_Duration;
	FGameplayTag Debuff_Frequency;
	
	FGameplayTag Abilities_None;
	FGameplayTag Abilities_Attack;
	FGameplayTag Abilities_Summon;
	FGameplayTag Abilities_Fire_FireBolt;
	FGameplayTag Abilities_Fire_DelayedBlast;
	FGameplayTag Abilities_Fire_FireNado;
	FGameplayTag Abilities_Lightning_Electrocute;
	FGameplayTag Abilities_Movement_Dash;
	FGameplayTag Abilities_HitReact;

	FGameplayTag Abilities_Status_Locked;
	FGameplayTag Abilities_Status_Eligible;
	FGameplayTag Abilities_Status_Unlocked;
	FGameplayTag Abilities_Status_Equipped;

	FGameplayTag Abilities_Type_Offensive;
	FGameplayTag Abilities_Type_Passive;
	FGameplayTag Abilities_Type_None;

	FGameplayTag Cooldown_Fire_FireBolt;
	FGameplayTag Cooldown_Fire_ChargedShot;
	FGameplayTag Cooldown_Fire_DelayedBlast;
	FGameplayTag Cooldown_Fire_FireNado;
	FGameplayTag Cooldown_Lightning_Electrocute;
	FGameplayTag Cooldown_Movement_Dash;

	FGameplayTag Montage_Attack_1;
	FGameplayTag Montage_Attack_2;
	FGameplayTag Montage_Attack_3;
	FGameplayTag Montage_Attack_4;

	FGameplayTag Event_Montage_DelayBlast;
	FGameplayTag Event_Montage_FireNado;
	FGameplayTag Event_Montage_Dash_Start;

	// 冲刺进行中状态：控制器用它拦截移动输入，Dash 技能在其期间不叠加，避免二次位移。
	FGameplayTag State_Dash_Active;

	FGameplayTag CombatSocket_Weapon;
	FGameplayTag CombatSocket_RightHand;
	FGameplayTag CombatSocket_LeftHand;
	FGameplayTag CombatSocket_Tail;
	
	TMap<FGameplayTag, FGameplayTag> DamageTypesToResistances;
	TMap<FGameplayTag, FGameplayTag> DamageTypesToDebuffs;

	FGameplayTag Effects_HitReact;
	FGameplayTag GameplayCue_Debuff_Burn;
	FGameplayTag GameplayCue_Dash_Start;
	FGameplayTag GameplayCue_Dash_Trail;
	FGameplayTag GameplayCue_Dash_End;
	FGameplayTag GameplayCue_Blink_Start;
	FGameplayTag GameplayCue_Blink_End;
	
	
private:
	static 	FAuraGameplayTags GameplayTags;
};
