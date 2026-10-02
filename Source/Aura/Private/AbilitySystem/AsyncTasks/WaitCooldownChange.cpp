// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AsyncTasks/WaitCooldownChange.h"

#include "AbilitySystemComponent.h"

UWaitCooldownChange* UWaitCooldownChange::WaitForCooldownChange(
	UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTag& InCooldownTag)
{
	if (!IsValid(AbilitySystemComponent) || !InCooldownTag.IsValid())
	{
		return nullptr;
	}

	UWaitCooldownChange* const WaitCooldownChange = NewObject<UWaitCooldownChange>();
	WaitCooldownChange->ASC = AbilitySystemComponent;
	WaitCooldownChange->CooldownTag = InCooldownTag;
	//防止异步对象被 GC
	WaitCooldownChange->RegisterWithGameInstance(AbilitySystemComponent);

	// 冷却标签从 ASC 上移除时，说明冷却已经结束。
	AbilitySystemComponent->RegisterGameplayTagEvent(
		InCooldownTag,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			WaitCooldownChange,
			&UWaitCooldownChange::CooldownTagChanged);

	// 冷却 GameplayEffect 应用时，查询并广播剩余时间。
	AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		WaitCooldownChange,
		&UWaitCooldownChange::OnActiveEffectAdded);

	return WaitCooldownChange;
}

void UWaitCooldownChange::Activate()
{
	Super::Activate();
	// Async-node delegates are bound before Activate, so a rebuilt Widget receives the current cooldown.
	BroadcastCurrentCooldown();
}

void UWaitCooldownChange::EndTask()
{
	if (bTaskEnded) return;
	bTaskEnded = true;
	if (IsValid(ASC))
	{
		ASC->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
	}

	ASC = nullptr;
	SetReadyToDestroy();
	MarkAsGarbage();
}
//冷却标签数量变成 0
void UWaitCooldownChange::CooldownTagChanged(const FGameplayTag InCooldownTag, int32 NewCount)
{
	if (!bTaskEnded && NewCount == 0)
	{
		CooldownEnd.Broadcast(0.f);
	}
}
//冷却 GE 被添加
void UWaitCooldownChange::OnActiveEffectAdded(
	UAbilitySystemComponent* TargetASC,
	const FGameplayEffectSpec& SpecApplied,
	FActiveGameplayEffectHandle ActiveEffectHandle)
{
	if (bTaskEnded || !IsValid(ASC)) return;

	FGameplayTagContainer AssetTags;
	SpecApplied.GetAllAssetTags(AssetTags);

	FGameplayTagContainer GrantedTags;
	SpecApplied.GetAllGrantedTags(GrantedTags);

	if (!AssetTags.HasTagExact(CooldownTag) && !GrantedTags.HasTagExact(CooldownTag))
	{
		return;
	}
	BroadcastCurrentCooldown();
}

void UWaitCooldownChange::BroadcastCurrentCooldown()
{
	if (bTaskEnded || !IsValid(ASC)) return;

	//创建 GameplayEffect 查询条件
	const FGameplayEffectQuery CooldownQuery =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTag.GetSingleTagContainer());
	//查询所有匹配效果的剩余时间
	const TArray<float> TimesRemaining = ASC->GetActiveEffectsTimeRemaining(CooldownQuery);
	if (TimesRemaining.IsEmpty())
	{
		CooldownEnd.Broadcast(0.f);
		return;
	}

	float TimeRemaining = TimesRemaining[0];
	for (const float CandidateTime : TimesRemaining)
	{
		TimeRemaining = FMath::Max(TimeRemaining, CandidateTime);
	}

	CooldownStart.Broadcast(TimeRemaining);
}

