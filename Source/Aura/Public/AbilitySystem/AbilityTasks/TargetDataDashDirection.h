// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "TargetDataDashDirection.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDashDirectionSignature, const FGameplayAbilityTargetDataHandle&, Data, const FVector&, DashDirection);

/**
 * 空格冲刺的方向采样任务，沿用 UTargetDataUnderMouse 的 GAS TargetData 协议。
 * 本地控制端用 ECC_Visibility 做一次光标检测，把采样原点与水平方向打包进
 * FGameplayAbilityTargetData_LocationInfo 发给服务器；服务器只做校验并自行归一化。
 */
UCLASS()
class AURA_API UTargetDataDashDirection : public UAbilityTask
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable,Category="Ability|Tasks",meta=(DisplayName="TargetDataDashDirection",HidePin="OwningAbility",DefaultToSelf="OwningAbility",BlueprintInternalUseOnly="true"))
	static UTargetDataDashDirection* CreateTargetDataDashDirection(UGameplayAbility* OwningAbility);

	/** Data 原样透传，DashDirection 在服务器端已经是校验后的水平单位向量。 */
	UPROPERTY(BlueprintAssignable)
	FDashDirectionSignature ValidData;

private:
	virtual void Activate() override;

	void SendDashDirectionData();
	void CancelTargetDataAndAbility(const FGameplayAbilityActorInfo* ActorInfo);
	void CancelOwningAbility();

	void OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle,FGameplayTag ActivationTag);
	void OnTargetDataCancelledCallback();
};