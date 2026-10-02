#pragma once

#include "CoreMinimal.h"
#include "AuraAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "AuraFireNadoField.generated.h"

class AAuraFireNadoArea;

/** Server-only cast state. It applies one periodic hit per target across the union of live areas. */
UCLASS(NotBlueprintable)
class AURA_API AAuraFireNadoField : public AActor
{
	GENERATED_BODY()

public:
	AAuraFireNadoField();

	void InitializeField(const FDamageEffectParams& InDamageParams, float InDamagePeriod);
	void AddArea(AAuraFireNadoArea* Area);
	void NotifyTornadoFinished();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void DamagePulse();

	UPROPERTY()
	FDamageEffectParams DamageParams;

	TArray<TWeakObjectPtr<AAuraFireNadoArea>> Areas;
	FTimerHandle DamageTimer;
	float DamagePeriod = 1.f;
	bool bTornadoActive = true;
};
