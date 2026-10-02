// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AuraAttributeSet.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "GameFramework/Character.h"
#include "GameplayEffectExtension.h"
#include "Aura/AuraLogChannels.h"
#include "Interfaction/CombatInterface.h"
#include "Input/AuraPlayerController.h"
#include "Interfaction/PlayerInterface.h"
#include "Net/UnrealNetwork.h"
#include "Tags/AuraGameplayTags.h"
#include "AbilitySystem/Effects/AuraBurnEffect.h"
#include "GameplayEffect.h"


UAuraAttributeSet::UAuraAttributeSet()
{
	const FAuraGameplayTags& Tags=FAuraGameplayTags::Get();
	/*
	 *主要属性
	 */
	TagToAttributeMap.Add(Tags.Attributes_Primary_Strength,GetStrengthAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Primary_Intelligence,GetIntelligenceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Primary_Resilience,GetResilienceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Primary_Vigor,GetVigorAttribute);

	/*
	 *次要属性
	 */
	TagToAttributeMap.Add(Tags.Attributes_Secondary_Armor,GetArmorAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_ArmorPenetration,GetArmorPenetrationAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_BlockChance,GetBlockChanceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_CriticalHitChance,GetCriticalHitChanceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_CriticalHitDamage,GetCriticalHitDamageAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_CriticalHitResistance,GetCriticalHitResistanceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_HealthRegeneration,GetHealthRegenerationAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_ManaRegeneration,GetManaRegenerationAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_MaxHealth,GetMaxHealthAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Secondary_MaxMana,GetMaxManaAttribute);

	/*
	 *次要属性->抗性属性
	 */
	TagToAttributeMap.Add(Tags.Attributes_Resistance_Fire, GetFireResistanceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Resistance_Lightning, GetLightningResistanceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Resistance_Arcane, GetArcaneResistanceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Resistance_Physical, GetPhysicalResistanceAttribute);
	TagToAttributeMap.Add(Tags.Attributes_Resistance_Debuff, GetDebuffResistanceAttribute);

}
void UAuraAttributeSet::PreAttributeChange(
	const FGameplayAttribute& Attribute,
	float& NewValue
)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}

	if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}

	if (Attribute == GetDebuffResistanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	}
}

void UAuraAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute() && bTopOffHealth)
	{
		SetHealth(GetMaxHealth());
		bTopOffHealth = false;
	}
	if (Attribute == GetMaxManaAttribute() && bTopOffMana)
	{
		SetMana(GetMaxMana());
		bTopOffMana = false;
	}
}

void UAuraAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//最后的Always代表每一次服务器修改都将调用OnRep
	//主要属性
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,Strength,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,Intelligence,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,Resilience,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,Vigor,COND_None,REPNOTIFY_Always);
	//次要属性
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,Armor,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,ArmorPenetration,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,BlockChance,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,CriticalHitChance,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,CriticalHitDamage,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,CriticalHitResistance,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,HealthRegeneration,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,ManaRegeneration,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,MaxMana,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,MaxHealth,COND_None,REPNOTIFY_Always);
	//抗性属性
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,FireResistance,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,LightningResistance,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,ArcaneResistance,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,PhysicalResistance,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,DebuffResistance,COND_None,REPNOTIFY_Always);
	//重要属性
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,Health,COND_None,REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAuraAttributeSet,Mana,COND_None,REPNOTIFY_Always);
}
//目的只有一个：获取各方面属性
void UAuraAttributeSet::SetEffectProperties(const struct FGameplayEffectModCallbackData& Data,
	FEffectProperties& Props)const

{
		Props.EffectContextHandle=Data.EffectSpec.GetContext();
    	Props.SourceASC=Props.EffectContextHandle.GetOriginalInstigatorAbilitySystemComponent();
    	//拿取effect发射方
    	if (Props.SourceASC && Props.SourceASC->AbilityActorInfo.IsValid() && Props.SourceASC->AbilityActorInfo->AvatarActor.IsValid())
    	{
    		Props.SourceAvatarActor=Props.SourceASC->AbilityActorInfo->AvatarActor.Get();
    		Props.SourceController = Props.SourceASC->AbilityActorInfo->PlayerController.Get();
    		
    		if (Props.SourceController==nullptr &&  Props.SourceAvatarActor!=nullptr)
    		{
    			if (APawn* Pawn=Cast<APawn>(Props.SourceAvatarActor))
    			{
    				Props.SourceController=Pawn->GetController();
    			}
    		}
    		if (Props.SourceController)
    		{
    			Props.SourceCharacter=Cast<ACharacter>(Props.SourceController->GetPawn());
    		}
    		
    	}
    	//拿取effect目标方
    	if (Data.Target.AbilityActorInfo && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
    	{
    		Props.TargetAvatarActor=Data.Target.AbilityActorInfo->AvatarActor.Get();
    		Props.TargetController=Data.Target.AbilityActorInfo->PlayerController.Get();
    		Props.TargetCharacter=Cast<ACharacter>(Props.TargetAvatarActor);
    		//可以通过UAbilitySystemBlueprintLibrary来获得ASC
    		Props.TargetASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Props.TargetAvatarActor);
    		
    	}

}

void UAuraAttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	//Data 里面保存了这次 GameplayEffect 修改属性时的相关信息。
	Super::PostGameplayEffectExecute(Data);
	FEffectProperties Props;
	SetEffectProperties(Data,Props);

	if (Data.EvaluatedData.Attribute==GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(),0.f,GetMaxHealth()));
	}
	if (Data.EvaluatedData.Attribute==GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(),0.f,GetMaxMana()));
	}
	if (Data.EvaluatedData.Attribute==GetIncomingDamageAttribute())
	{
		const float LocalIncomingDamage=GetIncomingDamage();
		SetIncomingDamage(0.f);
		if (Props.TargetAvatarActor && Props.TargetAvatarActor->Implements<UCombatInterface>()
			&& ICombatInterface::Execute_IsDead(Props.TargetAvatarActor))
		{
			return;
		}
		if (LocalIncomingDamage>0.f)
		{
			// Burn period tick recognition: the periodic burn effect must skip crit / block / HitReact handling and
			// re-entrant debuff application. Only Burn is implemented today; do not generalize this to all Debuff.* tags.
			const bool bBurnPeriod = Data.EffectSpec.Def && Data.EffectSpec.Def->IsA(UAuraBurnEffect::StaticClass());
			const bool bBlockedHit = !bBurnPeriod && UAuraAbilitySystemLibrary::IsBlockedHit(Props.EffectContextHandle);
			const bool bCriticalHit = !bBurnPeriod && UAuraAbilitySystemLibrary::IsCriticalHit(Props.EffectContextHandle);
			ShowFloatingText(Props, LocalIncomingDamage, bBlockedHit, bCriticalHit);
			
			const float NewHealth=GetHealth()-LocalIncomingDamage;
			SetHealth(FMath::Clamp(NewHealth,0.f,GetMaxHealth()));
			
			const bool bFatal =NewHealth<=0.f;
			if (bFatal)
			{
				if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Props.TargetAvatarActor))
				{
					CombatInterface->Die();
					SendXPEvent(Props);
				}
			}
			else if (!bBurnPeriod && Props.TargetASC)
			{
				FGameplayTagContainer TagContainer;
				TagContainer.AddTag(FAuraGameplayTags::Get().Effects_HitReact);
				Props.TargetASC->TryActivateAbilitiesByTag(TagContainer);
				TryApplyBurnFromDirectHit(Data, Props);
			}
		}
	}
	if (Data.EvaluatedData.Attribute==GetIncomingXPAttribute())
	{
		const float XP=GetIncomingXP();
		SetIncomingXP(0.f);
		if (IsValid(Props.SourceCharacter) &&
			Props.SourceCharacter->Implements<UPlayerInterface>() &&
			Props.SourceCharacter->Implements<UCombatInterface>())
		{
			const int32 CurrentLevel = ICombatInterface::Execute_GetPlayerLevel(Props.SourceCharacter);
			const int32 CurrentXP = IPlayerInterface::Execute_GetXP(Props.SourceCharacter);
			const int32 NewLevel = IPlayerInterface::Execute_FindLevelForXP(
				Props.SourceCharacter,
				CurrentXP + XP);
			const int32 NumLevelUps = NewLevel - CurrentLevel;
			
			if (NumLevelUps>0)
			{
				int32 AttributePointsReward = 0;
				int32 SpellPointsReward = 0;
				for (int32 RewardLevel = CurrentLevel; RewardLevel < NewLevel; ++RewardLevel)
				{
					AttributePointsReward += IPlayerInterface::Execute_GetAttributePointsReward(
						Props.SourceCharacter,
						RewardLevel);
					SpellPointsReward += IPlayerInterface::Execute_GetSpellPointsReward(
						Props.SourceCharacter,
						RewardLevel);
				}
			
				IPlayerInterface::Execute_AddToPlayerLevel(Props.SourceCharacter,NumLevelUps);
				IPlayerInterface::Execute_AddToAttributePoints(
					Props.SourceCharacter,
					AttributePointsReward);
				IPlayerInterface::Execute_AddToSpellPoints(
					Props.SourceCharacter,
					SpellPointsReward);
				
				bTopOffHealth = true;
				bTopOffMana = true;
				
				IPlayerInterface::Execute_LevelUp(Props.SourceCharacter);
			}
			
			IPlayerInterface::Execute_AddToXP(Props.SourceCharacter,XP);
		}
	}
}

