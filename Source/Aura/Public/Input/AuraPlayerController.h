// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerController.h"
#include "AuraPlayerController.generated.h"

class UAuraInteractionComponent;
class USplineComponent;
class UDamageTextComponent;
class UAuraAbilitySystemComponent;
class UAuraInputConfig;
class UEnhancedInputLocalPlayerSubsystem;
class ACharacter;
struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;

enum class ETargetingStatus : uint8
{
	TargetingEnemy,
	TargetingNonEnemy,
	NotTargeting
};

/**
 * 
 */
UCLASS()
class AURA_API AAuraPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AAuraPlayerController();
	void AutoRun();
	virtual void Tick(float DeltaTime) override;

	//显示伤害数字 
	UFUNCTION(Client, Reliable)
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter,bool IsBlocked,bool IsCriticalHit);

	//======== 点击交互 ========
	// 服务器请求走玩家连接，不在门 Actor 上加 Server RPC。
	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* Target, int32 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientInteractionResult(int32 RequestId, bool bAccepted);

	// 用现有 Spline 寻路走到 Destination 并开始自动移动；交互靠近与旧点击移动共用。
	// 寻路失败或路径为空时返回 false，不改动当前自动移动状态。
	bool StartAutoRunTo(const FVector& Destination);

	// 只停止自动移动，不取消交互请求。靠近中的请求由调用方 NotifyManualMove 处理。
	void StopAutoRun();

	bool IsAutoRunning() const { return bAutoRunning; }

	// ASC 的 Dash Spec 活动期间阻断普通移动。
	bool IsDashActive();
	// 冲刺开始：本次按键时长不再计入点击移动的短按判定。
	void NotifyDashStarted() { FollowTime = 0.f; }

	FORCEINLINE UAuraInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION()
	void MoveActionFunction(const FInputActionValue& InputActionValue);

private:
	//======== 输入处理 ========
	FORCEINLINE void ShiftPressed(){bShiftKeyDown=true;}
	FORCEINLINE void ShiftReleased(){bShiftKeyDown=false;}
	void AbilityInputTagPressed(FGameplayTag Tag);
	void AbilityInputTagReleased(FGameplayTag Tag);
	void AbilityInputTagHeld(FGameplayTag Tag);

	// 当前 IA_Move 是否为有效二维方向。不查询物理按键。
	bool HasActiveDirectionalInput() const;
	// 停掉鼠标路径和本次按住移动，并取消正在靠近的交互。不结束技能，不撤销已提交的交互。
	void CancelMouseMovement();
	UFUNCTION()
	void MoveInputEnded(const FInputActionValue& InputActionValue);

	//======== 光标追踪 ========
	void CursorTrace();

	// 轮廓开关：仅在实现 UHighlightInterface 时调用接口事件。
	static void HighlightActor(AActor* InActor);
	static void UnHighlightActor(AActor* InActor);

	//======== 内部访问器 ========
	UAuraAbilitySystemComponent*GetAuraAbilitySystemComponent();

	//======== 输入资产与状态 ========
	UPROPERTY(EditAnywhere,Category="Input")
	TObjectPtr<UInputMappingContext>AuraContext;
	
	UPROPERTY(EditAnywhere,Category="Input")
	TObjectPtr<UInputAction>MoveAction;
	
	UPROPERTY(EditAnywhere,Category="Input")
	TObjectPtr<UInputAction>ShiftAction;
	
	bool bShiftKeyDown=false;
	
	UPROPERTY()
	UEnhancedInputLocalPlayerSubsystem* Subsystem;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UNiagaraSystem> ClickNiagaraSystem;

	//======== 光标命中缓存 ========
	// 记录实现 UHighlightInterface 的 Actor，锁定判定另行检查 UEnemyInterface。
	// 高亮只看直接命中的对象，不因为解析出交互宿主而改高亮目标。
	TObjectPtr<AActor> LastActor;

	TObjectPtr<AActor> ThisActor;
	FHitResult CursorHit;
	
	UPROPERTY(EditDefaultsOnly,Category="Input")
	TObjectPtr<UAuraInputConfig> InputConfig;
	
	TObjectPtr<UAuraAbilitySystemComponent> AuraAbilitySystemComponent;
	
	//======== 点击移动与自动寻路状态 ========
	FVector CachedLocation=FVector::ZeroVector;
	float FollowTime = 0.f;
	float ShortPressThreshold = 0.5f;
	bool bAutoRunning = false;
	// 有效 WASD 已按下。松键或取消时清除，不因此恢复鼠标移动。
	bool bWasdMoveEngaged = false;
	// 旧左键移动被 WASD 接管后保持抑制，直到下一次左键按下才按当时的方向输入重新判断。
	bool bMouseMoveSuppressed = false;
	ETargetingStatus TargetingStatus = ETargetingStatus::NotTargeting;
	
	UPROPERTY(EditDefaultsOnly)
	float AutoRunningAcceptAnceRadius = 50.f;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USplineComponent> Spline;

	//======== 点击交互 ========
	// 控制器自己创建，不要求用户在蓝图里添加；状态只存在于本地玩家，不复制。
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAuraInteractionComponent> InteractionComponent;

	//======== 伤害飘字 ========
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;
};
