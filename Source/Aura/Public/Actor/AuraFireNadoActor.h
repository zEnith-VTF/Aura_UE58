#pragma once

#include "CoreMinimal.h"
#include "AuraAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "AuraFireNadoActor.generated.h"

class AAuraFireNadoArea;
class AAuraFireNadoField;
class UProjectileMovementComponent;
class USphereComponent;
class UPrimitiveComponent;

/** Authoritative moving fire nado. Swept travel overlaps enemies and passes through the world. */
UCLASS()
class AURA_API AAuraFireNadoActor : public AActor
{
	GENERATED_BODY()

public:
	AAuraFireNadoActor();

	void InitializeTornado(
		const FVector& InDirection, float InSpeed, float InMaxDistance,
		float InAreaSpacing, float InAreaRadius, float InAreaLifetime,
		int32 InMaxAreaCount, TSubclassOf<AAuraFireNadoArea> InAreaClass,
		AAuraFireNadoField* InField, const FDamageEffectParams& InDamageParams);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire Nado")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Fire Nado")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Height above the accepted floor for each spawned area. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Area", meta = (ClampMin = "0"))
	float GroundTraceUp = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fire Nado|Area", meta = (ClampMin = "0"))
	float GroundTraceDown = 500.f;

private:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnBlock(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	void ProcessTravelTo(const FVector& NewLocation);
	void SpawnAreaAt(const FVector& PathLocation);

	UPROPERTY()
	TObjectPtr<AAuraFireNadoField> Field;

	UPROPERTY()
	TSubclassOf<AAuraFireNadoArea> AreaClass;

	UPROPERTY()
	FDamageEffectParams DamageParams;

	TSet<TWeakObjectPtr<AActor>> DamagedAvatars;
	FVector LastProcessedLocation = FVector::ZeroVector;
	FVector TravelDirection = FVector::ForwardVector;
	float Speed = 600.f;
	float MaxDistance = 1000.f;
	float AreaSpacing = 200.f;
	float AreaRadius = 150.f;
	float AreaLifetime = 3.f;
	float TravelledDistance = 0.f;
	float NextAreaDistance = 200.f;
	int32 MaxAreaCount = 64;
	int32 AreaCount = 0;
	bool bFinished = false;
};
