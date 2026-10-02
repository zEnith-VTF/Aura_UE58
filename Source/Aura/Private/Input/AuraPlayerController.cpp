// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/AuraPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "Components/AuraInteractionComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Character.h"
#include "Input/AuraInputComponent.h"
#include "Interfaction/AuraInteractionInterface.h"
#include "Interfaction/EnemyInterface.h"
#include "Interfaction/HighlightInterface.h"
#include "Tags/AuraGameplayTags.h"
#include "UI/Widgets/DamageTextComponent.h"

AAuraPlayerController::AAuraPlayerController()
{
	bReplicates = true;
	
	Spline=CreateDefaultSubobject<USplineComponent>("Spline Component");

	// 交互组件由控制器自己创建，蓝图侧不需要再添加。
	InteractionComponent=CreateDefaultSubobject<UAuraInteractionComponent>("Interaction Component");
	
}

void AAuraPlayerController::AutoRun()
{
	if (!bAutoRunning)return;
	if (IsDashActive())return;
	if (HasActiveDirectionalInput())
	{
		CancelMouseMovement();
		return;
	}
	if (APawn*ControlleredPawn =GetPawn<APawn>())
	{
		//获取距离Spline最近的点，已经方向，防止偏离
		const FVector LocationOnSpline = Spline->FindLocationClosestToWorldLocation(ControlleredPawn->GetActorLocation(),ESplineCoordinateSpace::World);
		const FVector Direction=Spline->FindDirectionClosestToWorldLocation(LocationOnSpline,ESplineCoordinateSpace::World);
		ControlleredPawn->AddMovementInput(Direction);
		
		const float DistanceToDirection =(LocationOnSpline-CachedLocation).Length();
		if (DistanceToDirection<=AutoRunningAcceptAnceRadius)
		{
			bAutoRunning=false;

			// 到达接收半径就通知交互组件，避免它在自动移动停下后才判断出仍在范围外。
			if (InteractionComponent)
			{
				InteractionComponent->TickApproach(ControlleredPawn);
			}
		}
	}
}

void AAuraPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CursorTrace();
	AutoRun();
	// 靠近过程中的常规推进；到达帧 AutoRun 已通知一次，状态判断会去重。
	if (InteractionComponent)
	{
		InteractionComponent->TickApproach(GetPawn<APawn>());
	}
}

void AAuraPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter,bool IsBlocked,bool IsCriticalHit)
{
	if (IsValid(TargetCharacter)&&DamageTextComponentClass &&IsLocalController())
	{
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		DamageText->SetDamageText(DamageAmount,IsBlocked,IsCriticalHit);
	}
}

void AAuraPlayerController::ServerRequestInteract_Implementation(AActor* Target, int32 RequestId)
{
	// 服务器不信任客户端“已经在范围内”，用服务器上的 Pawn 位置重新判定。
	APawn* const ControlledPawn = GetPawn<APawn>();
	if (!IsValid(Target) || !IsValid(ControlledPawn) || !Target->Implements<UAuraInteractionInterface>())
	{
		ClientInteractionResult(RequestId, false);
		return;
	}

	AActor* HostActor = Target;
	FAuraInteractionInfo Info;
	IAuraInteractionInterface::Execute_QueryInteraction(HostActor, ControlledPawn, Info);

	// 允许比交互半径多 50cm 的容差，吸收客户端与服务器位置的细微差异。
	constexpr float ServerRangeTolerance = 50.f;
	const float DistanceToApproach = FVector::Dist(ControlledPawn->GetActorLocation(), Info.ApproachLocation);
	if (Info.bCanInteract && DistanceToApproach <= Info.InteractionRange + ServerRangeTolerance)
	{
		// 实现方可能因自身条件拒绝；结果以 TryInteract 的返回值为准。
		const bool bAccepted = IAuraInteractionInterface::Execute_TryInteract(HostActor, ControlledPawn);
		ClientInteractionResult(RequestId, bAccepted);
		return;
	}

	ClientInteractionResult(RequestId, false);
}

void AAuraPlayerController::ClientInteractionResult_Implementation(int32 RequestId, bool bAccepted)
{
	// 结果是否属于当前请求由组件判断；旧 id 在这里已经被组件忽略。
	if (InteractionComponent)
	{
		InteractionComponent->HandleServerResult(RequestId, bAccepted);
	}
}

