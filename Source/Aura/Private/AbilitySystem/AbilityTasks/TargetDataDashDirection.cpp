// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/TargetDataDashDirection.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 采样点到角色位置的近零判定：小于该距离视为没有方向。
	constexpr double MinDashDirectionLength = 1.0;

	// 服务器容忍的采样原点与自身角色之间的距离，吸收客户端与服务器的位置和延迟差异。
	constexpr float MaxDashOriginDrift = 300.f;
}

UTargetDataDashDirection* UTargetDataDashDirection::CreateTargetDataDashDirection(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<UTargetDataDashDirection>(OwningAbility);
}

void UTargetDataDashDirection::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (!ActorInfo || !AbilitySystemComponent.IsValid())
	{
		CancelOwningAbility();
		return;
	}

	if (ActorInfo->IsLocallyControlled())
	{
		SendDashDirectionData();
		return;
	}

	const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
	const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();
	AbilitySystemComponent.Get()->AbilityTargetDataSetDelegate(SpecHandle,ActivationPredictionKey).AddUObject(this,&UTargetDataDashDirection::OnTargetDataReplicatedCallback);
	// 没有无效数据时的显式取消，服务器任务会一直停在 SetWaitingOnRemotePlayerData。
	AbilitySystemComponent.Get()->AbilityTargetDataCancelledDelegate(SpecHandle,ActivationPredictionKey).AddUObject(this,&UTargetDataDashDirection::OnTargetDataCancelledCallback);

	if (!AbilitySystemComponent.Get()->CallReplicatedTargetDataDelegatesIfSet(SpecHandle,ActivationPredictionKey))
	{
		SetWaitingOnRemotePlayerData();
	}
}

void UTargetDataDashDirection::SendDashDirectionData()
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

	AActor* const AvatarActor = Ability->GetAvatarActorFromActorInfo();
	APlayerController* const PlayerController = ActorInfo->PlayerController.Get();
	if (!IsValid(AvatarActor) || !PlayerController)
	{
		CancelTargetDataAndAbility(ActorInfo);
		return;
	}

	// 与控制器 CursorTrace 相同的检测通道和接口，只在按下瞬间采样一次。
	FHitResult CursorResult;
	const bool bHit = PlayerController->GetHitResultUnderCursor(ECC_Visibility,false,CursorResult);
	const FVector OriginLocation = AvatarActor->GetActorLocation();
	FVector HorizontalDirection = FVector::ZeroVector;
	if (bHit && CursorResult.bBlockingHit)
	{
		const FVector HitLocation = CursorResult.ImpactPoint;
		const bool bFiniteHit = FMath::IsFinite(HitLocation.X) && FMath::IsFinite(HitLocation.Y) && FMath::IsFinite(HitLocation.Z);
		if (bFiniteHit)
		{
			const FVector RawDirection = HitLocation - OriginLocation;
			HorizontalDirection = FVector(RawDirection.X, RawDirection.Y, 0.0);
		}
	}

	// 鼠标没有打到可走表面时，沿角色当前朝向前移，不再把这次按键取消掉。
	if (HorizontalDirection.SizeSquared() <= FMath::Square(MinDashDirectionLength))
	{
		const FVector Forward = AvatarActor->GetActorForwardVector();
		HorizontalDirection = FVector(Forward.X, Forward.Y, 0.0);
	}
	if (HorizontalDirection.SizeSquared() <= FMath::Square(MinDashDirectionLength))
	{
		CancelTargetDataAndAbility(ActorInfo);
		return;
	}

	// 原点和方向都放进位置信息：Origin 保留原始角色位置供服务器做距离校验，方向由服务器重新归一化。
	FGameplayAbilityTargetData_LocationInfo* Data = new FGameplayAbilityTargetData_LocationInfo();
	Data->SourceLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Data->SourceLocation.LiteralTransform = FTransform(OriginLocation);
	Data->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	Data->TargetLocation.LiteralTransform = FTransform(HorizontalDirection.GetSafeNormal());

	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(Data);

	if (!ActorInfo->IsNetAuthority())
	{
		// 预测窗口必须在广播和 EndAbility 之前关掉，否则连续闪现会把预测键留脏，后面的火球术无法激活。
		FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get());
		AbilitySystemComponent->ServerSetReplicatedTargetData(GetAbilitySpecHandle(),
			GetActivationPredictionKey(),DataHandle,
			FGameplayTag(),AbilitySystemComponent->ScopedPredictionKey);
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		// 本地控制端先走一遍：客户端只做表现，权威位移仍由服务器发起。
		ValidData.Broadcast(DataHandle,HorizontalDirection.GetSafeNormal());
	}
	EndTask();
}

