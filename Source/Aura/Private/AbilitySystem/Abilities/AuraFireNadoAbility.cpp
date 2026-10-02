#include "AbilitySystem/Abilities/AuraFireNadoAbility.h"

#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Actor/AuraFireNadoActor.h"
#include "Actor/AuraFireNadoArea.h"
#include "Actor/AuraFireNadoField.h"
#include "Animation/AnimMontage.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaction/CombatInterface.h"
#include "Tags/AuraGameplayTags.h"

namespace
{
	bool IsLivingFireNadoCaster(const AActor* Avatar)
	{
		return IsValid(Avatar) && Avatar->Implements<UCombatInterface>() &&
			!ICombatInterface::Execute_IsDead(Avatar);
	}
}

UAuraFireNadoAbility::UAuraFireNadoAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = false;
	TornadoClass = AAuraFireNadoActor::StaticClass();
}

void UAuraFireNadoAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bTargetResolved = false;
	bSpawned = false;
	LockedDirection = FVector::ZeroVector;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive() || !ActorInfo) return;

	if (!IsLivingFireNadoCaster(GetAvatarActorFromActorInfo()) || !IsValid(CastMontage))
	{
		EndCastFlow(true);
		return;
	}
	const float MontageRate = FMath::Abs(CastMontage->RateScale);
	const float MontageLength = CastMontage->GetPlayLength();
	if (!FMath::IsFinite(MontageRate) || MontageRate <= KINDA_SMALL_NUMBER ||
		!FMath::IsFinite(MontageLength) || MontageLength <= 0.f)
	{
		EndCastFlow(true);
		return;
	}
	const float Timeout = MontageLength / MontageRate + 5.f;
	if (!FMath::IsFinite(Timeout))
	{
		EndCastFlow(true);
		return;
	}
	// Covers remote target-data waits as well as a montage that never completes.
	UAbilityTask_WaitDelay* const TimeoutTask = UAbilityTask_WaitDelay::WaitDelay(this, Timeout);
	if (!IsValid(TimeoutTask))
	{
		EndCastFlow(true);
		return;
	}
	TimeoutTask->OnFinish.AddDynamic(this, &UAuraFireNadoAbility::OnCastInterrupted);
	TimeoutTask->ReadyForActivation();

	UTargetDataUnderMouse* const Task = UTargetDataUnderMouse::CreateTargetDataUnderMouse(this, true);
	if (!IsValid(Task))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Task->ValidData.AddDynamic(this, &UAuraFireNadoAbility::OnTargetDataReady);
	Task->ReadyForActivation();
}

void UAuraFireNadoAbility::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData)
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (!IsActive() || !ActorInfo || bTargetResolved) return;
	bTargetResolved = true;

	if (!IsLivingFireNadoCaster(GetAvatarActorFromActorInfo()) ||
		!TryGetAimDirection(TargetData, LockedDirection) || !IsValid(CastMontage) || !CastEventTag.IsValid())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (IsValid(Avatar) && Avatar->Implements<UCombatInterface>())
	{
		ICombatInterface::Execute_UpdateFacingTarget(Avatar, Avatar->GetActorLocation() + LockedDirection * 1000.f);
	}
	StartCastFlow();
}

bool UAuraFireNadoAbility::TryGetAimDirection(
	const FGameplayAbilityTargetDataHandle& TargetData, FVector& OutDirection) const
{
	OutDirection = FVector::ZeroVector;
	if (TargetData.Num() != 1) return false;
	const FGameplayAbilityTargetData* const RawData = TargetData.Get(0);
	const FHitResult* const Hit = RawData ? RawData->GetHitResult() : nullptr;
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Hit || !Hit->bBlockingHit || !IsValid(Avatar) ||
		!FMath::IsFinite(Hit->ImpactPoint.X) || !FMath::IsFinite(Hit->ImpactPoint.Y) ||
		!FMath::IsFinite(Hit->ImpactPoint.Z)) return false;

	const FVector Delta = Hit->ImpactPoint - Avatar->GetActorLocation();
	const double LengthSquared = Delta.SizeSquared2D();
	if (!FMath::IsFinite(Delta.X) || !FMath::IsFinite(Delta.Y) || !FMath::IsFinite(Delta.Z) ||
		!FMath::IsFinite(LengthSquared) || LengthSquared < FMath::Square(10.f)) return false;
	OutDirection = Delta.GetSafeNormal2D();
	return FMath::IsFinite(OutDirection.X) && FMath::IsFinite(OutDirection.Y) &&
		FMath::IsFinite(OutDirection.Z) && !OutDirection.IsNearlyZero();
}

