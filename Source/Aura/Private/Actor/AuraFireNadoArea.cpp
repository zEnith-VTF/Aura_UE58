#include "Actor/AuraFireNadoArea.h"

#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"

AAuraFireNadoArea::AAuraFireNadoArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	SetRootComponent(AreaSphere);
	AreaSphere->InitSphereRadius(AreaRadius);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AreaSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	AreaSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AreaSphere->SetGenerateOverlapEvents(false);
}

void AAuraFireNadoArea::InitializeArea(const float InRadius, const float InLifetime)
{
	if (!HasAuthority()) return;

	AreaRadius = InRadius;
	AreaSphere->SetSphereRadius(AreaRadius);
	SetLifeSpan(InLifetime);
}

void AAuraFireNadoArea::BeginPlay()
{
	Super::BeginPlay();
	OnAreaConfigured(AreaRadius);
}

void AAuraFireNadoArea::OnRep_AreaRadius()
{
	AreaSphere->SetSphereRadius(AreaRadius);
	OnAreaConfigured(AreaRadius);
}

void AAuraFireNadoArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAuraFireNadoArea, AreaRadius);
}
