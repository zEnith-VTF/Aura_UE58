// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AuraAbilitySystemComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "Aura/AuraLogChannels.h"
#include "GameMode/LoadScreenSaveGame.h"
#include "Input/AuraPlayerState.h"
#include "Interfaction/PlayerInterface.h"
#include "Rendering/SkeletalMeshVertexBuffer.h"
#include "Tags/AuraGameplayTags.h"

void UAuraAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this,&UAuraAbilitySystemComponent::Client_EffectAppllied);
}
//对象是自身 
void UAuraAbilitySystemComponent::Client_EffectAppllied_Implementation(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayEffectSpec& GameplayEffectSpec, FActiveGameplayEffectHandle ActiveGameplayEffectHandle)
{
	FGameplayTagContainer TagContainer;
	GameplayEffectSpec.GetAllAssetTags(TagContainer);
	EffectAssetTags.Broadcast(TagContainer);

}

void UAuraAbilitySystemComponent::ClientUpdateAbilityStatus_Implementation(const FGameplayTag& AbilityTag,
	const FGameplayTag& StatusTag,int32 AbilityLevel)
{
	AbilityStatusChanged.Broadcast(AbilityTag, StatusTag,AbilityLevel);
}

void UAuraAbilitySystemComponent::AddAbilityToCharacter(const TArray<TSubclassOf<UGameplayAbility>>& StartUpAbilities)
{
	for (const auto Ability:StartUpAbilities)
	{
		if (!Ability || GetActivatableAbilities().ContainsByPredicate([Ability](const FGameplayAbilitySpec& Spec)
		{
			return Spec.Ability && Spec.Ability->GetClass() == Ability.Get();
		})) continue;

		FGameplayAbilitySpec AbilitySpec =FGameplayAbilitySpec(Ability,1);
		if (const UAuraGameplayAbility* GameplayAbility=Cast<UAuraGameplayAbility>(AbilitySpec.Ability))
		{
			//把技能默认对象上的 StartUpInputTag 加到这个 AbilitySpec 的动态标签里
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(GameplayAbility->StartUpInputTag);
			//新授予的主动技能初始状态为 Equipped，并随 AbilitySpec 复制到客户端
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Equipped);
			//把这个技能正式授予当前 AbilitySystemComponent
			GiveAbility(AbilitySpec);
		}
	}
	bStartUpAbilitiesGiven=true;
	AbilitiesGivenDelegate.Broadcast();
}

void UAuraAbilitySystemComponent::AddPassiveAbilityToCharacter(
	const TArray<TSubclassOf<UGameplayAbility>>& StartUpPassiveAbilities)
{
	for (const auto Ability:StartUpPassiveAbilities)
	{
		if (!Ability || GetActivatableAbilities().ContainsByPredicate([Ability](const FGameplayAbilitySpec& Spec)
		{
			return Spec.Ability && Spec.Ability->GetClass() == Ability.Get();
		})) continue;

		FGameplayAbilitySpec AbilitySpec =FGameplayAbilitySpec(Ability,1);
		GiveAbilityAndActivateOnce(AbilitySpec);
	}
}

void UAuraAbilitySystemComponent::AddCharacterAbilitiesFromSaveData(ULoadScreenSaveGame* SaveData)
{
	if (!IsValid(SaveData) || !GetOwnerActor() || !GetOwnerActor()->HasAuthority()) return;

	TSet<FGameplayTag> RestoredAbilityTags;
	for (int32 Index = SaveData->SavedAbilities.Num() - 1; Index >= 0; --Index)
	{
		const FSavedAbility& Data = SaveData->SavedAbilities[Index];
		if (!Data.GameplayAbility || !Data.AbilityTag.IsValid() || RestoredAbilityTags.Contains(Data.AbilityTag)) continue;
		if (Data.AbilityType != FAuraGameplayTags::Get().Abilities_Type_Offensive &&
			Data.AbilityType != FAuraGameplayTags::Get().Abilities_Type_Passive) continue;

		const TSubclassOf<UGameplayAbility> LoadedAbilityClass = Data.GameplayAbility;

		FGameplayAbilitySpec LoadedAbilitySpec = FGameplayAbilitySpec(LoadedAbilityClass, FMath::Max(1, Data.AbilityLevel));

		if (Data.AbilitySlot.IsValid()) LoadedAbilitySpec.GetDynamicSpecSourceTags().AddTag(Data.AbilitySlot);
		if (Data.AbilityStatus.IsValid()) LoadedAbilitySpec.GetDynamicSpecSourceTags().AddTag(Data.AbilityStatus);
		if (Data.AbilityType == FAuraGameplayTags::Get().Abilities_Type_Offensive)
		{
			GiveAbility(LoadedAbilitySpec);
		}
		else if (Data.AbilityType == FAuraGameplayTags::Get().Abilities_Type_Passive)
		{
			GiveAbilityAndActivateOnce(LoadedAbilitySpec);
		}
		RestoredAbilityTags.Add(Data.AbilityTag);
	}
}

void UAuraAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())return;
	for (auto & AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive())
			{
				//激活技能
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
		
		
}

void UAuraAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())return;
	for (auto & AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(Tag))
		{
			AbilitySpecInputReleased(AbilitySpec);
		}
	}
}

void UAuraAbilitySystemComponent::ForEachAbility(const FForEachAbility& Delegate)
{
	//先将列表技能锁住，防止期间Ability状态变化产生影响
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!Delegate.ExecuteIfBound(AbilitySpec))
		{
			UE_LOG(LogAura, Error, TEXT("Failed to execute delegate in %hs"), __FUNCTION__);
		}
	}
}

FGameplayAbilitySpec* UAuraAbilitySystemComponent::GetSpecFromAbilityTag(const FGameplayTag& AbilityTag)
{
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability) continue;

		for (const FGameplayTag& Tag : AbilitySpec.Ability->GetAssetTags())
		{
			if (Tag.MatchesTag(AbilityTag))
			{
				return &AbilitySpec;
			}
		}
	}

	return nullptr;
}

bool UAuraAbilitySystemComponent::GetDescriptionsByAbilityTag(
	const FGameplayTag& AbilityTag, const UAbilityInfo* InAbilityInfo,
	FString& OutDescription, FString& OutNextLevelDescription)
{
	OutDescription.Empty();
	OutNextLevelDescription.Empty();

	if (FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
	{
		if (UAuraGameplayAbility* AuraAbility = Cast<UAuraGameplayAbility>(AbilitySpec->Ability))
		{
			OutDescription = AuraAbility->GetDescription(AbilitySpec->Level);
			OutNextLevelDescription = AuraAbility->GetNextLevelDescription(AbilitySpec->Level + 1);
			return true;
		}
	}

	// 技能尚未授予，改为输出锁定描述。这里只使用调用方传入的 AbilityInfo：
	// GameMode 是服务器独有对象，客户端上向它取 AbilityInfo 只会得到空指针。
	if (AbilityTag.IsValid() &&
		!AbilityTag.MatchesTagExact(FAuraGameplayTags::Get().Abilities_None) &&
		IsValid(InAbilityInfo))
	{
		OutDescription = UAuraGameplayAbility::GetLockedDescription(
			InAbilityInfo->FindAbilityInfoForTag(AbilityTag).LevelRequirement);
	}

	return false;
}

void UAuraAbilitySystemComponent::UpdateAbilityStatuses(const int32 Level)
{
	UAbilityInfo* AbilityInfo = UAuraAbilitySystemLibrary::GetAbilityInfo(GetAvatarActor());
	if (!IsValid(AbilityInfo)) return;

	for (const FAuraAbilityInfo& Info : AbilityInfo->AbilityInformation)
	{
		if (!Info.AbilityTag.IsValid() || Level < Info.LevelRequirement || !Info.Ability) continue;

		if (GetSpecFromAbilityTag(Info.AbilityTag) == nullptr)
		{
			FGameplayAbilitySpec AbilitySpec(Info.Ability, 1);
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Eligible);
			const FGameplayAbilitySpecHandle AbilitySpecHandle = GiveAbility(AbilitySpec);
			if (FGameplayAbilitySpec* GrantedAbilitySpec = FindAbilitySpecFromHandle(AbilitySpecHandle))
			{
				// 标记实际授予的 Spec，确保动态状态标签参与复制同步。
				MarkAbilitySpecDirty(*GrantedAbilitySpec);
				ClientUpdateAbilityStatus(Info.AbilityTag, FAuraGameplayTags::Get().Abilities_Status_Eligible,1);
			}
		}
	}
}

FGameplayTag UAuraAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	if (AbilitySpec.Ability)
	{
		for (const FGameplayTag& Tag : AbilitySpec.Ability.Get()->GetAssetTags())
		{
			if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities"))))
			{
				return Tag;
			}
		}
	}

	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetStatusFromAbilityTag(const FGameplayTag& AbilityTag)
{
	if (const FGameplayAbilitySpec* Spec=GetSpecFromAbilityTag(AbilityTag))
	{
		return GetStatusFromSpec(*Spec);
	}
	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetInputTagFromAbilityTag(const FGameplayTag& AbilityTag)
{
	if (const FGameplayAbilitySpec* Spec=GetSpecFromAbilityTag(AbilityTag))
	{
		return GetInputTagFromSpec(*Spec);
	}
	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	for (const FGameplayTag& Tag : AbilitySpec.GetDynamicSpecSourceTags())
	{
		if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("InputTag"))))
		{
			return Tag;
		}
	}

	return FGameplayTag();
}

