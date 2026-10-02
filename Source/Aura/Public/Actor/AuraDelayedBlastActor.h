// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AuraAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "AuraDelayedBlastActor.generated.h"

class UDecalComponent;
class UMaterialInterface;
class FLifetimeProperty;

UCLASS()
class AURA_API AAuraDelayedBlastActor : public AActor
{
	GENERATED_BODY()

public:
	AAuraDelayedBlastActor();

	/** Called by the authoritative ability while this actor is still deferred. */
	void InitializeBlast(const FVector& InBlastCenter, const FDamageEffectParams& InDamageEffectParams);

	float GetBlastRadius() const { return BlastRadius; }
	float GetWarningDelay() const { return WarningDelay; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	/** Blueprint subclasses can add the warning decal material, Niagara, UI, or audio here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Delayed Blast|Presentation")
	void OnWarningStarted();

	/** Called on the server and every replicated client when the blast resolves. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Delayed Blast|Presentation")
	void OnBlastExploded();

private:
	void Explode();
	void ApplyDamageToOverlaps();
	void PlayExplosionPresentation();
	void UpdateWarningDecal();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayExplosionPresentation();

	UFUNCTION()
	void OnRep_BlastCenter();

	UFUNCTION()
	void OnRep_BlastRadius();

	UFUNCTION()
	void OnRep_HasExploded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delayed Blast|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDecalComponent> WarningDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delayed Blast|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> WarningDecalMaterial = nullptr;

	/** Replicated so the client presentation uses the server's accepted point, not a moving pawn. */
	UPROPERTY(ReplicatedUsing = OnRep_BlastCenter, BlueprintReadOnly, Category = "Delayed Blast", meta = (AllowPrivateAccess = "true"))
	FVector_NetQuantize10 BlastCenter = FVector::ZeroVector;

	/** Gameplay values are fixed for this ability: 300 horizontal units and 1.5 seconds. */
	UPROPERTY(ReplicatedUsing = OnRep_BlastRadius, BlueprintReadOnly, Category = "Delayed Blast", meta = (AllowPrivateAccess = "true"))
	float BlastRadius = 300.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Delayed Blast", meta = (AllowPrivateAccess = "true"))
	float WarningDelay = 1.5f;

	UPROPERTY(ReplicatedUsing = OnRep_HasExploded)
	bool bHasExploded = false;

	/** Server-only damage data; it is intentionally not replicated to visual-only clients. */
	UPROPERTY(BlueprintReadWrite, Category = "Delayed Blast", meta = (AllowPrivateAccess = "true"))
	FDamageEffectParams DamageEffectParams;

	FTimerHandle ExplosionTimerHandle;
	bool bExplosionPresentationPlayed = false;
};