void UAuraFireNadoAbility::StartCastFlow()
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (!IsActive() || !ActorInfo) return;
	if (!IsLivingFireNadoCaster(GetAvatarActorFromActorInfo()) || !IsValid(CastMontage))
	{
		EndCastFlow(true);
		return;
	}

	UAbilityTask_WaitGameplayEvent* const EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CastEventTag, nullptr, true, true);
	UAbilityTask_PlayMontageAndWait* const MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, CastMontage);
	if (!IsValid(EventTask) || !IsValid(MontageTask))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	EventTask->EventReceived.AddDynamic(this, &UAuraFireNadoAbility::OnCastNotify);
	MontageTask->OnCompleted.AddDynamic(this, &UAuraFireNadoAbility::OnCastCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UAuraFireNadoAbility::OnCastCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UAuraFireNadoAbility::OnCastInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UAuraFireNadoAbility::OnCastInterrupted);
	EventTask->ReadyForActivation();
	MontageTask->ReadyForActivation();
}

void UAuraFireNadoAbility::OnCastNotify(FGameplayEventData Payload)
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (!IsActive() || !ActorInfo || !ActorInfo->IsNetAuthority() || bSpawned || !bTargetResolved) return;
	if (!IsLivingFireNadoCaster(GetAvatarActorFromActorInfo()))
	{
		EndCastFlow(true);
		return;
	}
	bSpawned = true;
	if (!SpawnTornado())
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
	}
}

void UAuraFireNadoAbility::OnCastCompleted()
{
	EndCastFlow(false);
}

void UAuraFireNadoAbility::OnCastInterrupted()
{
	EndCastFlow(true);
}

void UAuraFireNadoAbility::EndCastFlow(const bool bWasCancelled)
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (IsActive() && ActorInfo)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, bWasCancelled);
	}
}

