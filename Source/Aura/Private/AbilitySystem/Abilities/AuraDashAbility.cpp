// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AuraDashAbility.h"

#include "AbilitySystem/AbilityTasks/TargetDataDashDirection.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Tags/AuraGameplayTags.h"

namespace
{
	constexpr float MinBlinkDistance = 25.f;
	constexpr double MinBlinkDirectionLength = 1.0;
}

UAuraDashAbility::UAuraDashAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

FString UAuraDashAbility::GetDescription(int32 Level)
{
	return FString::Printf(TEXT(
		"<Title>闪现</>\n\n"
		"<Small>等级：</><Level>%d</>\n"
		"<Small>冷却时间：</><Cooldown>%.1f</>\n\n"
		"<Default>按空格朝鼠标方向瞬移，最远 %.0f 厘米；若鼠标方向无效，则朝角色面向的方向移动。"
		"遇到阻挡物会停在其前方；没有安全落点时不会进入冷却。</>"),
		Level, GetCooldown(Level), BlinkDistance);
}

FString UAuraDashAbility::GetNextLevelDescription(int32 Level)
{
	return TEXT("<Title>闪现</>\n\n<Default>初始固定技能，目前没有升级效果。</>");
}

FGameplayTag UAuraDashAbility::GetCooldownTag() const
{
	return CooldownTag.IsValid() ? CooldownTag : FAuraGameplayTags::Get().Cooldown_Movement_Dash;
}

void UAuraDashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (bBlinkInProgress)
	{
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	if (!StartCueTag.IsValid())
	{
		StartCueTag = Tags.GameplayCue_Blink_Start;
	}
	if (!EndCueTag.IsValid())
	{
		EndCueTag = Tags.GameplayCue_Blink_End;
	}
	if (!CooldownTag.IsValid())
	{
		CooldownTag = Tags.Cooldown_Movement_Dash;
	}

	UTargetDataDashDirection* const DirectionTask = UTargetDataDashDirection::CreateTargetDataDashDirection(this);
	if (!IsValid(DirectionTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bBlinkInProgress = true;
	DirectionTask->ValidData.AddDynamic(this, &UAuraDashAbility::OnTargetDataReady);
	DirectionTask->ReadyForActivation();
}

void UAuraDashAbility::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, const FVector& DashDirection)
{
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	if (!IsActive() || !ActorInfo)
	{
		return;
	}

	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Character))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}

	const FVector Direction = ResolveBlinkDirection(DashDirection, Character);
	FVector Destination = FVector::ZeroVector;
	if (Direction.IsNearlyZero() || !ComputeSafeBlinkDestination(Character, Direction, Destination))
	{
		// 预测窗口和 ActivateAbility 还在栈上，下一帧再结束，避免把技能系统的预测键留脏。
		RequestEndBlink(false);
		return;
	}

	if (!CommitAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo()))
	{
		RequestEndBlink(true);
		return;
	}

	FRotator FaceRotation = Character->GetActorRotation();
	if (bFaceBlinkDirection)
	{
		FaceRotation = FRotator(0.f, Direction.Rotation().Yaw, 0.f);
		Character->SetActorRotation(FaceRotation);
	}

	const FVector StartLocation = Character->GetActorLocation();
	PlayCosmeticMontage(Character);
	ExecuteCueAtLocation(StartCueTag, StartLocation);
	PlayBlinkSoundAtLocation(BlinkStartSound, StartLocation);

	Character->TeleportTo(Destination, FaceRotation, false, true);
	if (UCharacterMovementComponent* const Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	ExecuteTrailCue(TrailCueTag, StartLocation, Destination);
	ExecuteCueAtLocation(EndCueTag, Destination);
	PlayBlinkSoundAtLocation(BlinkEndSound, Destination);
	RequestEndBlink(false);
}