void UAuraAttributeSet::SendXPEvent(const FEffectProperties& Props)
{
	if (Props.SourceCharacter == nullptr)
	{
		return;
	}

	if (Props.TargetCharacter && Props.TargetCharacter->Implements<UCombatInterface>())
	{
		const int32 TargetLevel = ICombatInterface::Execute_GetPlayerLevel(Props.TargetCharacter);
		const ECharacterClass TargetClass = ICombatInterface::Execute_GetCharacterClass(Props.TargetCharacter);
		const int32 XPReward = UAuraAbilitySystemLibrary::GetXPRewardForClassAndLevel(
			Props.TargetCharacter,
			TargetClass,
			TargetLevel);

		const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
		FGameplayEventData Payload;
		Payload.EventTag = GameplayTags.Attributes_Meta_IncomingXP;
		Payload.EventMagnitude = XPReward;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Props.SourceCharacter,
			GameplayTags.Attributes_Meta_IncomingXP,
			Payload);
	}
}

void UAuraAttributeSet::ShowFloatingText(const FEffectProperties& Props, float Damage,bool IsBlocked,bool IsCriticalHit) const
{
	if (Props.SourceCharacter && Props.TargetCharacter && Props.SourceCharacter != Props.TargetCharacter)
	{
		if (AAuraPlayerController* PC = Cast<AAuraPlayerController>(Props.SourceCharacter->Controller))
		{
			PC->ShowDamageNumber(Damage, Props.TargetCharacter,IsBlocked,IsCriticalHit);
			return;
		}

		if (AAuraPlayerController* PC = Cast<AAuraPlayerController>(Props.TargetCharacter->Controller))
		{
			PC->ShowDamageNumber(Damage, Props.TargetCharacter,IsBlocked,IsCriticalHit);
		}
	}
}

void UAuraAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)const
{
	//如果不写这个宏，属性数值可能复制过来了，但 GAS 的一些监听回调、UI 更新逻辑可能不会正常触发,方便预测功能触发
	//检测到预测偏差，及时平滑修正
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,Health,OldHealth);
}

void UAuraAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,MaxHealth,OldMaxHealth);

}

void UAuraAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,Mana,OldMana);
}

void UAuraAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,MaxMana,OldMaxMana);
}

void UAuraAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldStrength) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,Strength,OldStrength);
}

void UAuraAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldIntelligence) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,Intelligence,OldIntelligence);
}

void UAuraAttributeSet::OnRep_Resilience(const FGameplayAttributeData& OldResilience) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,Resilience,OldResilience);
}

void UAuraAttributeSet::OnRep_Vigor(const FGameplayAttributeData& OldVigor) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,Vigor,OldVigor);
}

void UAuraAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,Armor,OldArmor);
}

void UAuraAttributeSet::OnRep_ArmorPenetration(const FGameplayAttributeData& OldArmorPenetration) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,ArmorPenetration,OldArmorPenetration);
}

void UAuraAttributeSet::OnRep_BlockChance(const FGameplayAttributeData& OldBlockChance) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,BlockChance,OldBlockChance);
}

void UAuraAttributeSet::OnRep_CriticalHitChance(const FGameplayAttributeData& OldCriticalHitChance) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,CriticalHitChance,OldCriticalHitChance);
}

void UAuraAttributeSet::OnRep_CriticalHitDamage(const FGameplayAttributeData& OldCriticalHitDamage) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,CriticalHitDamage,OldCriticalHitDamage);
}

void UAuraAttributeSet::OnRep_CriticalHitResistance(const FGameplayAttributeData& OldCriticalHitResistance) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,CriticalHitResistance,OldCriticalHitResistance);
}

void UAuraAttributeSet::OnRep_HealthRegeneration(const FGameplayAttributeData& OldHealthRegeneration) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,HealthRegeneration,OldHealthRegeneration);
}

void UAuraAttributeSet::OnRep_ManaRegeneration(const FGameplayAttributeData& OldManaRegeneration) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,ManaRegeneration,OldManaRegeneration);
}

void UAuraAttributeSet::OnRep_FireResistance(const FGameplayAttributeData& OldFireResistance) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,FireResistance,OldFireResistance);
}

