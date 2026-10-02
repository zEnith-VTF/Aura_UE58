#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"
#include "AuraFireNadoAbility.generated.h"

class AAuraFireNadoActor;
class AAuraFireNadoArea;
class UAnimMontage;

UCLASS()
class AURA_API UAuraFireNadoAbility : public UAuraDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UAuraFireNadoAbility();

	virtual FString GetDescription(int32 Level) override;
	virtual FString GetNextLevelDescription(int32 Level) override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Cast")
	TObjectPtr<UAnimMontage> CastMontage;

	/** Must match the GameplayEvent notify on CastMontage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Cast")
	FGameplayTag CastEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Actors")
	TSubclassOf<AAuraFireNadoActor> TornadoClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Actors")
	TSubclassOf<AAuraFireNadoArea> AreaClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Level")
	FScalableFloat MaxTravelDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Level")
	FScalableFloat AreaLifetime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Level")
	FScalableFloat AreaDamage;

	/** Inherited Damage supplies the TornadoDamage CurveTable row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Movement", meta = (ClampMin = "1"))
	float TravelSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Movement", meta = (ClampMin = "1"))
	float AreaSpacing = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Area", meta = (ClampMin = "1"))
	float AreaRadius = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Area", meta = (ClampMin = "0.01"))
	float AreaDamagePeriod = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Area", meta = (ClampMin = "1"))
	int32 MaxAreasPerCast = 64;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Spawn")
	float SpawnForwardOffset = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Spawn")
	float SpawnHeightOffset = 50.f;

private:
	UFUNCTION()
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION()
	void OnCastNotify(FGameplayEventData Payload);

	UFUNCTION()
	void OnCastCompleted();

	UFUNCTION()
	void OnCastInterrupted();

	void StartCastFlow();
	void EndCastFlow(bool bWasCancelled);
	bool TryGetAimDirection(const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutDirection) const;
	bool SpawnTornado();
	FString BuildDescription(int32 Level, bool bNextLevel) const;

	FVector LockedDirection = FVector::ZeroVector;
	bool bTargetResolved = false;
	bool bSpawned = false;
};
