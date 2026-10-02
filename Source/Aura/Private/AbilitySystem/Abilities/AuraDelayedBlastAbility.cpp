#include "AbilitySystem/Abilities/AuraDelayedBlastAbility.h"

#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Actor/AuraDelayedBlastActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaction/CombatInterface.h"
#include "Tags/AuraGameplayTags.h"

UAuraDelayedBlastAbility::UAuraDelayedBlastAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = false;
	BlastActorClass = AAuraDelayedBlastActor::StaticClass();
}

void UAuraDelayedBlastAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bServerResolvedTarget = false;
	bServerBlastSpawned = false;
	bClientCastStarted = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive() || !ActorInfo)
	{
		return;
	}

	UTargetDataUnderMouse* const TargetDataTask = UTargetDataUnderMouse::CreateTargetDataUnderMouse(this);
	if (!IsValid(TargetDataTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TargetDataTask->ValidData.AddDynamic(this, &UAuraDelayedBlastAbility::OnTargetDataReady);
	TargetDataTask->ReadyForActivation();
}

void UAuraDelayedBlastAbility::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (!IsActive() || !ActorInfo)
	{
		return;
	}

	if (!CastMontage)
	{
		// The owning client only completes its prediction window. It never commits or spawns;
		// the server callback below is the sole acceptance path.
		if (!ActorInfo->IsNetAuthority())
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
			return;
		}

		if (bServerResolvedTarget)
		{
			return;
		}
		bServerResolvedTarget = true;

		FVector TargetLocation = FVector::ZeroVector;
		if (!TryGetValidatedTargetLocation(TargetData, TargetLocation) || !SpawnBlast(TargetLocation))
		{
			EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
			return;
		}

		// The actor owns the delayed work from this point onward, so the ability can end now.
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
		return;
	}

	// With a cast montage the ability stays active until the montage ends. The client only
	// mirrors the cast; the server keeps the validated point and waits for the notify.
	if (!ActorInfo->IsNetAuthority())
	{
		if (bClientCastStarted)
		{
			return;
		}
		bClientCastStarted = true;

		// The client mirrors the cast and never runs the server-only validated trace, so the
		// facing point comes straight from the cursor hit. Without a blocking hit the montage
		// still plays, only the warp is skipped.
		const FGameplayAbilityTargetData* const RawTargetData = TargetData.Num() == 1 ? TargetData.Get(0) : nullptr;
		const FHitResult* const ClientHitResult = RawTargetData ? RawTargetData->GetHitResult() : nullptr;
		if (ClientHitResult && ClientHitResult->bBlockingHit &&
			FMath::IsFinite(ClientHitResult->ImpactPoint.X) &&
			FMath::IsFinite(ClientHitResult->ImpactPoint.Y) &&
			FMath::IsFinite(ClientHitResult->ImpactPoint.Z))
		{
			UpdateAvatarFacingTarget(ClientHitResult->ImpactPoint);
		}

		StartCastTaskFlow();
		return;
	}

	if (bServerResolvedTarget)
	{
		return;
	}
	bServerResolvedTarget = true;

	FVector TargetLocation = FVector::ZeroVector;
	if (!TryGetValidatedTargetLocation(TargetData, TargetLocation))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	PendingTargetLocation = TargetLocation;
	UpdateAvatarFacingTarget(PendingTargetLocation);
	StartCastTaskFlow();
}

void UAuraDelayedBlastAbility::StartCastTaskFlow()
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (!IsActive() || !ActorInfo)
	{
		return;
	}

	if (!CastMontage)
	{
		// Never leave the caster frozen with no montage that could end the cast.
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	UAbilityTask_WaitGameplayEvent* const EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FAuraGameplayTags::Get().Event_Montage_DelayBlast,
		nullptr,
		true,
		true);
	UAbilityTask_PlayMontageAndWait* const MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, CastMontage);
	if (!IsValid(EventTask) || !IsValid(MontageTask))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	EventTask->EventReceived.AddDynamic(this, &UAuraDelayedBlastAbility::OnDelayBlastNotify);
	MontageTask->OnCompleted.AddDynamic(this, &UAuraDelayedBlastAbility::OnCastMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UAuraDelayedBlastAbility::OnCastMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &UAuraDelayedBlastAbility::OnCastMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UAuraDelayedBlastAbility::OnCastMontageCancelled);

	// The listener must be live before the montage can fire its notify.
	EventTask->ReadyForActivation();
	MontageTask->ReadyForActivation();
}

