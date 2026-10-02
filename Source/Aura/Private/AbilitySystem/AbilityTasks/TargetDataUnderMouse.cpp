// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"


UTargetDataUnderMouse* UTargetDataUnderMouse::CreateTargetDataUnderMouse(UGameplayAbility* OwningAbility, bool bInCancelOwningAbilityOnInvalidData)
{
	UTargetDataUnderMouse* Myobj = NewAbilityTask<UTargetDataUnderMouse>(OwningAbility);
	if (Myobj)
	{
		Myobj->bCancelOwningAbilityOnInvalidData = bInCancelOwningAbilityOnInvalidData;
	}
	return Myobj;
}

void UTargetDataUnderMouse::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (!ActorInfo || !AbilitySystemComponent.IsValid())
	{
		CancelOwningAbility();
		return;
	}
	
	const bool bIsLocallyControlled = ActorInfo->IsLocallyControlled();
	if (bIsLocallyControlled)
	{
		SendMouseCursorData();
	}
	else
	{
		//获取当前 Ability 的唯一标识符
		const FGameplayAbilitySpecHandle SpecHandle=GetAbilitySpecHandle();
		const FPredictionKey ActivationPredictionKey=GetActivationPredictionKey();
		//当服务器接收到客户端发送的 TargetData 时，调用我的 OnTargetDataReplicatedCallback 函数
		AbilitySystemComponent.Get()->AbilityTargetDataSetDelegate(SpecHandle,ActivationPredictionKey).AddUObject(this,&UTargetDataUnderMouse::OnTargetDataReplicatedCallback);
		// 客户端可能把鼠标移到地图外。服务器也必须监听 TargetData 取消，否则服务器 Ability 会一直等待。
		AbilitySystemComponent.Get()->AbilityTargetDataCancelledDelegate(SpecHandle,ActivationPredictionKey).AddUObject(this,&UTargetDataUnderMouse::OnTargetDataCancelledCallback);
		//检查是否已经有数据
		const bool bCalledDelegate=AbilitySystemComponent.Get()->CallReplicatedTargetDataDelegatesIfSet(SpecHandle,ActivationPredictionKey);
		if (!bCalledDelegate)
		{
			//如果没有，设置等待状态
			SetWaitingOnRemotePlayerData();
		}
	}
	
}

void UTargetDataUnderMouse::SendMouseCursorData()
{
	if (!AbilitySystemComponent.IsValid() || !Ability)
	{
		CancelOwningAbility();
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		CancelOwningAbility();
		return;
	}
	
	//创建一个客户端预测窗口
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get());
	
	APlayerController* PlayerController=ActorInfo->PlayerController.Get();
	if (!PlayerController)
	{
		CancelTargetDataAndAbility(ActorInfo);
		return;
	}

	FHitResult CursorResult;
	const bool bHit=PlayerController->GetHitResultUnderCursor(ECC_Visibility,false,CursorResult);
	if (!bHit || !CursorResult.bBlockingHit)
	{
		if (!bCancelOwningAbilityOnInvalidData)
		{
			// 按住类技能允许鼠标移出地图：不能取消 Ability，但必须给服务器一个明确的收尾，
			// 否则服务器任务会一直停在 SetWaitingOnRemotePlayerData。
			FGameplayAbilityTargetDataHandle EmptyHandle;
			if (!ActorInfo->IsNetAuthority())
			{
				AbilitySystemComponent->ServerSetReplicatedTargetData(GetAbilitySpecHandle(),
					GetActivationPredictionKey(), EmptyHandle,
					FGameplayTag(), AbilitySystemComponent->ScopedPredictionKey);
			}
			if (ShouldBroadcastAbilityTaskDelegates())
			{
				ValidData.Broadcast(EmptyHandle);
			}
			EndTask();
			return;
		}

		// 地图外没有有效 Visibility 命中：同步取消 TargetData，并结束本次 Ability。
		CancelTargetDataAndAbility(ActorInfo);
		return;
	}
	//创建一个“单目标命中数据盒子”
	FGameplayAbilityTargetData_SingleTargetHit* Data=new FGameplayAbilityTargetData_SingleTargetHit();
	Data->HitResult=CursorResult;
	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(Data);

	if (!ActorInfo->IsNetAuthority())
	{
		AbilitySystemComponent->ServerSetReplicatedTargetData(GetAbilitySpecHandle(),
			GetActivationPredictionKey(),DataHandle,
			FGameplayTag(),AbilitySystemComponent->ScopedPredictionKey);
	}
	
	if(ShouldBroadcastAbilityTaskDelegates())
	{
		//客户端先应用效果，保证低延迟
		ValidData.Broadcast(DataHandle);
	}
	EndTask();
}

void UTargetDataUnderMouse::CancelTargetDataAndAbility(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (ActorInfo && AbilitySystemComponent.IsValid() && !ActorInfo->IsNetAuthority())
	{
		// 远程服务器可能正在 SetWaitingOnRemotePlayerData；显式发送取消可以唤醒服务器任务。
		AbilitySystemComponent->ServerSetReplicatedTargetDataCancelled(
			GetAbilitySpecHandle(),
			GetActivationPredictionKey(),
			AbilitySystemComponent->ScopedPredictionKey);
	}

	CancelOwningAbility();
}

void UTargetDataUnderMouse::CancelOwningAbility()
{
	// EndTask 与 EndAbility 是两个不同的生命周期。先结束任务，再取消拥有它的 Ability，
	// 防止无效鼠标目标让 AbilitySpec 永久保持 Active。
	UGameplayAbility* const OwningAbility = Ability;
	EndTask();

	if (IsValid(OwningAbility))
	{
		OwningAbility->K2_CancelAbility();
	}
}

void UTargetDataUnderMouse::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle &DataHandle,
	FGameplayTag ActivationTag)
{
	//先在本地保存一份 TargetData
	const FGameplayAbilityTargetDataHandle LocalDataHandle = DataHandle;
	//从 ASC 缓存中消费并移除原数据
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(),GetActivationPredictionKey());
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		//服务器应用效果
		ValidData.Broadcast(LocalDataHandle);
	}
	EndTask();
}

void UTargetDataUnderMouse::OnTargetDataCancelledCallback()
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(),GetActivationPredictionKey());
	}

	CancelOwningAbility();
}
