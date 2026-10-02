// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AuraChargedProjectileSpell.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "Actor/AuraProjectile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tags/AuraGameplayTags.h"

UAuraChargedProjectileSpell::UAuraChargedProjectileSpell()
{
	InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly=false;
}

void UAuraChargedProjectileSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ChargeStartTime=0;
	bHasValidCharge=false;
	bChargeQualifiedForRelease = false;
	
	AActor* AvatarActor=GetAvatarActorFromActorInfo();
	UWorld*World=GetWorld();
	
	
	if (IsValid(AvatarActor)&&AvatarActor->HasAuthority()&&IsValid(World))
	{
		ChargeStartTime=World->GetTimeSeconds();
		bHasValidCharge=true;
	}
	
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}
	UAbilityTask_WaitInputRelease* ReleaseTask=UAbilityTask_WaitInputRelease::WaitInputRelease(this,true);
	if (!IsValid(ReleaseTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ReleaseTask->OnRelease.AddDynamic(this,&UAuraChargedProjectileSpell::OnChargedReleased);
	ReleaseTask->ReadyForActivation();
}

void UAuraChargedProjectileSpell::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	ChargeStartTime=0;
	bHasValidCharge=false;
	bChargeQualifiedForRelease = false;
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAuraChargedProjectileSpell::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	if (!IsActive())return;
	UAbilitySystemComponent *AuraASC=GetAbilitySystemComponentFromActorInfo();
	
	if (!IsValid(AuraASC))return;
	
	AuraASC->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased,Handle,ActivationInfo.GetActivationPredictionKey());
		
	
}

void UAuraChargedProjectileSpell::OnChargedReleased(float TimeHeld)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const bool bAuthority =
		IsValid(AvatarActor) && AvatarActor->HasAuthority();
	
	if (!IsActive()||!ActorInfo)return;
	if (ActorInfo->IsNetAuthority())
	{
		if (IsValid(AvatarActor)&&bHasValidCharge&&GetWorld())
		{
			const double ChargedTime=GetWorld()->GetTimeSeconds()-ChargeStartTime;
			bHasValidCharge=false;
			const bool bChargeQualified = ChargedTime >= 0.8;
			bChargeQualifiedForRelease = bChargeQualified;
			if (bChargeQualified)
			{
				UE_LOG(LogTemp,Display,TEXT("Charge release: Avatar=%s Authority=%d ReleasedTime=%.3f ChargeQualified=%d"),*GetNameSafe(AvatarActor),bAuthority,ChargedTime,bChargeQualified);
			
			}
			else
			{
				UE_LOG(LogTemp,Display,TEXT("Charge release: Avatar=%s Authority=%d ReleasedTime=%.3f ChargeQualified=%d"),*GetNameSafe(AvatarActor),bAuthority,ChargedTime,bChargeQualified);
			}
		}
		else
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
			return;
		}
	}
	BeginReleaseTargetData();
}

void UAuraChargedProjectileSpell::BeginReleaseTargetData()
{
	const FGameplayAbilityActorInfo* ActorInfo=GetCurrentActorInfo();
	if (IsActive()&&ActorInfo)
	{
		UTargetDataUnderMouse *TargetDataUnderMouseTask= UTargetDataUnderMouse::CreateTargetDataUnderMouse(this);
		if (!TargetDataUnderMouseTask)
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
			return;
		}
		TargetDataUnderMouseTask->ValidData.AddDynamic(this,&UAuraChargedProjectileSpell::OnReleaseTargetData);
		TargetDataUnderMouseTask->ReadyForActivation();
	}
}

void UAuraChargedProjectileSpell::OnReleaseTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	const FGameplayAbilityActorInfo* ActorInfo=GetCurrentActorInfo();
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const FGameplayAbilityTargetData* TargetData=Data.Num()?Data.Get(0):nullptr;
	const FHitResult* HitResult =TargetData ? TargetData->GetHitResult() : nullptr;
	if (!IsActive()||!ActorInfo)return;
	const bool bAuthority=ActorInfo->IsNetAuthority();
	if (ActorInfo->IsNetAuthority())
	{
		if (!bChargeQualifiedForRelease)
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
			return;
		}
		
		if (!HitResult|| !HitResult->bBlockingHit||!FMath::IsFinite(HitResult->ImpactPoint.X)||!FMath::IsFinite(HitResult->ImpactPoint.Y)||!FMath::IsFinite(HitResult->ImpactPoint.Z))
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
			return;
		}
		
		const FVector TargetLocation= HitResult->ImpactPoint;
		UE_LOG(LogTemp,Display,TEXT("Charge Target: Avatar=%s Authority=%d TargetLocation=%s"),*GetNameSafe(AvatarActor),bAuthority,*TargetLocation.ToString());
		if (!IsValid(AvatarActor)||!IsValid(GetWorld()))
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
			return;
		}
		if (!ProjectileClass ||!DamageEffectClass ||!CostGameplayEffectClass ||!CooldownGameplayEffectClass)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Charge config invalid: Avatar=%s Projectile=%d Damage=%d Cost=%d Cooldown=%d"),
				*GetNameSafe(AvatarActor),
				ProjectileClass.Get() != nullptr,
				DamageEffectClass.Get() != nullptr,
				CostGameplayEffectClass.Get() != nullptr,
				CooldownGameplayEffectClass.Get() != nullptr);

			EndAbility(
				GetCurrentAbilitySpecHandle(),
				GetCurrentActorInfo(),
				GetCurrentActivationInfo(),
				true,
				true);
			return;
		}
		bChargeQualifiedForRelease = false;
		bool IsCommit= CommitAbility(GetCurrentAbilitySpecHandle(),ActorInfo,GetCurrentActivationInfo());
		UE_LOG(LogTemp,Display,TEXT("Charge Commit: Avatar=%s Authority=%d IsCommit=%d"),*GetNameSafe(AvatarActor),bAuthority,IsCommit);
		if (!IsCommit)
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
			return;
		}
		if (!IsActive())return;
		AAuraProjectile* Projectile= SpawnProjectileWithResult(TargetLocation,FAuraGameplayTags::Get().CombatSocket_Weapon);
		if (!IsValid(Projectile))
		{
			UE_LOG(LogTemp,Warning,TEXT("Charge spawn failed after Commit: Avatar=%s Authority=%d"),*GetNameSafe(GetAvatarActorFromActorInfo()),bAuthority);
			if (IsActive())
			{
				EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,true);
			}

			return;
		}
		Projectile->SetLifeSpan(2.0f);
		UE_LOG(LogTemp, Display, TEXT("Charge spawn: Avatar=%s Authority=%d Projectile=%s LifeSpan=%.1f"), *GetNameSafe(AvatarActor), bAuthority, *GetNameSafe(Projectile), Projectile->GetLifeSpan());

		if (IsActive())
		{
			EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),true,false);
		}
	}
}