void UAuraDelayedBlastAbility::OnDelayBlastNotify(FGameplayEventData /*Payload*/)
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	// The client only mirrors the cast montage; the server owns the spawn and the commit.
	if (!IsActive() || !ActorInfo || !ActorInfo->IsNetAuthority() || bServerBlastSpawned)
	{
		return;
	}

	bServerBlastSpawned = true;
	if (!SpawnBlast(PendingTargetLocation))
	{
		bServerBlastSpawned = false;
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
	}
}

void UAuraDelayedBlastAbility::OnCastMontageCompleted()
{
	EndCastFlow(false);
}

void UAuraDelayedBlastAbility::OnCastMontageBlendOut()
{
	EndCastFlow(false);
}

void UAuraDelayedBlastAbility::OnCastMontageInterrupted()
{
	EndCastFlow(true);
}

void UAuraDelayedBlastAbility::OnCastMontageCancelled()
{
	EndCastFlow(true);
}

void UAuraDelayedBlastAbility::EndCastFlow(bool bWasCancelled)
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (!IsActive() || !ActorInfo)
	{
		return;
	}

	// Reaching here without a notify means nothing was spawned and nothing was committed.
	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, bWasCancelled);
}

void UAuraDelayedBlastAbility::UpdateAvatarFacingTarget(const FVector& FacingLocation)
{
	AActor* const AvatarActor = GetAvatarActorFromActorInfo();
	// The facing point is a presentation-only hint; an avatar without the interface simply
	// casts without warping instead of failing the ability.
	if (IsValid(AvatarActor) && AvatarActor->Implements<UCombatInterface>())
	{
		ICombatInterface::Execute_UpdateFacingTarget(AvatarActor, FacingLocation);
	}
}

bool UAuraDelayedBlastAbility::TryGetValidatedTargetLocation(
	const FGameplayAbilityTargetDataHandle& TargetData,
	FVector& OutTargetLocation) const
{
	OutTargetLocation = FVector::ZeroVector;
	if (TargetData.Num() != 1)
	{
		return false;
	}

	const FGameplayAbilityTargetData* const RawTargetData = TargetData.Get(0);
	const FHitResult* const ClientHitResult = RawTargetData ? RawTargetData->GetHitResult() : nullptr;
	if (!ClientHitResult || !ClientHitResult->bBlockingHit)
	{
		return false;
	}

	const FVector CandidateLocation = ClientHitResult->ImpactPoint;
	if (!FMath::IsFinite(CandidateLocation.X) ||
		!FMath::IsFinite(CandidateLocation.Y) ||
		!FMath::IsFinite(CandidateLocation.Z))
	{
		return false;
	}

	AActor* const AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	if (!IsValid(AvatarActor) || !IsValid(World) || !AvatarActor->HasAuthority())
	{
		return false;
	}

	// A cursor hit on a pawn/combat actor is not a ground point. The server then traces
	// around the candidate to prevent a client from inventing an arbitrary location.
	AActor* const ClientHitActor = ClientHitResult->GetActor();
	if (IsValid(ClientHitActor) &&
		(ClientHitActor == AvatarActor || Cast<APawn>(ClientHitActor) || ClientHitActor->Implements<UCombatInterface>()))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DelayedBlastGroundValidation), true);
	QueryParams.AddIgnoredActor(AvatarActor);
	FHitResult ServerGroundHit;
	const FVector TraceStart = CandidateLocation + FVector(0.f, 0.f, 100.f);
	const FVector TraceEnd = CandidateLocation - FVector(0.f, 0.f, 100.f);
	if (!World->LineTraceSingleByChannel(
		ServerGroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams) ||
		!ServerGroundHit.bBlockingHit ||
		!FMath::IsFinite(ServerGroundHit.ImpactNormal.Z) ||
		ServerGroundHit.ImpactNormal.Z < 0.5f)
	{
		return false;
	}

	// The validation trace must resolve close to the submitted hit point. This keeps
	// the accepted center on server-visible ground while preserving the cursor point.
	if ((ServerGroundHit.ImpactPoint - CandidateLocation).SizeSquared() > FMath::Square(10.f))
	{
		return false;
	}

	AActor* const ServerGroundActor = ServerGroundHit.GetActor();
	if (IsValid(ServerGroundActor) &&
		(Cast<APawn>(ServerGroundActor) || ServerGroundActor->Implements<UCombatInterface>()))
	{
		return false;
	}

	OutTargetLocation = CandidateLocation;
	return true;
}

