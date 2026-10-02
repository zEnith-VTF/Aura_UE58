// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/OverlayWidgetController.h"
#include "Input/AuraPlayerState.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/Data/LevelUpInfo.h"
#include "Tags/AuraGameplayTags.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
	//初始化函数
	OnHealthChanged.Broadcast(GetAuraAS()->GetHealth());
	OnMaxHealthChanged.Broadcast(GetAuraAS()->GetMaxHealth());
	OnManaChanged.Broadcast(GetAuraAS()->GetMana());
	OnMaxManaChanged.Broadcast(GetAuraAS()->GetMaxMana());

	OnXPChanged(GetAuraPS()->GetXP());
	OnPlayerLevelChangedDelegate.Broadcast(GetAuraPS()->GetPlayerLevel());
	// HUD calls this after the Overlay and its child globes have their controller.
	if (GetAuraASC()->bStartUpAbilitiesGiven)
	{
		BroadcastAbilityInfo();
	}
}

void UOverlayWidgetController::BindCallbacksToDependencies()
{
	//当后续函数改变时调用
	GetAuraPS()->OnXPChangedDelegate.AddUObject(this,&UOverlayWidgetController::OnXPChanged);
	GetAuraPS()->OnLevelChangedDelegate.AddLambda(
	[this](int32 NewValue)
	{
		OnPlayerLevelChangedDelegate.Broadcast(NewValue);
	}	
	);
	
	//绑定内部属性变化委托
	GetAuraASC()->GetGameplayAttributeValueChangeDelegate(GetAuraAS()->GetHealthAttribute()).AddLambda(
	[this](const FOnAttributeChangeData& Data)
		{
			OnHealthChanged.Broadcast(Data.NewValue);
		}	
	);
	GetAuraASC()->GetGameplayAttributeValueChangeDelegate(GetAuraAS()->GetMaxHealthAttribute()).AddLambda(
	[this](const FOnAttributeChangeData& Data)
		{
			OnMaxHealthChanged.Broadcast(Data.NewValue);	
		}	
	);
	GetAuraASC()->GetGameplayAttributeValueChangeDelegate(GetAuraAS()->GetManaAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnManaChanged.Broadcast(Data.NewValue);
		}
	);
	GetAuraASC()->GetGameplayAttributeValueChangeDelegate(GetAuraAS()->GetMaxManaAttribute()).AddLambda(
	[this](const FOnAttributeChangeData& Data)
		{
			OnMaxManaChanged.Broadcast(Data.NewValue);
		}
	);
	if (!GetAuraASC()->bStartUpAbilitiesGiven)
	{
		GetAuraASC()->AbilitiesGivenDelegate.AddUObject(this, &UOverlayWidgetController::BroadcastAbilityInfo);
	}
	//SpellMenu 里换槽后 HUD 技能栏必须跟着变，否则要等下一次 BroadcastInitialValues 才刷新
	GetAuraASC()->AbilityEquipped.AddUObject(this, &UOverlayWidgetController::OnAbilityEquipped);
	//采用 Lambda 来代替 Dynamic
	GetAuraASC()->EffectAssetTags.AddLambda(
		[this](const FGameplayTagContainer& AssetTags)
		{
			for (auto Tag:AssetTags)
			{
				FGameplayTag MessageTag=FGameplayTag::RequestGameplayTag("Message");
				if (Tag.MatchesTag(MessageTag))
				{
					//GetDataTableRowByTag用当前 Tag 的名字去 DataTable 里找一行
					const FUIWidgetRow *Row= GetDataTableRowByTag<FUIWidgetRow>(MessageWidgetTable,Tag);
					MessageWidgetRowDelegate.Broadcast(*Row);
				}
		
				
			}
		} 	
		);
}

void UOverlayWidgetController::OnXPChanged(int32 NewXP)
{
	ULevelUpInfo* LevelUpInfo=GetAuraPS()->LevelUpInfo;
	const int32 NewLevel =LevelUpInfo->FindLevelForXP(NewXP);
	TArray<FAuraLevelUpInfo> LevelUpInformation=LevelUpInfo->LevelUpInformation;
	int32 NextXP=LevelUpInformation[NewLevel].LevelUpRequirement;
	int32 LastXP=LevelUpInformation[NewLevel-1].LevelUpRequirement;
	float Percent =static_cast<float>(NewXP - LastXP) /static_cast<float>(NextXP - LastXP);
	OnXPPercentChangedDelegate.Broadcast(Percent);
}

void UOverlayWidgetController::OnAbilityEquipped(const FGameplayTag& AbilityTag, const FGameplayTag& Status,
	const FGameplayTag& Slot, const FGameplayTag& PreviousSlot)
{
	if (!IsValid(AbilityInfo)) return;

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	// 先清空旧槽位。AbilityTag 用 Abilities.None 作为"该槽位已空"的信号，
	// 与 SpellMenu 侧保持同一份数据契约，UMG 依据它决定是否恢复空槽外观、不要覆盖底框。
	FAuraAbilityInfo LastSlotInfo;
	LastSlotInfo.StatusTag = GameplayTags.Abilities_Status_Unlocked;
	LastSlotInfo.InputTag = PreviousSlot;
	LastSlotInfo.AbilityTag = GameplayTags.Abilities_None;
	AbilityInfoDelegate.Broadcast(LastSlotInfo);

	// 再刷新新槽位。Status 由服务器下发，此处不再从 Spec 重新读取。
	FAuraAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(AbilityTag);
	Info.StatusTag = Status;
	Info.InputTag = Slot;
	AbilityInfoDelegate.Broadcast(Info);
}





