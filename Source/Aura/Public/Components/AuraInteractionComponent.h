// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaction/AuraInteractionInterface.h"
#include "AuraInteractionComponent.generated.h"

class AAuraPlayerController;
class APawn;

/** 一次点击交互请求的状态。不包含开门中、对话中之类的中间态。 */
UENUM(BlueprintType)
enum class EAuraInteractionState : uint8
{
	Idle,
	Approaching,
	AwaitingResult
};

/**
 * 本地玩家控制器的点击交互请求，由 AAuraPlayerController 构造函数创建，不复制状态。
 * 负责解析交互宿主、读取站位与半径、驱动自动靠近，并在服务器结果返回后回到 Idle。
 * 服务器请求本身发在控制器上，组件不发送 RPC。
 */
UCLASS(ClassGroup=(Aura), meta=(BlueprintSpawnableComponent))
class AURA_API UAuraInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraInteractionComponent();

	virtual void BeginPlay() override;

	/** 取消当前请求并让 RequestId 前进，之后到达的旧结果会被忽略。不回滚服务器已经接受的交互。 */
	void CancelRequest();

	/** 左键短按调用：从命中 Actor 解析交互宿主并查询。返回 true 表示本次点击已被交互分支消费。 */
	bool TryBeginClick(AActor* HitActor);

	/** 玩家开始手动移动时调用：只取消正在靠近的请求，等待服务器结果时不打断。 */
	void NotifyManualMove();

	/** 由控制器 Tick 与自动移动结束处调用；只在 Approaching 状态推进。 */
	void TickApproach(APawn* Pawn);

	/** 接收服务器结果：只有 RequestId 仍是当前请求时才生效并回到 Idle，旧 id 直接丢弃。 */
	void HandleServerResult(int32 InRequestId, bool bAccepted);

	EAuraInteractionState GetState() const { return State; }

private:
	/** 解析交互宿主：命中 Actor 自身，或只沿 ChildActorComponent 父链向上，深度上限 8。 */
	static bool ResolveInteractionHost(AActor* HitActor, AActor*& OutHostActor);
	/** 无副作用查询；查询返回 false 或 bCanInteract 为 false 都表示本次不发起交互。 */
	bool QueryInteractionInfo(AActor* HostActor, APawn* Interactor, FAuraInteractionInfo& OutInfo) const;
	void StartRequest(AActor* HostActor, const FAuraInteractionInfo& Info);
	bool SubmitRequest(APawn* Pawn);
	/** 停掉本次靠近使用的自动移动；普通移动不受影响。 */
	void StopControllerAutoRun() const;

	UPROPERTY()
	TObjectPtr<AAuraPlayerController> OwnerController = nullptr;

	// 请求数据只在本机使用，不复制。
	EAuraInteractionState State = EAuraInteractionState::Idle;
	int32 RequestId = 0;
	TWeakObjectPtr<AActor> Target;
	FVector ApproachLocation = FVector::ZeroVector;
	float InteractionRange = 150.f;
	float ApproachStallTime = 0.f;
	float LastDistanceToApproachLocation = -1.f;
};
