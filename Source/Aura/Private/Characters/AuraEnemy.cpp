// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/AuraEnemy.h"

#include "AI/AuraAIController.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "Aura/Aura.h"
#include "Aura/AuraLogChannels.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaction/CombatInterface.h"
#include "Tags/AuraGameplayTags.h"
#include "UI/Widgets/AuraUserWidget.h"


AAuraEnemy::AAuraEnemy()
{
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	AbilitySystemComponent=CreateDefaultSubobject<UAuraAbilitySystemComponent>("AuraAbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	
	AttributeSet=CreateDefaultSubobject<UAuraAttributeSet>("AuraAttributeSet");
	
	HealthBar=CreateDefaultSubobject<UWidgetComponent>("HealthBar");
	HealthBar->SetupAttachment(RootComponent);
	
	// 轮廓模板值在构造期写入，高亮时只切换 RenderCustomDepth。
	GetMesh()->SetCustomDepthStencilValue(CUSTOM_DEPTH_RED);
	GetMesh()->MarkRenderStateDirty();
	Weapon->SetCustomDepthStencilValue(CUSTOM_DEPTH_RED);
	Weapon->MarkRenderStateDirty();
}

void AAuraEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (!HasAuthority()) return;

	AuraAIController = Cast<AAuraAIController>(NewController);
	if (!AuraAIController || !BehaviorTree || !BehaviorTree->BlackboardAsset) return;

	UBlackboardComponent* BlackboardComponent = AuraAIController->GetBlackboardComponent();
	if (!BlackboardComponent) return;

	BlackboardComponent->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
	AuraAIController->RunBehaviorTree(BehaviorTree);

	BlackboardComponent->SetValueAsBool(FName("HitReacting"), false);
	BlackboardComponent->SetValueAsBool(FName("RangedAttacker"), CharacterClass != ECharacterClass::Warrior);
}

void AAuraEnemy::HighlightActor_Implementation()
{
	GetMesh()->SetRenderCustomDepth(true);
	Weapon->SetRenderCustomDepth(true);
}

void AAuraEnemy::UnHighlightActor_Implementation()
{
	GetMesh()->SetRenderCustomDepth(false);
	Weapon->SetRenderCustomDepth(false);
}

void AAuraEnemy::SetMoveToLocation_Implementation(FVector& OutDestination)
{
	// 敌人不改变点击落点。
}

void AAuraEnemy::SetCombatTarget_Implementation(AActor* InCombatTarget)
{
	const bool bIsDeadCombatTarget = IsValid(InCombatTarget) &&
		InCombatTarget->Implements<UCombatInterface>() &&
		ICombatInterface::Execute_IsDead(InCombatTarget);

	CombatTarget = IsValid(InCombatTarget) && InCombatTarget != this && !bIsDeadCombatTarget
		? InCombatTarget
		: nullptr;
}

AActor* AAuraEnemy::GetCombatTarget_Implementation() const
{
	AActor* const TargetActor = CombatTarget.Get();
	if (!IsValid(TargetActor)) return nullptr;
	if (TargetActor->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(TargetActor)) return nullptr;

	return TargetActor;
}

int32 AAuraEnemy::GetPlayerLevel_Implementation()
{
	return Level;
}

void AAuraEnemy::Die()
{
	if (bDead) return;

	CombatTarget = nullptr;
	SetLifeSpan(LifeSpan);
	if (AuraAIController)
	{
		if (UBlackboardComponent* BlackboardComponent = AuraAIController->GetBlackboardComponent())
		{
			BlackboardComponent->SetValueAsBool(FName("Dead"), true);
		}
	}
	
	Super::Die();
}

void AAuraEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 只解除本 Enemy 的登记与本类回调，不销毁随从，也不取消别人的能力。
	for (const TWeakObjectPtr<AActor>& Minion : SummonedMinions)
	{
		if (AActor* const MinionActor = Minion.Get())
		{
			MinionActor->OnDestroyed.RemoveDynamic(this, &AAuraEnemy::HandleSummonedMinionDestroyed);
		}
	}

	// 生命周期结束时按剩余登记释放计数，使计数随本 Actor 结束归零；随从本身保持原样。
	const int32 ReleasedMinionCount = SummonedMinions.Num();
	SummonedMinions.Empty();

	if (ReleasedMinionCount > 0)
	{
		ICombatInterface::Execute_IncrementMinionCount(this, -ReleasedMinionCount);
	}

	Super::EndPlay(EndPlayReason);
}

