// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "TargetDataUnderMouse.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMouseTargetDataSignature,const FGameplayAbilityTargetDataHandle&,Data);

/*
 *
 */
UCLASS()
class AURA_API UTargetDataUnderMouse : public UAbilityTask
{
	GENERATED_BODY()
public:
	//第一步，创造实例
	UFUNCTION(BlueprintCallable,Category="Ability|Tasks",meta=(DisplayName="TargetDataUnderMouse",HidePin="OwningAbility",DefaultToSelf="OwningAbility",BlueprintInternalUseOnly="true"))
	static UTargetDataUnderMouse* CreateTargetDataUnderMouse(UGameplayAbility *OwningAbility, bool bInCancelOwningAbilityOnInvalidData = true);
	//新增引脚	
	UPROPERTY(BlueprintAssignable)
	FMouseTargetDataSignature ValidData;

	/**
	 * Default true keeps the original behavior (invalid cursor data cancels the owning ability).
	 * Held abilities set it to false: the task then resends an empty handle so the owning ability survives and
	 * the server task is still released instead of waiting on remote player data forever.
	 */
	bool bCancelOwningAbilityOnInvalidData = true;
private:
	virtual void Activate()override;	
	
	void SendMouseCursorData();
	void CancelTargetDataAndAbility(const FGameplayAbilityActorInfo* ActorInfo);
	void CancelOwningAbility();
	
	void OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle &DataHandle,FGameplayTag ActivationTag);
	void OnTargetDataCancelledCallback();
};
