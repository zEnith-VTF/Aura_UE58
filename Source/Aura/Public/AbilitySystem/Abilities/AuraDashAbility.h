// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AuraDashAbility.generated.h"

class UAnimMontage;
class USoundBase;

/**
 * 空格闪现：按下后立刻沿鼠标水平方向瞬移，动画和 GameplayCue 只做表现。
 * 不使用 Root Motion，不等待 Montage Notify。
 */
UCLASS()
class AURA_API UAuraDashAbility : public UAuraGameplayAbility
{
	GENERATED_BODY()

public:
	UAuraDashAbility();
	virtual FString GetDescription(int32 Level) override;
	virtual FString GetNextLevelDescription(int32 Level) override;

	UFUNCTION(BlueprintPure, Category = "Dash")
	float GetBlinkDistance() const { return BlinkDistance; }

	UFUNCTION(BlueprintPure, Category = "Dash")
	FGameplayTag GetCooldownTag() const;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, const FVector& DashDirection);

	FVector ResolveBlinkDirection(const FVector& SubmittedDirection, const AActor* Avatar) const;
	bool ComputeSafeBlinkDestination(const ACharacter* Character, const FVector& Direction, FVector& OutDestination) const;
	void PlayCosmeticMontage(ACharacter* Character) const;
	void StopCosmeticMontage() const;
	void ExecuteCueAtLocation(const FGameplayTag& CueTag, const FVector& Location) const;
	void PlayBlinkSoundAtLocation(USoundBase* Sound, const FVector& Location) const;
	void ExecuteTrailCue(const FGameplayTag& CueTag, const FVector& TrailStart, const FVector& TrailEnd) const;
	void RequestEndBlink(bool bWasCancelled);

	UFUNCTION()
	void FinishBlink();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	TObjectPtr<UAnimMontage> StartMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	float BlinkDistance = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	float StaticObstacleClearance = 10.f;

	/** 为真时，闪现前把角色偏航转到水平方向，不改控制器旋转。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	bool bFaceBlinkDirection = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	FGameplayTag StartCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	FGameplayTag TrailCueTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	FGameplayTag EndCueTag;

	/** 成功闪现时在起点播放；留空则不出声。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (DisplayName = "闪现开始"))
	TObjectPtr<USoundBase> BlinkStartSound;

	/** 成功闪现时在落点播放；留空则不出声。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (DisplayName = "闪现结束"))
	TObjectPtr<USoundBase> BlinkEndSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash")
	FGameplayTag CooldownTag;

private:
	bool bBlinkInProgress = false;
	bool bPendingEndWasCancelled = false;
};
