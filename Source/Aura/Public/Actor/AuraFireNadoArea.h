#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuraFireNadoArea.generated.h"

class USphereComponent;

/** Replicated presentation and lifetime for one patch of fire. Damage belongs to the cast field. */
UCLASS()
class AURA_API AAuraFireNadoArea : public AActor
{
	GENERATED_BODY()

public:
	AAuraFireNadoArea();

	void InitializeArea(float InRadius, float InLifetime);
	float GetAreaRadius() const { return AreaRadius; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Fire Nado|Presentation")
	void OnAreaConfigured(float Radius);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire Nado")
	TObjectPtr<USphereComponent> AreaSphere;

private:
	UFUNCTION()
	void OnRep_AreaRadius();

	UPROPERTY(ReplicatedUsing = OnRep_AreaRadius)
	float AreaRadius = 150.f;
};
