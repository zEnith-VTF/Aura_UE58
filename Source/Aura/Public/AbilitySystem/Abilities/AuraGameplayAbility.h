// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AuraGameplayAbility.generated.h"

class AAuraCharacter;
class UAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class AURA_API UAuraGameplayAbility : public UGameplayAbility 
{
	GENERATED_BODY()
public:
	virtual FString GetDescription(int32 Level);
	virtual FString GetNextLevelDescription(int32 Level);
	static FString GetLockedDescription(int32 Level);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) override;

	/** Call only after this cast has started its gameplay effect; damage and hits are not casts. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Voice")
	void ReportSuccessfulCastForVoice();

	/** Zero disables voice counting. Each ability Blueprint configures its own threshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice", meta = (ClampMin = "0", UIMin = "0"))
	int32 VoiceEverySuccessfulCasts = 0;

	UPROPERTY(EditAnywhere,Category="Input")
	FGameplayTag StartUpInputTag;
	
protected:
	void RecordVoiceCastCommitResult(bool bSucceeded);

	float GetManaCost(float InLevel = 1.f) const;
	float GetCooldown(float InLevel = 1.f) const;

private:
	TWeakObjectPtr<AAuraCharacter> VoiceCastCharacter;
	TWeakObjectPtr<UAbilitySystemComponent> VoiceCastASC;
	FGameplayTag VoiceCastAbilityTag;
	uint64 VoiceCastGeneration = 0;
	bool bVoiceCastCommitted = false;
	bool bVoiceCastReported = false;
};
