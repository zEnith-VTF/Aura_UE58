// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/AuraCharacterBase.h"
#include "Interfaction/EnemyInterface.h"
#include "Interfaction/HighlightInterface.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "AuraEnemy.generated.h"


class UBehaviorTree;
class AAuraAIController;
class UWidgetComponent;
struct FGameplayTag;
/**
 * 
 */
UCLASS()
class AURA_API AAuraEnemy : public AAuraCharacterBase,public IEnemyInterface,public IHighlightInterface
{
	GENERATED_BODY()
public:
	AAuraEnemy();
	virtual void PossessedBy(AController* NewController) override;
	virtual void HighlightActor_Implementation() override;
	virtual void UnHighlightActor_Implementation() override;
	virtual void SetMoveToLocation_Implementation(FVector& OutDestination) override;
	virtual void SetCombatTarget_Implementation(AActor* InCombatTarget) override;
	virtual AActor* GetCombatTarget_Implementation() const override;
	
	virtual int32 GetPlayerLevel_Implementation() override;
	virtual void Die() override;

	// 随从登记：只在服务器执行；返回 true 表示该 Actor 已由本 Enemy 登记（含此前已登记的重复调用），
	// 返回 false 表示未登记、不计数，蓝图可据此清理本次生成的随从。
	UFUNCTION(BlueprintCallable, Category = "Combat|Minion")
	bool RegisterSummonedMinion(AActor* SummonedMinion);
	
	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnHealthChanged;
	
	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnMaxHealthChanged;
protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo()override;
	virtual void InitializeDefaultAttributes()const override;
	void HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// 随从 OnDestroyed（销毁，而不是死亡）回调：按身份解除登记并释放一次计数。
	UFUNCTION()
	void HandleSummonedMinionDestroyed(AActor* DestroyedActor);

	// 生命周期结束：只解除本 Enemy 的登记与本类回调，不销毁随从、不取消别人的能力。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	TObjectPtr<UWidgetComponent> HealthBar;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat")
	float BaseWalkSpeed = 250.f;

	UPROPERTY(BlueprintReadOnly,Category="Combat")
	bool bHitReacting = false;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat")
	float LifeSpan = 5.f;

	// 已登记的召唤随从：弱引用，不阻止 GC；按 OnDestroyed 而不是 OnDeath 释放计数。
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SummonedMinions;

	UPROPERTY(BlueprintReadWrite, Category="Combat")
	TObjectPtr<AActor> CombatTarget;
	
	UPROPERTY(EditAnywhere,Category="AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;
	
	UPROPERTY()
	TObjectPtr<AAuraAIController>AuraAIController;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Character Class Defaults",meta=(AllowPrivateAccess="true"))
	int32 Level=1;
	
};