bool AAuraPlayerController::StartAutoRunTo(const FVector& Destination)
{
	if (IsDashActive())
	{
		// 冲刺中不生成新的点击移动路径；解锁后下一次左键按下仍走原来的点击流程。
		return false;
	}
	if (HasActiveDirectionalInput())
	{
		return false;
	}

	APawn* const ControlledPawn = GetPawn<APawn>();
	if (!IsValid(ControlledPawn) || !Spline)
	{
		return false;
	}

	UNavigationPath* const NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(
		this, ControlledPawn->GetActorLocation(), Destination);
	if (!NavPath || NavPath->PathPoints.Num() == 0)
	{
		return false;
	}

	Spline->ClearSplinePoints();
	for (const auto& PointLoc : NavPath->PathPoints)
	{
		Spline->AddSplinePoint(PointLoc, ESplineCoordinateSpace::World);
	}
	CachedLocation = NavPath->PathPoints.Last();
	bAutoRunning = true;
	return true;
}

void AAuraPlayerController::StopAutoRun()
{
	bAutoRunning = false;
}

bool AAuraPlayerController::HasActiveDirectionalInput() const
{
	if (!MoveAction)
	{
		return false;
	}

	const UEnhancedPlayerInput* const EnhancedInput = Cast<UEnhancedPlayerInput>(PlayerInput);
	if (!EnhancedInput)
	{
		return false;
	}

	const FVector2D Input = EnhancedInput->GetActionValue(MoveAction).Get<FVector2D>();
	return FMath::IsFinite(Input.X) && FMath::IsFinite(Input.Y) && !Input.IsNearlyZero(KINDA_SMALL_NUMBER);
}

void AAuraPlayerController::CancelMouseMovement()
{
	StopAutoRun();
	FollowTime = 0.f;
	bMouseMoveSuppressed = true;
	if (InteractionComponent)
	{
		InteractionComponent->NotifyManualMove();
	}
}

void AAuraPlayerController::MoveInputEnded(const FInputActionValue& InputActionValue)
{
	// 只结束本次 WASD。抑制保持到下一次左键按下，旧点击不会在松键时恢复。
	(void)InputActionValue;
	bWasdMoveEngaged = false;
}

bool AAuraPlayerController::IsDashActive()
{
	UAuraAbilitySystemComponent* const AuraASC = GetAuraAbilitySystemComponent();
	if (!IsValid(AuraASC)) return false;

	// The active Spec covers target-data waiting, teleport, failure, and next-tick cleanup.
	const FGameplayAbilitySpec* const DashSpec = AuraASC->GetSpecFromAbilityTag(FAuraGameplayTags::Get().Abilities_Movement_Dash);
	return DashSpec && DashSpec->IsActive();
}

void AAuraPlayerController::OnUnPossess()
{
	if (InteractionComponent)
	{
		InteractionComponent->CancelRequest();
	}
	bWasdMoveEngaged = false;
	bMouseMoveSuppressed = false;
	FollowTime = 0.f;
	StopAutoRun();

	Super::OnUnPossess();
}

void AAuraPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InteractionComponent)
	{
		InteractionComponent->CancelRequest();
	}
	bWasdMoveEngaged = false;
	bMouseMoveSuppressed = false;
	FollowTime = 0.f;
	StopAutoRun();

	Super::EndPlay(EndPlayReason);
}

void AAuraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	//检查控制器是否有效，若不有效，立刻停止。
	check(AuraContext);

	//添加mapping给增强输入
	Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	//只有控制本地角色时候Subsystem才是非空指针，所以不使用断言
	if (Subsystem==nullptr)return;
	Subsystem->AddMappingContext(AuraContext, 0);
	
	//设置鼠标显示，鼠标样式为默认
	bShowMouseCursor = true;
	DefaultMouseCursor=EMouseCursor::Default;
	
	//创建游戏UI输入变量，设置鼠标不被锁定到窗口，防止鼠标点击屏幕锁定，设置输入模式
	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}

void AAuraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	UAuraInputComponent* AuraInputComponent =Cast<UAuraInputComponent>(InputComponent);
	
	check(AuraInputComponent);
	
	AuraInputComponent->BindAction(MoveAction,ETriggerEvent::Triggered,this,&AAuraPlayerController::MoveActionFunction);
	AuraInputComponent->BindAction(MoveAction,ETriggerEvent::Completed,this,&AAuraPlayerController::MoveInputEnded);
	AuraInputComponent->BindAction(MoveAction,ETriggerEvent::Canceled,this,&AAuraPlayerController::MoveInputEnded);
	AuraInputComponent->BindAction(ShiftAction,ETriggerEvent::Started,this,&AAuraPlayerController::ShiftPressed);
	AuraInputComponent->BindAction(ShiftAction,ETriggerEvent::Completed,this,&AAuraPlayerController::ShiftReleased);
	AuraInputComponent->BindAbilityActions(InputConfig,this,&AAuraPlayerController::AbilityInputTagPressed,&AAuraPlayerController::AbilityInputTagReleased,&AAuraPlayerController::AbilityInputTagHeld);

}

