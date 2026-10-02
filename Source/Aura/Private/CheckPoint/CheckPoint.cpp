// Fill out your copyright notice in the Description page of Project Settings.


#include "CheckPoint/CheckPoint.h"

#include "Aura/Aura.h"
#include "Components/SphereComponent.h"
#include "GameMode/AURAGameModeBase.h"
#include "Interfaction/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

ACheckPoint::ACheckPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	CheckpointMesh = CreateDefaultSubobject<UStaticMeshComponent>("CheckpointMesh");
	CheckpointMesh->SetupAttachment(GetRootComponent());
	CheckpointMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CheckpointMesh->SetCollisionResponseToAllChannels(ECR_Block);

	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	Sphere->SetupAttachment(CheckpointMesh);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	MoveToComponent = CreateDefaultSubobject<USceneComponent>("MoveToComponent");
	MoveToComponent->SetupAttachment(GetRootComponent());

	CheckpointMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_TAN);
	CheckpointMesh->MarkRenderStateDirty();
}

void ACheckPoint::HighlightActor_Implementation()
{
	CheckpointMesh->SetRenderCustomDepth(true);
}

void ACheckPoint::UnHighlightActor_Implementation()
{
	CheckpointMesh->SetRenderCustomDepth(false);
}

void ACheckPoint::SetMoveToLocation_Implementation(FVector& OutDestination)
{
	OutDestination = MoveToComponent->GetComponentLocation();
}

void ACheckPoint::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IsValid(OtherActor) && OtherActor->Implements<UPlayerInterface>())
	{
		if (!bReached)
		{
			bReached = true;
			if (HasAuthority())
			{
				AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(this));
				if (!IsValid(AuraGameMode) || !AuraGameMode->SaveWorldState(GetWorld()))
				{
					bReached = false;
					UE_LOG(LogTemp, Error, TEXT("CheckPoint %s was not reached because world state could not be saved"), *GetName());
					return;
				}
				IPlayerInterface::Execute_SaveProgress(OtherActor, PlayerStartTag);
			}
			HandleGlowEffects();
		}
	}
}

void ACheckPoint::BeginPlay()
{
	Super::BeginPlay();

	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ACheckPoint::OnSphereOverlap);
}

void ACheckPoint::HandleGlowEffects()
{
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UMaterialInstanceDynamic* DynamicMaterialInstance = UMaterialInstanceDynamic::Create(CheckpointMesh->GetMaterial(0), this);
	CheckpointMesh->SetMaterial(0, DynamicMaterialInstance);
	CheckpointReached(DynamicMaterialInstance);
}

bool ACheckPoint::ShouldLoadTransform_Implementation()
{
	return false;
}

void ACheckPoint::LoadActor_Implementation()
{
	if (bReached)
	{
		HandleGlowEffects();
	}
}
