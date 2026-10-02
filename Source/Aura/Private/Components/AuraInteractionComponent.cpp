// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/AuraInteractionComponent.h"

#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Input/AuraPlayerController.h"

namespace
{
	// 沿 ChildActorComponent 父链向上解析交互宿主的深度上限，避免异常层级导致无限循环。
	constexpr int32 MaxInteractionHostDepth = 8;

	// 靠近过程中连续没有更接近站位的累计时间上限，超过就取消这次靠近。
	constexpr float ApproachStallTimeout = 1.5f;

	// 判定“有更接近”的距离容差，避免浮点抖动反复清零计时。
	constexpr float ApproachProgressTolerance = 1.f;
}

UAuraInteractionComponent::UAuraInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UAuraInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerController = Cast<AAuraPlayerController>(GetOwner());
}

void UAuraInteractionComponent::CancelRequest()
{
	// 让 RequestId 前进，已经发出的服务器请求即使返回也不再被接受。
	++RequestId;
	State = EAuraInteractionState::Idle;
	Target = nullptr;
	ApproachStallTime = 0.f;
	LastDistanceToApproachLocation = -1.f;

	// 取消靠近请求时一并停掉本次自动移动，避免角色继续走向已经作废的站位。
	StopControllerAutoRun();
}

bool UAuraInteractionComponent::TryBeginClick(AActor* HitActor)
{
	APawn* const Pawn = OwnerController ? OwnerController->GetPawn() : nullptr;
	AActor* HostActor = nullptr;
	if (!IsValid(Pawn) || !ResolveInteractionHost(HitActor, HostActor))
	{
		// 命中对象没有交互宿主：保持原状态，交回控制器走旧的点击移动分支。
		return false;
	}

	FAuraInteractionInfo Info;
	if (!QueryInteractionInfo(HostActor, Pawn, Info))
	{
		return false;
	}

	if (!Info.bCanInteract)
	{
		// 实现方当前不接受交互：不开始请求，本次点击仍走普通移动。
		return false;
	}

	const float DistanceToApproach = FVector::Dist(Pawn->GetActorLocation(), Info.ApproachLocation);
	if (DistanceToApproach <= Info.InteractionRange)
	{
		// 已经在交互范围内：不自动靠近，直接提交服务器请求；TryInteract 只在服务器调用。
		StartRequest(HostActor, Info);
		State = EAuraInteractionState::AwaitingResult;
		if (!SubmitRequest(Pawn))
		{
			CancelRequest();
		}
		return true;
	}

	if (!Info.bAllowApproach)
	{
		// 超范围且不允许自动靠近：本次点击已被交互分支消费，不执行普通移动，也不发请求。
		CancelRequest();
		return true;
	}

	StartRequest(HostActor, Info);
	if (!OwnerController || !OwnerController->StartAutoRunTo(Info.ApproachLocation))
	{
		// 寻路失败或路径为空：消费本次点击，不再回到普通移动分支。
		CancelRequest();
		return true;
	}

	State = EAuraInteractionState::Approaching;
	LastDistanceToApproachLocation = DistanceToApproach;
	return true;
}

void UAuraInteractionComponent::NotifyManualMove()
{
	// 玩家自己开始移动时不再替他跑完靠近；等待服务器结果期间不打断。
	if (State == EAuraInteractionState::Approaching)
	{
		CancelRequest();
	}
}

