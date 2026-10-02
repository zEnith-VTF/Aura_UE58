#include "Actor/AuraFireNadoActor.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/AuraFireNadoArea.h"
#include "Actor/AuraFireNadoField.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interfaction/CombatInterface.h"

AAuraFireNadoActor::AAuraFireNadoActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(65.f);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->SetNotifyRigidBodyCollision(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 600.f;
	ProjectileMovement->MaxSpeed = 600.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
}

void AAuraFireNadoActor::InitializeTornado(
	const FVector& InDirection, const float InSpeed, const float InMaxDistance,
	const float InAreaSpacing, const float InAreaRadius, const float InAreaLifetime,
	const int32 InMaxAreaCount, TSubclassOf<AAuraFireNadoArea> InAreaClass,
	AAuraFireNadoField* InField, const FDamageEffectParams& InDamageParams)
{
	if (!HasAuthority()) return;
	TravelDirection = InDirection.GetSafeNormal2D();
	Speed = InSpeed;
	MaxDistance = InMaxDistance;
	AreaSpacing = InAreaSpacing;
	AreaRadius = InAreaRadius;
	AreaLifetime = InAreaLifetime;
	NextAreaDistance = FMath::Min(AreaSpacing, MaxDistance);
	MaxAreaCount = InMaxAreaCount;
	AreaClass = InAreaClass;
	Field = InField;
	DamageParams = InDamageParams;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = TravelDirection * Speed;
}

void AAuraFireNadoActor::BeginPlay()
{
	Super::BeginPlay();
	// Keep the Blueprint component reference, but let only the server actor drive movement.
	ProjectileMovement->Deactivate();
	ProjectileMovement->SetComponentTickEnabled(false);
	SetActorTickEnabled(HasAuthority());
	if (!HasAuthority())
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	// Override serialized Blueprint collision defaults: nothing may block travel.
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->SetNotifyRigidBodyCollision(false);
	LastProcessedLocation = GetActorLocation();
	if (AActor* const SourceAvatar = Cast<AActor>(DamageParams.WorldContextObject.Get()))
	{
		CollisionSphere->IgnoreActorWhenMoving(SourceAvatar, true);
	}
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AAuraFireNadoActor::OnOverlap);
	CollisionSphere->OnComponentHit.AddDynamic(this, &AAuraFireNadoActor::OnBlock);
	// Retain a finite fallback if external movement changes prevent normal completion.
	SetLifeSpan(MaxDistance / Speed + 1.f);
	if (AreaCount < MaxAreaCount)
	{
		++AreaCount;
		SpawnAreaAt(LastProcessedLocation);
	}
}

void AAuraFireNadoActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || bFinished) return;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) return;
	const float Remaining = FMath::Max(0.f, MaxDistance - TravelledDistance);
	if (Remaining <= KINDA_SMALL_NUMBER)
	{
		bFinished = true;
		Destroy();
		return;
	}
	const double Step = FMath::Min(static_cast<double>(Speed) * DeltaSeconds, static_cast<double>(Remaining));
	FHitResult Hit;
	SetActorLocation(GetActorLocation() + TravelDirection * Step, true, &Hit);
	if (bFinished) return;
	// Handle the sweep result even when a component does not deliver a hit delegate.
	if (Hit.bBlockingHit)
	{
		OnBlock(CollisionSphere, Hit.GetActor(), Hit.GetComponent(), FVector::ZeroVector, Hit);
		if (bFinished) return;
	}
	ProcessTravelTo(GetActorLocation());
}

void AAuraFireNadoActor::ProcessTravelTo(const FVector& NewLocation)
{
	if (!HasAuthority() || bFinished || NewLocation.ContainsNaN()) return;
	const float SegmentLength = FVector::Dist2D(LastProcessedLocation, NewLocation);
	if (!FMath::IsFinite(SegmentLength) || SegmentLength <= KINDA_SMALL_NUMBER) return;

	const float Remaining = FMath::Max(0.f, MaxDistance - TravelledDistance);
	const float UsedLength = FMath::Min(SegmentLength, Remaining);
	const FVector SegmentDirection = (NewLocation - LastProcessedLocation).GetSafeNormal2D();
	const bool bReachedEnd = TravelledDistance + UsedLength + KINDA_SMALL_NUMBER >= MaxDistance;
	const float EndDistance = bReachedEnd ? MaxDistance : TravelledDistance + UsedLength;
	while (!bFinished && NextAreaDistance <= EndDistance &&
		AreaCount < MaxAreaCount)
	{
		const float AlongSegment = FMath::Clamp(NextAreaDistance - TravelledDistance, 0.f, UsedLength);
		const FVector AreaLocation = LastProcessedLocation + SegmentDirection * AlongSegment;
		const bool bEndpointSample = NextAreaDistance >= MaxDistance;
		// Clamp the final sample to the endpoint, including a shorter final interval.
		if (!bEndpointSample) NextAreaDistance = FMath::Min(NextAreaDistance + AreaSpacing, MaxDistance);
		++AreaCount;
		SpawnAreaAt(AreaLocation);
		if (bEndpointSample) break;
	}
	if (bFinished) return;

	TravelledDistance = EndDistance;
	LastProcessedLocation = NewLocation;
	if (bReachedEnd)
	{
		bFinished = true;
		Destroy();
	}
}