void AAuraPlayerController::MoveActionFunction(const FInputActionValue& InputActionValue)
{
	const FVector2D Input = InputActionValue.Get<FVector2D>();
	const bool bValidInput = FMath::IsFinite(Input.X) && FMath::IsFinite(Input.Y)
		&& !Input.IsNearlyZero(KINDA_SMALL_NUMBER);
	if (bValidInput)
	{
		const bool bTakeover = !bWasdMoveEngaged;
		CancelMouseMovement();
		if (IsDashActive())
		{
			// 冲刺中不接受 WASD 位移。这次按键仍然废弃旧的鼠标移动请求。
			bWasdMoveEngaged = true;
			return;
		}

		if (APawn* ControlerPawn = GetPawn<APawn>())
		{
			if (bTakeover && !ControlerPawn->IsMoveInputIgnored())
			{
				ControlerPawn->ConsumeMovementInputVector();
			}
			bWasdMoveEngaged = true;

			const FRotator Rotation = GetControlRotation();
			const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
			ControlerPawn->AddMovementInput(ForwardDirection, Input.Y);
			ControlerPawn->AddMovementInput(RightDirection, Input.X);
		}
		return;
	}

	if (IsDashActive())
	{
		// 冲刺中不接受 WASD：位移只由技能自己的 Root Motion 推进。
		return;
	}

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f,Rotation.Yaw,0.f);
	//先将旋转度数转换为旋转矩阵，再将矩阵中的X轴，Y轴提取出，X代表前方,Y代表右方
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	
	if (APawn* ControlerPawn=GetPawn<APawn>())
	{
		//第一个变量代表方向，第二个变量代表强度
		ControlerPawn->AddMovementInput(ForwardDirection,Input.Y);
		ControlerPawn->AddMovementInput(RightDirection,Input.X);
	}
}

void AAuraPlayerController::CursorTrace()
{
	//允许可见性通道被高亮
	GetHitResultUnderCursor(ECC_Visibility,false,CursorHit);

	LastActor=ThisActor;
	ThisActor=nullptr;

	AActor* const HitActor = CursorHit.bBlockingHit ? CursorHit.GetActor() : nullptr;
	if (IsValid(HitActor) && HitActor->Implements<UHighlightInterface>())
	{
		ThisActor=HitActor;
	}

	// 光标离开任何可高亮对象时，先熄灭上一个轮廓。
	if (ThisActor==nullptr)
	{
		UnHighlightActor(LastActor);
		return;
	}

	if (LastActor!=ThisActor)
	{
		UnHighlightActor(LastActor);
		HighlightActor(ThisActor);
	}
}

void AAuraPlayerController::HighlightActor(AActor* InActor)
{
	if (IsValid(InActor) && InActor->Implements<UHighlightInterface>())
	{
		IHighlightInterface::Execute_HighlightActor(InActor);
	}
}

void AAuraPlayerController::UnHighlightActor(AActor* InActor)
{
	if (IsValid(InActor) && InActor->Implements<UHighlightInterface>())
	{
		IHighlightInterface::Execute_UnHighlightActor(InActor);
	}
}

void AAuraPlayerController::AbilityInputTagPressed(FGameplayTag Tag)
{
	// 空格只按一次算一次冲刺：按下时直接激活，Held 不再重复触发。
	if (Tag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_Space))
	{
		if (GetAuraAbilitySystemComponent())
		{
			GetAuraAbilitySystemComponent()->AbilityInputTagHeld(Tag);
		}
		return;
	}

	if (Tag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		// 只有敌人才能进入锁定攻击；检查点可高亮但不锁定。
		if (IsValid(ThisActor))
		{
			TargetingStatus = ThisActor->Implements<UEnemyInterface>() ? ETargetingStatus::TargetingEnemy : ETargetingStatus::TargetingNonEnemy;
		}
		else
		{
			TargetingStatus = ETargetingStatus::NotTargeting;
		}
		bAutoRunning=false;
		FollowTime = 0.f;
		bMouseMoveSuppressed = HasActiveDirectionalInput();
		// 新的左键按下取消上一次未完成的交互请求，敌人与 Shift 分支不受影响。
		if (InteractionComponent)
		{
			InteractionComponent->CancelRequest();
		}
	}

}