void UAuraInteractionComponent::TickApproach(APawn* Pawn)
{
	if (State != EAuraInteractionState::Approaching)
	{
		return;
	}

	APawn* ControlledPawn = IsValid(Pawn) ? Pawn : nullptr;
	if (!ControlledPawn && OwnerController)
	{
		ControlledPawn = OwnerController->GetPawn();
	}
	if (!IsValid(ControlledPawn) || !Target.IsValid())
	{
		CancelRequest();
		return;
	}

	const float DistanceToApproach = FVector::Dist(ControlledPawn->GetActorLocation(), ApproachLocation);
	if (DistanceToApproach <= InteractionRange)
	{
		// 已进入交互范围：停掉自动移动并提交服务器请求。
		StopControllerAutoRun();
		State = EAuraInteractionState::AwaitingResult;
		if (!SubmitRequest(ControlledPawn))
		{
			CancelRequest();
		}
		return;
	}

	if (!OwnerController || !OwnerController->IsAutoRunning())
	{
		// 自动移动已经停下但仍在范围外：只走完部分路径，取消，不提交交互。
		CancelRequest();
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	if (LastDistanceToApproachLocation >= 0.f &&
		DistanceToApproach < LastDistanceToApproachLocation - ApproachProgressTolerance)
	{
		ApproachStallTime = 0.f;
	}
	else
	{
		ApproachStallTime += DeltaSeconds;
	}
	LastDistanceToApproachLocation = DistanceToApproach;

	if (ApproachStallTime > ApproachStallTimeout)
	{
		CancelRequest();
	}
}

void UAuraInteractionComponent::HandleServerResult(int32 InRequestId, bool bAccepted)
{
	// 只有服务器请求的结果本身能让请求结束；接受与否都由实现方在 TryInteract 里处理。
	if (State != EAuraInteractionState::AwaitingResult || InRequestId != RequestId)
	{
		// 旧请求或已经取消的请求：结果直接丢弃。
		return;
	}

	CancelRequest();
}

bool UAuraInteractionComponent::ResolveInteractionHost(AActor* HitActor, AActor*& OutHostActor)
{
	OutHostActor = nullptr;
	if (!IsValid(HitActor))
	{
		return false;
	}

	// 命中对象自身是交互宿主时直接用；否则只沿 ChildActorComponent 父链向上找。
	AActor* CurrentActor = HitActor;
	for (int32 Depth = 0; IsValid(CurrentActor) && Depth < MaxInteractionHostDepth; ++Depth)
	{
		if (CurrentActor->Implements<UAuraInteractionInterface>())
		{
			OutHostActor = CurrentActor;
			return true;
		}

		const UChildActorComponent* ChildActorComponent = nullptr;
		if (const USceneComponent* const RootComponent = CurrentActor->GetRootComponent())
		{
			// 子 Actor 的根组件挂在它的 UChildActorComponent 上，据此回到宿主对象。
			ChildActorComponent = Cast<UChildActorComponent>(RootComponent->GetAttachParent());
		}
		CurrentActor = ChildActorComponent ? ChildActorComponent->GetOwner() : nullptr;
	}

	return false;
}

bool UAuraInteractionComponent::QueryInteractionInfo(AActor* HostActor, APawn* Interactor, FAuraInteractionInfo& OutInfo) const
{
	OutInfo = FAuraInteractionInfo();
	if (!IsValid(HostActor) || !IsValid(Interactor) || !HostActor->Implements<UAuraInteractionInterface>())
	{
		return false;
	}

	// 查询无副作用；bCanInteract 以实现方写入结构体的值为准。
	IAuraInteractionInterface::Execute_QueryInteraction(HostActor, Interactor, OutInfo);
	return true;
}

void UAuraInteractionComponent::StartRequest(AActor* HostActor, const FAuraInteractionInfo& Info)
{
	++RequestId;
	Target = HostActor;
	ApproachLocation = Info.ApproachLocation;
	InteractionRange = Info.InteractionRange;
	ApproachStallTime = 0.f;
	LastDistanceToApproachLocation = -1.f;
}

bool UAuraInteractionComponent::SubmitRequest(APawn* Pawn)
{
	if (!OwnerController || !IsValid(Pawn) || !OwnerController->IsLocalController() || !Pawn->IsLocallyControlled())
	{
		// 交互只由本地玩家控制器发起，服务器上的其他连接不接受这条输入路径。
		return false;
	}

	OwnerController->ServerRequestInteract(Target.Get(), RequestId);
	return true;
}

void UAuraInteractionComponent::StopControllerAutoRun() const
{
	if (OwnerController)
	{
		OwnerController->StopAutoRun();
	}
}