FGameplayTag UAuraAbilitySystemComponent::GetStatusFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	for (const FGameplayTag& Tag : AbilitySpec.GetDynamicSpecSourceTags())
	{
		if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities.Status"))))
		{
			return Tag;
		}
	}

	return FGameplayTag();
}

void UAuraAbilitySystemComponent::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
	if (GetAvatarActor()->Implements<UPlayerInterface>())
	{
		if (IPlayerInterface::Execute_GetAttributePoints(GetAvatarActor()) > 0)
		{
			ServerUpgradeAttribute(AttributeTag);
		}
	}
}

void UAuraAbilitySystemComponent::ServerSpendSpellPoint_Implementation(const FGameplayTag& AbilityTag)
{
	// 客户端 UI 的点数、所选技能和状态都可能过期或被伪造：
	// 扣点前用服务器权威数据重新校验，任一条件不满足时既不扣点也不改状态。
	AActor* const ControlledAvatar = GetAvatarActor();
	if (!IsValid(ControlledAvatar) || !ControlledAvatar->HasAuthority() || !ControlledAvatar->Implements<UPlayerInterface>())
	{
		return;
	}

	// 玩家 ASC 的 OwnerActor 是 PlayerState，以它复制的权威点数为准。
	const AAuraPlayerState* AuraPlayerState = Cast<AAuraPlayerState>(GetOwnerActor());
	if (!IsValid(AuraPlayerState) || AuraPlayerState->GetSpellPoints() <= 0)
	{
		return;
	}

	// Dash 的升级按钮在 SpellMenu 中已固定禁用，服务端同样拒绝，避免被绕过后消耗点数。
	// 槽位约束仍由 ServerEquipAbility 负责，本轮不在这里改动。
	if (AbilityTag.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Movement_Dash))
	{
		return;
	}

	// 只有真实授予且状态为 Eligible/Unlocked/Equipped 的 Spec 才允许升级。
	FScopedAbilityListLock AbilityListLock(*this);
	FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag);
	if (!AbilitySpec || !AbilitySpec->Ability || !AbilityTag.IsValid() ||
		!GetAbilityTagFromSpec(*AbilitySpec).MatchesTagExact(AbilityTag))
	{
		return;
	}

	FGameplayTag Status = GetStatusFromSpec(*AbilitySpec);
	if (!Status.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Eligible) &&
		!Status.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Unlocked) &&
		!Status.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Equipped))
	{
		return;
	}

	IPlayerInterface::Execute_AddToSpellPoints(ControlledAvatar, -1);

	if (Status.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Eligible))
	{
		AbilitySpec->GetDynamicSpecSourceTags().RemoveTag(FAuraGameplayTags::Get().Abilities_Status_Eligible);
		AbilitySpec->GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Unlocked);
		Status = FAuraGameplayTags::Get().Abilities_Status_Unlocked;
	}
	else if (Status.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Equipped) ||
		Status.MatchesTagExact(FAuraGameplayTags::Get().Abilities_Status_Unlocked))
	{
		AbilitySpec->Level += 1;
	}

	ClientUpdateAbilityStatus(AbilityTag, Status, AbilitySpec->Level);
	MarkAbilitySpecDirty(*AbilitySpec);
}

void UAuraAbilitySystemComponent::ServerUpgradeAttribute_Implementation(const FGameplayTag& AttributeTag)
{
	// 属性点只能花在四个主属性上；事件与扣点前都按服务器权威数据校验。
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	const bool bIsPrimaryAttribute = AttributeTag.MatchesTagExact(GameplayTags.Attributes_Primary_Strength) ||
		AttributeTag.MatchesTagExact(GameplayTags.Attributes_Primary_Intelligence) ||
		AttributeTag.MatchesTagExact(GameplayTags.Attributes_Primary_Resilience) ||
		AttributeTag.MatchesTagExact(GameplayTags.Attributes_Primary_Vigor);
	if (!bIsPrimaryAttribute)
	{
		return;
	}

	AActor* const ControlledAvatar = GetAvatarActor();
	if (!IsValid(ControlledAvatar) || !ControlledAvatar->HasAuthority() || !ControlledAvatar->Implements<UPlayerInterface>())
	{
		return;
	}

	const AAuraPlayerState* AuraPlayerState = Cast<AAuraPlayerState>(GetOwnerActor());
	if (!IsValid(AuraPlayerState) || AuraPlayerState->GetAttributePoints() <= 0)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = AttributeTag;
	Payload.EventMagnitude = 1.f;

	// 合法事件与既有成本保持原样：先发事件，再扣一点属性点。
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(ControlledAvatar, AttributeTag, Payload);
	IPlayerInterface::Execute_AddToAttributePoints(ControlledAvatar, -1);
}

