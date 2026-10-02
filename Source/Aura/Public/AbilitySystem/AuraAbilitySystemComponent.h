// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AuraAbilitySystemComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags, const FGameplayTagContainer&)
DECLARE_MULTICAST_DELEGATE(FAbilitiesGiven);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FAbilityStatusChanged, const FGameplayTag&, const FGameplayTag&,int32);
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec&)
DECLARE_MULTICAST_DELEGATE_FourParams(FAbilityEquipped, const FGameplayTag&, const FGameplayTag&,const FGameplayTag&, const FGameplayTag&);

class UAbilityInfo;
class ULoadScreenSaveGame;
/**
 * 
 */
UCLASS()
class AURA_API UAuraAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	//======== 委托与启动状态 ========
	FEffectAssetTags EffectAssetTags;
	FAbilitiesGiven AbilitiesGivenDelegate;
	FAbilityStatusChanged AbilityStatusChanged;
	FAbilityEquipped AbilityEquipped;
	bool bStartUpAbilitiesGiven=false;

	//======== 初始化与技能授予（服务器） ========
	void AbilityActorInfoSet();
	void AddAbilityToCharacter(const TArray<TSubclassOf<UGameplayAbility>>& StartUpAbilities);
	void AddPassiveAbilityToCharacter(const TArray<TSubclassOf<UGameplayAbility>>& StartUpPassiveAbilities);
	void AddCharacterAbilitiesFromSaveData(ULoadScreenSaveGame* SaveData);
	void UpdateAbilityStatuses(int32 Level);

	//======== 输入 ========
	void AbilityInputTagHeld(const FGameplayTag& Tag);
	void AbilityInputTagReleased(const FGameplayTag& Tag);

	//======== Spec 遍历与查询 ========
	void ForEachAbility(const FForEachAbility& Delegate);
	FGameplayAbilitySpec* GetSpecFromAbilityTag(const FGameplayTag& AbilityTag);
	static FGameplayTag GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	static FGameplayTag GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	static FGameplayTag GetStatusFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	FGameplayTag GetStatusFromAbilityTag(const FGameplayTag& AbilityTag);
	FGameplayTag GetInputTagFromAbilityTag(const FGameplayTag& AbilityTag);
	// AbilityInfo 由调用方（UI 层）提供：它是 UI 配置数据，且 GameMode 只存在于服务器，
	// 客户端无法通过 UAuraAbilitySystemLibrary::GetAbilityInfo() 取到。
	bool GetDescriptionsByAbilityTag(const FGameplayTag& AbilityTag, const UAbilityInfo* InAbilityInfo, FString& OutDescription, FString& OutNextLevelDescription);

	//======== 装备槽位 ========
	//需要 MarkAbilitySpecDirty 让槽位变更参与复制，因此不能是静态函数
	void ClearSlot(FGameplayAbilitySpec* Spec);
	static bool AbilityHasSlot(FGameplayAbilitySpec* Spec, const FGameplayTag& Slot);
	void ClearAbilitiesOfSlot(const FGameplayTag& Slot);

	//======== 属性升级与技能点：本地入口 ========
	void UpgradeAttribute(const FGameplayTag& AttributeTag);

	//======== 属性升级与技能点：Server RPC ========
	UFUNCTION(Server, Reliable)
	void ServerSpendSpellPoint(const FGameplayTag& AbilityTag);

	UFUNCTION(Server, Reliable)
	void ServerEquipAbility(const FGameplayTag&AbilityTag,const FGameplayTag& Slot);

protected:
	//======== 复制回调 ========
	virtual void OnRep_ActivateAbilities()override;

	//======== Server RPC ========
	UFUNCTION(Server, Reliable)
	void ServerUpgradeAttribute(const FGameplayTag& AttributeTag);

	//======== Client RPC ========
	//将它变成一个客户端RPC,负责客户端不能实现消息同步
	UFUNCTION(Client, Reliable)
	void Client_EffectAppllied(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayEffectSpec& GameplayEffectSpec,FActiveGameplayEffectHandle ActiveGameplayEffectHandle);

	UFUNCTION(Client, Reliable)
	void ClientUpdateAbilityStatus(const FGameplayTag& AbilityTag, const FGameplayTag& StatusTag,int32 AbilityLevel);

	//装备结果由服务器权威下发给拥有端；若不是 Client RPC，专用服务器上客户端 UI 收不到任何通知
	UFUNCTION(Client, Reliable)
	void ClientEquipAbility(const FGameplayTag& AbilityTag, const FGameplayTag& Status,
		const FGameplayTag& Slot, const FGameplayTag& PreviousSlot);
};