void UTargetDataDashDirection::CancelTargetDataAndAbility(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (ActorInfo && AbilitySystemComponent.IsValid() && !ActorInfo->IsNetAuthority())
	{
		// 显式取消让服务器任务立刻收尾，而不是等待另一份永远不会到达的数据。
		AbilitySystemComponent->ServerSetReplicatedTargetDataCancelled(
			GetAbilitySpecHandle(),
			GetActivationPredictionKey(),
			AbilitySystemComponent->ScopedPredictionKey);
	}

	CancelOwningAbility();
}

void UTargetDataDashDirection::CancelOwningAbility()
{
	// EndTask 与取消 Ability 是两条生命周期，先结束任务再取消技能，避免 Spec 永久 Active。
	UGameplayAbility* const OwningAbility = Ability;
	EndTask();

	if (IsValid(OwningAbility))
	{
		OwningAbility->K2_CancelAbility();
	}
}

void UTargetDataDashDirection::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle,
	FGameplayTag ActivationTag)
{
	const FGameplayAbilityTargetDataHandle LocalDataHandle = DataHandle;
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(),GetActivationPredictionKey());

	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		EndTask();
		return;
	}

	FVector DashDirection = FVector::ZeroVector;
	if (const AActor* const AvatarActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr)
	{
		if (LocalDataHandle.Num() == 1)
		{
			const FGameplayAbilityTargetData* const RawData = LocalDataHandle.Get(0);
			const bool bIsLocationData = RawData &&
				RawData->GetScriptStruct() == FGameplayAbilityTargetData_LocationInfo::StaticStruct();
			const FGameplayAbilityTargetData_LocationInfo* const LocationData =
				bIsLocationData ? static_cast<const FGameplayAbilityTargetData_LocationInfo*>(RawData) : nullptr;
			if (LocationData && LocationData->TargetLocation.LocationType == EGameplayAbilityTargetingLocationType::LiteralTransform)
			{
				const FVector SubmittedDirection = LocationData->TargetLocation.LiteralTransform.GetTranslation();
				const FVector OriginLocation = LocationData->SourceLocation.LiteralTransform.GetTranslation();
				const bool bFiniteDirection = FMath::IsFinite(SubmittedDirection.X) &&
					FMath::IsFinite(SubmittedDirection.Y) && FMath::IsFinite(SubmittedDirection.Z);
				const bool bFiniteOrigin = FMath::IsFinite(OriginLocation.X) &&
					FMath::IsFinite(OriginLocation.Y) && FMath::IsFinite(OriginLocation.Z);
				// 服务器只接受距离自己角色不远的采样原点，并在本地重新归一化水平方向。
				if (bFiniteDirection && bFiniteOrigin &&
					FVector::Dist2D(OriginLocation,AvatarActor->GetActorLocation()) <= MaxDashOriginDrift)
				{
					const FVector HorizontalDirection(SubmittedDirection.X,SubmittedDirection.Y,0.0);
					if (HorizontalDirection.SizeSquared() > FMath::Square(MinDashDirectionLength))
					{
						DashDirection = HorizontalDirection.GetSafeNormal();
					}
				}
				if (DashDirection.IsNearlyZero())
				{
					const FVector Forward = AvatarActor->GetActorForwardVector();
					DashDirection = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
				}
			}
		}
	}

	ValidData.Broadcast(LocalDataHandle,DashDirection);
	EndTask();
}

void UTargetDataDashDirection::OnTargetDataCancelledCallback()
{
	if (AbilitySystemComponent.IsValid())
	{
		AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(),GetActivationPredictionKey());
	}

	CancelOwningAbility();
}