void UAuraAbilitySystemComponent::ServerEquipAbility_Implementation(const FGameplayTag& AbilityTag,
	const FGameplayTag& Slot)
{
	// Dash 与空格槽位一一绑定：Dash 不能换到别的槽，别的技能也不能占用空格。
	const FAuraGameplayTags& EquipTags = FAuraGameplayTags::Get();
	if (AbilityTag.MatchesTagExact(EquipTags.Abilities_Movement_Dash) &&
		!Slot.MatchesTagExact(EquipTags.InputTag_Space))
	{
		return;
	}
	if (Slot.MatchesTagExact(EquipTags.InputTag_Space) &&
		!AbilityTag.MatchesTagExact(EquipTags.Abilities_Movement_Dash))
	{
		return;
	}

	if (FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag))
	{
		const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
		const FGameplayTag PrevSlot = GetInputTagFromSpec(*AbilitySpec);
		const FGameplayTag Status = GetStatusFromSpec(*AbilitySpec);

		const bool bStatusValid = Status.MatchesTagExact(GameplayTags.Abilities_Status_Equipped) ||
			Status.MatchesTagExact(GameplayTags.Abilities_Status_Unlocked);
		if (bStatusValid)
		{
			// Remove this InputTag (slot) from any Ability that has it.
			ClearAbilitiesOfSlot(Slot);
			// Clear this ability's slot, just in case, it's a different slot.
			ClearSlot(AbilitySpec);
			// Now, assign this ability to this slot.
			AbilitySpec->GetDynamicSpecSourceTags().AddTag(Slot);
			if (Status.MatchesTagExact(GameplayTags.Abilities_Status_Unlocked))
			{
				AbilitySpec->GetDynamicSpecSourceTags().RemoveTag(GameplayTags.Abilities_Status_Unlocked);
				AbilitySpec->GetDynamicSpecSourceTags().AddTag(GameplayTags.Abilities_Status_Equipped);
			}
			MarkAbilitySpecDirty(*AbilitySpec);
			// 装备成功后 Spec 上的状态一定是 Equipped，回传修改前的旧状态会让 UI 显示成未装备。
			// 同时该通知必须只在装备真正生效时发送，否则状态非法时 UI 会误显示装备成功。
			ClientEquipAbility(AbilityTag, GameplayTags.Abilities_Status_Equipped, Slot, PrevSlot);
		}
	}
}

void UAuraAbilitySystemComponent::ClientEquipAbility_Implementation(const FGameplayTag& AbilityTag, const FGameplayTag& Status,
	const FGameplayTag& Slot, const FGameplayTag& PreviousSlot)
{
	AbilityEquipped.Broadcast(AbilityTag,Status,Slot,PreviousSlot);
}

void UAuraAbilitySystemComponent::ClearSlot(FGameplayAbilitySpec* Spec)
{
	const FGameplayTag Slot = GetInputTagFromSpec(*Spec);
	Spec->GetDynamicSpecSourceTags().RemoveTag(Slot);
	// 槽位变更必须标脏，否则 FastArraySerializer 不会把这条 Spec 重新复制给客户端，
	// 被顶下来的技能在客户端上会残留旧 InputTag。
	MarkAbilitySpecDirty(*Spec);
}

bool UAuraAbilitySystemComponent::AbilityHasSlot(FGameplayAbilitySpec* Spec, const FGameplayTag& Slot)
{
	for (const FGameplayTag& Tag : Spec->GetDynamicSpecSourceTags())
	{
		if (Tag.MatchesTagExact(Slot))
		{
			return true;
		}
	}
	return false;
}
void UAuraAbilitySystemComponent::ClearAbilitiesOfSlot(const FGameplayTag& Slot)
{
	FScopedAbilityListLock ActiveScopeLock(*this);
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (AbilityHasSlot(&Spec, Slot))
		{
			ClearSlot(&Spec);
		}
	}
}

void UAuraAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	if (!bStartUpAbilitiesGiven)
	{
		bStartUpAbilitiesGiven=true;
		AbilitiesGivenDelegate.Broadcast();
	}
}
