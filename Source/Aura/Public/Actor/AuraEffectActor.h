// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "AuraEffectActor.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

UENUM(BlueprintType) 
enum class EEffectApplicationPolicy:UINT8
{
	ApplyOnOverlap,
	ApplyOnEndOverlap,
	DoNotApply
};


UENUM(BlueprintType) 
enum class EEffectRemovalPolicy:UINT8
{
	RemoveOnOverlap,
	RemoveOnEndOverlap,
	DoNotRemove
};

UCLASS()
class AURA_API AAuraEffectActor : public AActor
{
	GENERATED_BODY()
	
public:	

	AAuraEffectActor();
	
protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintCallable)
	void ApplyEffectToTarget(AActor* Actor,TSubclassOf<UGameplayEffect>GameplayEffectClass);
	
	UFUNCTION(BlueprintCallable)
	void OnBeginOverlap(AActor* TargetActor);
	
	UFUNCTION(BlueprintCallable)
	void OnEndOverlap(AActor* TargetActor);
	
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	bool bDestroyOnEffectApplication=false;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	bool bApplyEffectToEnemies=false;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	TSubclassOf<UGameplayEffect>InstantGameplayEffectClass;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	EEffectApplicationPolicy InstantEffectApplicationPolicy=EEffectApplicationPolicy::DoNotApply;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	TSubclassOf<UGameplayEffect>DurationGameplayEffectClass;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	EEffectApplicationPolicy DurationEffectApplicationPolicy=EEffectApplicationPolicy::DoNotApply;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	TSubclassOf<UGameplayEffect>InfiniteGameplayEffectClass;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	EEffectApplicationPolicy InfiniteEffectApplicationPolicy=EEffectApplicationPolicy::DoNotApply;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	EEffectRemovalPolicy InfiniteEffectRemovalPolicy=EEffectRemovalPolicy::DoNotRemove;
	
	//下面数组只定义，未实现，有机会可以实现
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	TArray<TSubclassOf<UGameplayEffect>>InfiniteGameplayEffectClassArray;
	
	TMap<FActiveGameplayEffectHandle,UAbilitySystemComponent*> ActiveEffectHandles;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Applied Effect")
	float ActorLevel=1.f;

private:
	bool BeginTargetOverlap(AActor* TargetActor);
	bool EndTargetOverlap(AActor* TargetActor);

	TMap<TWeakObjectPtr<AActor>, int32> ActiveOverlapCounts;
};