bool AAuraEnemy::RegisterSummonedMinion(AActor* SummonedMinion)
{
	// 计数与登记只在服务器维护；其他端返回 false，蓝图可据此清理本地生成物。
	if (!HasAuthority()) return false;

	// 空引用、自己，以及正在销毁的召唤者或随从都不登记。
	if (!IsValid(SummonedMinion) || SummonedMinion == this) return false;
	if (IsActorBeingDestroyed() || SummonedMinion->IsActorBeingDestroyed()) return false;

	const TWeakObjectPtr<AActor> MinionKey(SummonedMinion);
	if (SummonedMinions.Contains(MinionKey))
	{
		// 重复登记：去重，不重复计数、不重复绑定；该对象仍由本 Enemy 登记。
		return true;
	}

	SummonedMinions.Add(MinionKey);

	// 首次登记才绑定：按 OnDestroyed（实际销毁）而不是 OnDeath 释放计数。
	SummonedMinion->OnDestroyed.AddUniqueDynamic(this, &AAuraEnemy::HandleSummonedMinionDestroyed);

	ICombatInterface::Execute_IncrementMinionCount(this, 1);

	UE_LOG(LogAura, Log, TEXT("%s registered summoned minion %s"),
		*GetNameSafe(this),
		*GetNameSafe(SummonedMinion));

	return true;
}

void AAuraEnemy::HandleSummonedMinionDestroyed(AActor* DestroyedActor)
{
	if (!DestroyedActor) return;

	// 用弱引用身份（对象索引 + 序列号）匹配登记条目：广播时 DestroyedActor 可能已处于销毁/PendingKill
	// 状态，Get() 与 IsValid() 都已为假，只有身份比较才能正常移除并释放这一条计数。
	const TWeakObjectPtr<AActor> DestroyedKey(DestroyedActor);
	const int32 RegisteredIndex = SummonedMinions.IndexOfByPredicate(
		[&DestroyedKey](const TWeakObjectPtr<AActor>& RegisteredMinion)
		{
			return RegisteredMinion.HasSameIndexAndSerialNumber(DestroyedKey);
		});

	if (RegisteredIndex == INDEX_NONE)
	{
		// 不是本 Enemy 登记的对象（从未登记，或已被其他 writer 移除）：不猜测计数，也不静默吞掉。
		UE_LOG(LogAura, Warning, TEXT("%s received OnDestroyed for unregistered actor %s"),
			*GetNameSafe(this),
			*GetNameSafe(DestroyedActor));
		return;
	}

	SummonedMinions.RemoveAtSwap(RegisteredIndex);

	// 确实移除了一条登记才 -1 一次，避免负数漂移。
	ICombatInterface::Execute_IncrementMinionCount(this, -1);
}

void AAuraEnemy::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed=BaseWalkSpeed;
	InitAbilityActorInfo();
	
	if (HasAuthority())
	{
		UAuraAbilitySystemLibrary::GiveStartupAbilities(this, AbilitySystemComponent, CharacterClass);
	}
	
	if (UAuraUserWidget* AuraUserWigdet=Cast<UAuraUserWidget>(HealthBar->GetUserWidgetObject()))
	{
		AuraUserWigdet->SetWidgetController(this);
	}
	
	UAuraAttributeSet* AuraAS=Cast<UAuraAttributeSet>(AttributeSet);
	if (AuraAS)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AuraAS->GetHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			}
		);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AuraAS->GetMaxHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxHealthChanged.Broadcast(Data.NewValue);
			}
		);
	//注册标签事件（当新添加或删除）
		AbilitySystemComponent->RegisterGameplayTagEvent(FAuraGameplayTags::Get().Effects_HitReact, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&AAuraEnemy::HitReactTagChanged
		);

		OnMaxHealthChanged.Broadcast(AuraAS->GetMaxHealth());
		OnHealthChanged.Broadcast(AuraAS->GetHealth());
	}
	
}

void AAuraEnemy::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this,this);
	UAuraAbilitySystemComponent*AuraAbilitySystemComponent=Cast<UAuraAbilitySystemComponent>(GetAbilitySystemComponent());
	AuraAbilitySystemComponent->AbilityActorInfoSet();
	
	if (HasAuthority())
	{
		InitializeDefaultAttributes();
	}
}

void AAuraEnemy::InitializeDefaultAttributes() const
{
	UAuraAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, AbilitySystemComponent);
}

void AAuraEnemy::HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bHitReacting = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 0.f : BaseWalkSpeed;
	if (AuraAIController&& AuraAIController->GetBlackboardComponent())
	{
		AuraAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), bHitReacting);
	}

}