bool UAuraDelayedBlastAbility::SpawnBlast(const FVector& TargetLocation)
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	AActor* const AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* const World = GetWorld();
	UAbilitySystemComponent* const SourceASC = GetAbilitySystemComponentFromActorInfo();
	APawn* const InstigatorPawn = Cast<APawn>(AvatarActor);

	if (!ActorInfo || !ActorInfo->IsNetAuthority() ||
		!IsValid(AvatarActor) || !AvatarActor->HasAuthority() ||
		!IsValid(World) || !IsValid(SourceASC) || !IsValid(InstigatorPawn) ||
		!BlastActorClass || !DamageEffectClass || !CostGameplayEffectClass || !CooldownGameplayEffectClass)
	{
		return false;
	}

	const AAuraDelayedBlastActor* const BlastCDO = BlastActorClass->GetDefaultObject<AAuraDelayedBlastActor>();
	if (!BlastCDO ||
		!FMath::IsFinite(BlastCDO->GetBlastRadius()) || BlastCDO->GetBlastRadius() <= 0.f ||
		!FMath::IsFinite(BlastCDO->GetWarningDelay()) || BlastCDO->GetWarningDelay() < 0.f)
	{
		return false;
	}

	const FDamageEffectParams DamageParams = MakeDamageEffectParamsFromClassDefaults();
	if (!IsValid(DamageParams.SourceAbilitySystemComponent) || !DamageParams.DamageGameplayEffectClass)
	{
		return false;
	}

	AActor* const OwnerActor = IsValid(GetOwningActorFromActorInfo())
		? GetOwningActorFromActorInfo()
		: AvatarActor;
	const FTransform SpawnTransform(FRotator::ZeroRotator, TargetLocation);
	AAuraDelayedBlastActor* const BlastActor = World->SpawnActorDeferred<AAuraDelayedBlastActor>(
		BlastActorClass,
		SpawnTransform,
		OwnerActor,
		InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(BlastActor))
	{
		return false;
	}

	BlastActor->InitializeBlast(TargetLocation, DamageParams);
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		// The deferred actor has not begun play and therefore has not shown a warning.
		BlastActor->Destroy();
		return false;
	}

	BlastActor->FinishSpawning(SpawnTransform);
	// A zero warning delay can legitimately explode and destroy the actor during BeginPlay.
	ReportSuccessfulCastForVoice();
	return true;
}

bool UAuraDelayedBlastAbility::TryGetBlastPresentationValues(
	float& OutWarningDelay,
	float& OutBlastRadius) const
{
	OutWarningDelay = 0.f;
	OutBlastRadius = 0.f;

	if (!BlastActorClass)
	{
		return false;
	}

	const AAuraDelayedBlastActor* const BlastCDO = BlastActorClass->GetDefaultObject<AAuraDelayedBlastActor>();
	if (!BlastCDO)
	{
		return false;
	}

	const float WarningDelay = BlastCDO->GetWarningDelay();
	const float BlastRadius = BlastCDO->GetBlastRadius();
	if (!FMath::IsFinite(WarningDelay) || WarningDelay < 0.f ||
		!FMath::IsFinite(BlastRadius) || BlastRadius <= 0.f)
	{
		return false;
	}

	OutWarningDelay = WarningDelay;
	OutBlastRadius = BlastRadius;
	return true;
}