bool UAuraFireNadoAbility::SpawnTornado()
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	APawn* const InstigatorPawn = Cast<APawn>(Avatar);
	UWorld* const World = GetWorld();
	UAbilitySystemComponent* const SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !IsLivingFireNadoCaster(Avatar) || !Avatar->HasAuthority() ||
		!IsValid(InstigatorPawn) || !IsValid(World) || !IsValid(SourceASC) || !CastMontage ||
		!TornadoClass || !AreaClass || !DamageEffectClass || !CostGameplayEffectClass ||
		!CooldownGameplayEffectClass || !DamageType.IsValid() ||
		!FMath::IsFinite(LockedDirection.X) || !FMath::IsFinite(LockedDirection.Y) ||
		!FMath::IsFinite(LockedDirection.Z) || LockedDirection.IsNearlyZero()) return false;

	const int32 Level = GetAbilityLevel();
	const float Distance = MaxTravelDistance.GetValueAtLevel(Level);
	const float Lifetime = AreaLifetime.GetValueAtLevel(Level);
	const float DirectDamage = Damage.GetValueAtLevel(Level);
	const float PeriodicDamage = AreaDamage.GetValueAtLevel(Level);
	if (!FMath::IsFinite(Distance) || Distance <= 0.f ||
		!FMath::IsFinite(Lifetime) || Lifetime <= 0.f ||
		!FMath::IsFinite(DirectDamage) || DirectDamage <= 0.f ||
		!FMath::IsFinite(PeriodicDamage) || PeriodicDamage <= 0.f ||
		!FMath::IsFinite(TravelSpeed) || TravelSpeed <= 0.f ||
		!FMath::IsFinite(AreaSpacing) || AreaSpacing <= 0.f ||
		!FMath::IsFinite(AreaRadius) || AreaRadius <= 0.f ||
		!FMath::IsFinite(AreaDamagePeriod) || AreaDamagePeriod <= 0.f ||
		!FMath::IsFinite(SpawnForwardOffset) || !FMath::IsFinite(SpawnHeightOffset) ||
		!FMath::IsFinite(Distance / TravelSpeed) || MaxAreasPerCast < 1 ||
		FMath::CeilToDouble(static_cast<double>(Distance) / AreaSpacing) + 1.0 > MaxAreasPerCast) return false;

	FDamageEffectParams DirectParams = MakeDamageEffectParamsFromClassDefaults();
	if (!IsValid(DirectParams.SourceAbilitySystemComponent) || !DirectParams.DamageGameplayEffectClass) return false;
	FDamageEffectParams AreaParams = DirectParams;
	AreaParams.BaseDamage = PeriodicDamage;

	AActor* const OwnerActor = IsValid(GetOwningActorFromActorInfo()) ? GetOwningActorFromActorInfo() : Avatar;
	const FVector SpawnLocation = Avatar->GetActorLocation() + LockedDirection * SpawnForwardOffset +
		FVector(0.f, 0.f, SpawnHeightOffset);
	const AAuraFireNadoActor* const TornadoDefaults = TornadoClass->GetDefaultObject<AAuraFireNadoActor>();
	const USphereComponent* const SpawnSphere = IsValid(TornadoDefaults) ?
		Cast<USphereComponent>(TornadoDefaults->GetRootComponent()) : nullptr;
	if (!IsValid(SpawnSphere) || !FMath::IsFinite(SpawnSphere->GetScaledSphereRadius()) ||
		SpawnSphere->GetScaledSphereRadius() <= 0.f || SpawnLocation.ContainsNaN()) return false;

	const FTransform SpawnTransform(LockedDirection.Rotation(), SpawnLocation);
	AAuraFireNadoField* const Field = World->SpawnActorDeferred<AAuraFireNadoField>(
		AAuraFireNadoField::StaticClass(), FTransform(FRotator::ZeroRotator, SpawnLocation),
		OwnerActor, InstigatorPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Field)) return false;

	AAuraFireNadoActor* const Tornado = World->SpawnActorDeferred<AAuraFireNadoActor>(
		TornadoClass, SpawnTransform, OwnerActor, InstigatorPawn,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Tornado))
	{
		Field->Destroy();
		return false;
	}

	Field->InitializeField(AreaParams, AreaDamagePeriod);
	Tornado->InitializeTornado(LockedDirection, TravelSpeed, Distance, AreaSpacing,
		AreaRadius, Lifetime, MaxAreasPerCast, AreaClass, Field, DirectParams);
	// Recheck after deferred actors have been constructed, before any cost is applied.
	if (!IsActive() || !IsLivingFireNadoCaster(Avatar) || !IsValid(SourceASC) ||
		SourceASC->GetAvatarActor() != Avatar)
	{
		Tornado->Destroy();
		Field->Destroy();
		return false;
	}
	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		Tornado->Destroy();
		Field->Destroy();
		return false;
	}

	Field->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation));
	Tornado->FinishSpawning(SpawnTransform);
	ReportSuccessfulCastForVoice();
	return true;
}

FString UAuraFireNadoAbility::BuildDescription(const int32 Level, const bool bNextLevel) const
{
	return FString::Printf(
		TEXT("<Title>%s</>\n\n<Small>技能等级：</><Level>%d</>\n<Small>法力消耗：</><ManaCost>%.1f</>\n<Small>冷却时间：</><Cooldown>%.1f</><Default> 秒</>\n\n<Default>向施法方向释放穿透敌人的烈火飓风，行进距离 %.1f 米。本体每名敌人命中一次，造成 </><Damage>%.0f</><Default> 点火焰伤害；沿途地面火焰持续 %.1f 秒，每个伤害周期造成 </><Damage>%.0f</><Default> 点火焰伤害。</>"),
		bNextLevel ? TEXT("下一等级：") : TEXT("烈火飓风"), Level,
		FMath::Abs(GetManaCost(Level)), GetCooldown(Level),
		MaxTravelDistance.GetValueAtLevel(Level) / 100.f,
		Damage.GetValueAtLevel(Level), AreaLifetime.GetValueAtLevel(Level),
		AreaDamage.GetValueAtLevel(Level));
}

FString UAuraFireNadoAbility::GetDescription(const int32 Level)
{
	return BuildDescription(Level, false);
}

FString UAuraFireNadoAbility::GetNextLevelDescription(const int32 Level)
{
	return BuildDescription(Level, true);
}
