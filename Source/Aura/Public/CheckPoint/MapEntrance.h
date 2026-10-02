// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaction/AuraInteractionInterface.h"
#include "Interfaction/HighlightInterface.h"
#include "Interfaction/SaveInterface.h"
#include "MapEntrance.generated.h"

class APawn;
class UChildActorComponent;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
class UWorld;

UCLASS()
class AURA_API AMapEntrance : public AActor, public IHighlightInterface, public ISaveInterface, public IAuraInteractionInterface
{
	GENERATED_BODY()

public:
	AMapEntrance(const FObjectInitializer& ObjectInitializer);

	virtual void HighlightActor_Implementation() override;
	virtual void UnHighlightActor_Implementation() override;
	virtual void SetMoveToLocation_Implementation(FVector& OutDestination) override;
	virtual bool ShouldLoadTransform_Implementation() override;
	virtual void LoadActor_Implementation() override;

	virtual bool QueryInteraction_Implementation(APawn* Interactor, FAuraInteractionInfo& OutInfo) override;
	/** 服务器调用；返回 true 只表示这次传送请求已接受。 */
	virtual bool TryInteract_Implementation(APawn* Interactor) override;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Map Entrance")
	bool bReached = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Entrance")
	TSoftObjectPtr<UWorld> DestinationMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Entrance")
	FName DestinationPlayerStartTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Entrance|Highlight")
	FName HighlightComponentTag = TEXT("Highlight");

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	void SetEntranceHighlighted(bool bHighlighted);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Entrance|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Entrance|Components")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Entrance|Components")
	TObjectPtr<USceneComponent> MoveToComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Entrance|Components")
	TObjectPtr<UChildActorComponent> ChildActor;

private:
	bool bTravelRequested = false;
};