void UAuraDashAbility::RequestEndBlink(bool bWasCancelled)
{
	bPendingEndWasCancelled = bWasCancelled;
	UWorld* const World = GetWorld();
	if (!World)
	{
		FinishBlink();
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(this, &UAuraDashAbility::FinishBlink);
}

void UAuraDashAbility::FinishBlink()
{
	if (!IsActive())
	{
		bBlinkInProgress = false;
		return;
	}

	// 闪现蒙太奇和火球术共用 DefaultSlot。不立刻停掉的话，槽位会被占住，火球术蒙太奇播不出来。
	StopCosmeticMontage();
	const FGameplayAbilityActorInfo* const ActorInfo = GetCurrentActorInfo();
	EndAbility(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, true, bPendingEndWasCancelled);
}

FVector UAuraDashAbility::ResolveBlinkDirection(const FVector& SubmittedDirection, const AActor* Avatar) const
{
	const FVector Horizontal(SubmittedDirection.X, SubmittedDirection.Y, 0.0);
	if (Horizontal.SizeSquared() > FMath::Square(MinBlinkDirectionLength))
	{
		return Horizontal.GetSafeNormal();
	}

	if (!IsValid(Avatar))
	{
		return FVector::ZeroVector;
	}

	const FVector Forward(Avatar->GetActorForwardVector().X, Avatar->GetActorForwardVector().Y, 0.0);
	return Forward.GetSafeNormal();
}

bool UAuraDashAbility::ComputeSafeBlinkDestination(const ACharacter* Character, const FVector& Direction, FVector& OutDestination) const
{
	if (!IsValid(Character) || Direction.IsNearlyZero())
	{
		return false;
	}

	UWorld* const World = Character->GetWorld();
	const UCapsuleComponent* const Capsule = Character->GetCapsuleComponent();
	if (!World || !Capsule || !FMath::IsFinite(BlinkDistance) || BlinkDistance <= 0.f)
	{
		return false;
	}

	const FVector Start = Character->GetActorLocation();
	const FVector Desired = Start + Direction * BlinkDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AuraBlink), false, Character);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());

	// Pawn 通道：墙和地形 Block，遭遇区 / 触发器 / 检查点为 Overlap，不会被当成墙。
	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(
		Hit, Start, Desired, FQuat::Identity, ECC_Pawn, Shape, Params);

	FVector SafeDestination = Desired;
	if (bHit && Hit.bStartPenetrating)
	{
		return false;
	}
	if (bHit)
	{
		SafeDestination = Hit.Location - Direction * StaticObstacleClearance;
	}

	if (FVector::Dist2D(Start, SafeDestination) < MinBlinkDistance)
	{
		return false;
	}

	OutDestination = SafeDestination;
	return true;
}

void UAuraDashAbility::PlayCosmeticMontage(ACharacter* Character) const
{
	if (!StartMontage || !IsValid(Character))
	{
		return;
	}

	USkeletalMeshComponent* const Mesh = Character->GetMesh();
	UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	AnimInstance->Montage_Play(StartMontage);
	if (FAnimMontageInstance* const MontageInstance = AnimInstance->GetActiveInstanceForMontage(StartMontage))
	{
		MontageInstance->PushDisableRootMotion();
	}
}

void UAuraDashAbility::StopCosmeticMontage() const
{
	ACharacter* const Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* const Mesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (AnimInstance && StartMontage && AnimInstance->Montage_IsPlaying(StartMontage))
	{
		AnimInstance->Montage_Stop(0.05f, StartMontage);
	}
}

void UAuraDashAbility::PlayBlinkSoundAtLocation(USoundBase* Sound, const FVector& Location) const
{
	AActor* const Avatar = GetAvatarActorFromActorInfo();
	if (!Sound || !IsValid(Avatar))
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(Avatar, Sound, Location);
}

void UAuraDashAbility::ExecuteCueAtLocation(const FGameplayTag& CueTag, const FVector& Location) const
{
	if (!CueTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* const AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystem)
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Location;
	Parameters.Instigator = GetAvatarActorFromActorInfo();
	AbilitySystem->ExecuteGameplayCue(CueTag, Parameters);
}

void UAuraDashAbility::ExecuteTrailCue(const FGameplayTag& CueTag, const FVector& TrailStart, const FVector& TrailEnd) const
{
	if (!CueTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* const AbilitySystem = GetAbilitySystemComponentFromActorInfo();
	if (!AbilitySystem)
	{
		return;
	}

	FHitResult TrailHit;
	TrailHit.Location = TrailStart;
	TrailHit.ImpactPoint = TrailEnd;
	TrailHit.bBlockingHit = true;

	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddHitResult(TrailHit, true);

	FGameplayCueParameters Parameters;
	Parameters.Location = TrailStart;
	Parameters.EffectContext = EffectContext;
	Parameters.Instigator = GetAvatarActorFromActorInfo();
	AbilitySystem->ExecuteGameplayCue(CueTag, Parameters);
}

void UAuraDashAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	bBlinkInProgress = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