void AAuraPlayerController::AbilityInputTagReleased(FGameplayTag Tag)
{
	//判断是否左键
	if (!Tag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetAuraAbilitySystemComponent())
		{
			GetAuraAbilitySystemComponent()->AbilityInputTagReleased(Tag);
		}
		return;
	}
	//先告知已经松开
	if (GetAuraAbilitySystemComponent())
	{
		GetAuraAbilitySystemComponent()->AbilityInputTagReleased(Tag);
	}
	//如果没有敌人且没有按下shift，执行
	if (TargetingStatus != ETargetingStatus::TargetingEnemy && !bShiftKeyDown)
	{
		const APawn* ControllerPawn=GetPawn<APawn>();
		//如果没有Pawn，设置无敌人，设置FollowTime为0
		if (!ControllerPawn)
		{
			FollowTime=0.f;
			TargetingStatus=ETargetingStatus::NotTargeting;
			return;
		}
		// 本次左键已被 WASD 接管，或方向键仍按着：不寻路、不交互、不放点击特效。
		const bool bRejectMouseMove = bMouseMoveSuppressed || HasActiveDirectionalInput();
		//否则判断按下时间，如果小于短按时间，开启自动寻路
		if (!bRejectMouseMove && FollowTime<=ShortPressThreshold)
		{
			
			// 先尝试交互分支：返回 true 表示本次点击已被交互消费，不再执行普通移动。
			if (InteractionComponent && InteractionComponent->TryBeginClick(ThisActor))
			{
				FollowTime=0.f;
				TargetingStatus=ETargetingStatus::NotTargeting;
				return;
			}

			// 命中对象可自定义落点，例如检查点会把目的地改到自身站位。
			if (IsValid(ThisActor) && ThisActor->Implements<UHighlightInterface>())
			{
				IHighlightInterface::Execute_SetMoveToLocation(ThisActor,CachedLocation);
			}
			
			// 点击位置特效
			if (ClickNiagaraSystem)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					this,
					ClickNiagaraSystem,
					CachedLocation
				);
			}
			StartAutoRunTo(CachedLocation);
		}
	}

	// 左键释放后统一清理本次输入状态，避免锁定攻击或 Shift 施法状态残留到下一次输入。
	FollowTime=0.f;
	TargetingStatus=ETargetingStatus::NotTargeting;
}

void AAuraPlayerController::AbilityInputTagHeld(FGameplayTag Tag)
{
	// 空格冲刺在 Pressed 激活一次即可；按住期间不再往 ASC 送输入。
	if (Tag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_Space))
	{
		return;
	}
	//如果不是左键，那么告诉该Tag被按下
	if (!Tag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetAuraAbilitySystemComponent())
		{
			GetAuraAbilitySystemComponent()->AbilityInputTagHeld(Tag);
		}
		return;
	}
	//如果是左键且存在敌人或者shift被按下，就广播该Tag
	if (TargetingStatus == ETargetingStatus::TargetingEnemy || bShiftKeyDown)
	{
		if (GetAuraAbilitySystemComponent())
		{
			GetAuraAbilitySystemComponent()->AbilityInputTagHeld(Tag);
		}
	}
	//否则，记录左键按下的时间
	else
	{
		if (IsDashActive())
		{
			// 冲刺中不接受左键按住移动，也不刷新落点缓存；松开后的路径生成同样被 StartAutoRunTo 挡住。
			return;
		}
		if (bMouseMoveSuppressed || HasActiveDirectionalInput())
		{
			return;
		}

		// 玩家自己按住移动时取消正在靠近的请求；敌人与 Shift 分支不进入这里。
		if (InteractionComponent)
		{
			InteractionComponent->NotifyManualMove();
		}

		FollowTime+=GetWorld()->GetDeltaSeconds();
		//找到点击的地点
		if (CursorHit.bBlockingHit)
		{
			//设置导航地点
			CachedLocation=CursorHit.ImpactPoint;
		}
		
		if (APawn *ControllerPawn=GetPawn())
		{
			// 使用 GetSafeNormal() 的三个关键原因：
			// 获取纯方向：去除距离信息，只保留方向
			// 恒定移动速度：无论目标多远，移动速度都一致
			// 防止崩溃：安全处理角色已到达目标点的边界情况
			const FVector WorldLocation = (CachedLocation-ControllerPawn->GetActorLocation()).GetSafeNormal();
			//// 朝这个方向移动一帧
			ControllerPawn->AddMovementInput(WorldLocation);
		}
	}
}

UAuraAbilitySystemComponent* AAuraPlayerController::GetAuraAbilitySystemComponent()
{
	if (AuraAbilitySystemComponent==nullptr)
	{
		UAbilitySystemComponent *ASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>());
		AuraAbilitySystemComponent=Cast<UAuraAbilitySystemComponent>(ASC);
		return AuraAbilitySystemComponent;
	}
	return AuraAbilitySystemComponent;
}