void UAuraAttributeSet::OnRep_LightningResistance(const FGameplayAttributeData& OldLightningResistance) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,LightningResistance,OldLightningResistance);
}

void UAuraAttributeSet::OnRep_ArcaneResistance(const FGameplayAttributeData& OldArcaneResistance) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,ArcaneResistance,OldArcaneResistance);
}

void UAuraAttributeSet::OnRep_PhysicalResistance(const FGameplayAttributeData& OldPhysicalResistance) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,PhysicalResistance,OldPhysicalResistance);
}

void UAuraAttributeSet::OnRep_DebuffResistance(const FGameplayAttributeData& OldDebuffResistance) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAuraAttributeSet,DebuffResistance,OldDebuffResistance);
}

float UAuraAttributeSet::CalculateDebuffTriggerChance(float TableChance, float InDebuffResistance)
{
	const float ClampedChance = FMath::Clamp(TableChance, 0.f, 100.f);
	const float ClampedResistance = FMath::Clamp(InDebuffResistance, 0.f, 100.f);
	return ClampedChance * (1.f - ClampedResistance / 100.f);
}

void UAuraAttributeSet::TryApplyBurnFromDirectHit(const FGameplayEffectModCallbackData& Data, const FEffectProperties& Props)
{
	if (!Props.TargetASC || !Props.SourceASC || !Props.TargetASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();

	// Burn is currently the only implemented damage-type debuff. The Stun / Arcane / Physical entries in
	// DamageTypesToDebuffs are reserved mappings that this function does not consume.
	if (Data.EffectSpec.GetSetByCallerMagnitude(Tags.Damage_Fire, false) <= 0.f)
	{
		return;
	}

	const float Chance = Data.EffectSpec.GetSetByCallerMagnitude(Tags.Debuff_Chance, false);
	const float BurnDamage = Data.EffectSpec.GetSetByCallerMagnitude(Tags.Debuff_Damage, false);
	const float Duration = Data.EffectSpec.GetSetByCallerMagnitude(Tags.Debuff_Duration, false);
	const float Frequency = Data.EffectSpec.GetSetByCallerMagnitude(Tags.Debuff_Frequency, false);
	if (!FMath::IsFinite(Chance) || Chance <= 0.f
		|| !FMath::IsFinite(BurnDamage) || BurnDamage < 0.f
		|| !FMath::IsFinite(Duration) || Duration <= 0.f
		|| !FMath::IsFinite(Frequency) || Frequency <= 0.f)
	{
		return;
	}

	const float EffectiveChance = CalculateDebuffTriggerChance(Chance, GetDebuffResistance());
	if (EffectiveChance <= 0.f || FMath::FRand() * 100.f >= EffectiveChance)
	{
		return;
	}

	ApplyOrRefreshBurn(const_cast<UAbilitySystemComponent*>(Props.SourceASC), Props.TargetASC, BurnDamage, Duration, Frequency);
}

void UAuraAttributeSet::ApplyOrRefreshBurn(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	float BurnDamage,
	float Duration,
	float Frequency)
{
	if (!IsValid(SourceASC) || !IsValid(TargetASC))
	{
		return;
	}

	float AppliedDamage = BurnDamage;
	float AppliedFrequency = Frequency;
	// Intentional manual copy on refresh: a same-source re-apply must keep the existing Burn's damage / frequency
	// and the period progress it already accumulated. Do not replace this loop with "let stacking refresh it"
	// without runtime evidence - the stacking configuration alone does not preserve those values.
	const FGameplayTagContainer BurnTags(FAuraGameplayTags::Get().Debuff_Burn);
	for (const FActiveGameplayEffectHandle& Handle : TargetASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(BurnTags)))
	{
		const FActiveGameplayEffect* const Effect = TargetASC->GetActiveGameplayEffect(Handle);
		if (!Effect || !Effect->Spec.Def || !Effect->Spec.Def->IsA(UAuraBurnEffect::StaticClass()))
		{
			continue;
		}
		if (Effect->Spec.GetEffectContext().GetInstigatorAbilitySystemComponent() != SourceASC)
		{
			continue;
		}
		AppliedDamage = Effect->Spec.GetSetByCallerMagnitude(FAuraGameplayTags::Get().Debuff_Damage, false);
		if (Effect->Spec.Period > 0.f)
		{
			AppliedFrequency = Effect->Spec.Period;
		}
		break;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(UAuraBurnEffect::StaticClass(), 1.f, Context);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetDuration(Duration, true);
	SpecHandle.Data->Period = AppliedFrequency;
	SpecHandle.Data->SetSetByCallerMagnitude(FAuraGameplayTags::Get().Debuff_Damage, AppliedDamage);
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}