void AAuraFireNadoActor::SpawnAreaAt(const FVector& PathLocation)
{
	UWorld* const World = GetWorld();
	if (!HasAuthority() || bFinished || !IsValid(World) || !AreaClass || !IsValid(Field) ||
		!FMath::IsFinite(GroundTraceUp) || !FMath::IsFinite(GroundTraceDown) ||
		GroundTraceUp < 0.f || GroundTraceDown < 0.f || GroundTraceUp + GroundTraceDown <= 0.f) return;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FireNadoGround), false);
	QueryParams.AddIgnoredActor(this);
	if (AActor* const SourceAvatar = GetInstigator()) QueryParams.AddIgnoredActor(SourceAvatar);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	TArray<FHitResult> GroundHits;
	World->LineTraceMultiByObjectType(GroundHits,
		PathLocation + FVector(0.f, 0.f, GroundTraceUp),
		PathLocation - FVector(0.f, 0.f, GroundTraceDown), ObjectParams, QueryParams);
	const FHitResult* GroundHit = nullptr;
	for (const FHitResult& Hit : GroundHits)
	{
		const AActor* const HitActor = Hit.GetActor();
		const UPrimitiveComponent* const HitComponent = Hit.GetComponent();
		if ((IsValid(HitActor) && (HitActor->Implements<UCombatInterface>() || HitActor->IsA<APawn>())) ||
			!IsValid(HitComponent) || HitComponent->GetCollisionResponseToChannel(
				CollisionSphere->GetCollisionObjectType()) != ECR_Block) continue;
		if (!GroundHit || Hit.Time < GroundHit->Time) GroundHit = &Hit;
	}
	if (!GroundHit || GroundHit->bStartPenetrating || GroundHit->ImpactPoint.ContainsNaN() ||
		!FMath::IsFinite(GroundHit->ImpactNormal.Z) || GroundHit->ImpactNormal.Z < 0.5f) return;

	// The authored elongated visual uses its local Y axis along the travel path.
	const FRotator AreaRotation(0.f, TravelDirection.Rotation().Yaw - 90.f, 0.f);
	const FTransform SpawnTransform(AreaRotation, GroundHit->ImpactPoint);
	AAuraFireNadoArea* const Area = World->SpawnActorDeferred<AAuraFireNadoArea>(
		AreaClass, SpawnTransform, GetOwner(), GetInstigator(),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Area)) return;

	Area->InitializeArea(AreaRadius, AreaLifetime);
	Area->FinishSpawning(SpawnTransform);
	if (IsValid(Area) && IsValid(Field)) Field->AddArea(Area);
}

void AAuraFireNadoActor::OnOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || bFinished || !IsValid(OtherActor) || !OtherActor->Implements<UCombatInterface>() ||
		ICombatInterface::Execute_IsDead(OtherActor)) return;

	UAbilitySystemComponent* const SourceASC = DamageParams.SourceAbilitySystemComponent;
	AActor* const SourceAvatar = Cast<AActor>(DamageParams.WorldContextObject.Get());
	AActor* const Avatar = ICombatInterface::Execute_GetAvatar(OtherActor);
	if (!IsValid(SourceASC) || !IsValid(SourceAvatar) || SourceASC->GetAvatarActor() != SourceAvatar ||
		!IsValid(Avatar) || Avatar == SourceAvatar ||
		UAuraAbilitySystemLibrary::AreActorsFriends(SourceAvatar, Avatar) ||
		DamagedAvatars.Contains(Avatar) ||
		(Avatar->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Avatar))) return;

	UAbilitySystemComponent* const TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Avatar);
	if (bFinished || !IsValid(TargetASC) || !IsValid(SourceASC) || !IsValid(SourceAvatar) ||
		SourceASC->GetAvatarActor() != SourceAvatar || !IsValid(Avatar) ||
		UAuraAbilitySystemLibrary::AreActorsFriends(SourceAvatar, Avatar) ||
		(Avatar->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Avatar))) return;

	DamagedAvatars.Add(Avatar);
	FDamageEffectParams TargetParams = DamageParams;
	TargetParams.TargetAbilitySystemComponent = TargetASC;
	UAuraAbilitySystemLibrary::ApplyDamageEffect(TargetParams);
}

void AAuraFireNadoActor::OnBlock(
	UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || bFinished) return;
	if (IsValid(OtherActor) &&
		(OtherActor->Implements<UCombatInterface>() || OtherActor->IsA<APawn>()))
	{
		// Auxiliary enemy components may block WorldDynamic even when the capsule overlaps.
		CollisionSphere->IgnoreActorWhenMoving(OtherActor, true);
		OnOverlap(HitComponent, OtherActor, OtherComponent, INDEX_NONE, true, Hit);
		return;
	}
	// Environment contacts do not end the cast; distance/lifespan own completion.
}

void AAuraFireNadoActor::Destroyed()
{
	bFinished = true;
	SetActorTickEnabled(false);
	if (HasAuthority() && IsValid(Field)) Field->NotifyTornadoFinished();
	Super::Destroyed();
}
