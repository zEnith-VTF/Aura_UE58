// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/AuraCharacterBase.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Aura/Aura.h"
#include "GameplayEffect.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tags/AuraGameplayTags.h"


AAuraCharacterBase::AAuraCharacterBase()
{
 	
	PrimaryActorTick.bCanEverTick = false;
	
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Projectile,ECR_Overlap);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_EQSLineOfSight,ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile,ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_EQSLineOfSight,ECR_Ignore);
	
	//此处不使用TEXT是因为需要FName,而FString需要FText
	Weapon=CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(),FName("WeaponHandSocket"));
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Weapon->SetCollisionResponseToChannel(ECC_EQSLineOfSight,ECR_Ignore);
}

UAbilitySystemComponent* AAuraCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FVector AAuraCharacterBase::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Weapon))
	{
		if (IsValid(Weapon) && Weapon->DoesSocketExist(WeaponTipSocketName))
		{
			return Weapon->GetSocketLocation(WeaponTipSocketName);
		}

		UE_LOG(LogTemp, Error,
			TEXT("%s is missing weapon socket [%s]."),
			*GetNameSafe(this),
			*WeaponTipSocketName.ToString());
		return GetActorLocation();
	}

	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_RightHand))
	{
		if (GetMesh() && GetMesh()->DoesSocketExist(RightHandSocketName))
		{
			return GetMesh()->GetSocketLocation(RightHandSocketName);
		}

		UE_LOG(LogTemp, Error,
			TEXT("%s is missing right-hand socket [%s]."),
			*GetNameSafe(this),
			*RightHandSocketName.ToString());
		return GetActorLocation();
	}

	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_LeftHand))
	{
		if (GetMesh() && GetMesh()->DoesSocketExist(LeftHandSocketName))
		{
			return GetMesh()->GetSocketLocation(LeftHandSocketName);
		}

		UE_LOG(LogTemp, Error,
			TEXT("%s is missing left-hand socket [%s]."),
			*GetNameSafe(this),
			*LeftHandSocketName.ToString());
		return GetActorLocation();
	}

	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Tail))
	{
		if (GetMesh() && GetMesh()->DoesSocketExist(TailSocketName))
		{
			return GetMesh()->GetSocketLocation(TailSocketName);
		}

		UE_LOG(LogTemp, Error,
			TEXT("%s is missing tail socket [%s]."),
			*GetNameSafe(this),
			*TailSocketName.ToString());
		return GetActorLocation();
	}

	UE_LOG(LogTemp, Error,
		TEXT("%s received unsupported combat socket tag [%s]."),
		*GetNameSafe(this),
		*MontageTag.ToString());
	return GetActorLocation();
}

bool AAuraCharacterBase::IsDead_Implementation() const
{
	return bDead;
}

AActor* AAuraCharacterBase::GetAvatar_Implementation() const
{
	return const_cast<AAuraCharacterBase*>(this);
}

TArray<FTaggedMontage> AAuraCharacterBase::GetAttackMontages_Implementation()
{
	return AttackMontages;
}

FTaggedMontage AAuraCharacterBase::GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag)
{
	if (!MontageTag.IsValid())
	{
		return FTaggedMontage();
	}

	for (const FTaggedMontage& TaggedMontage : AttackMontages)
	{
		if (TaggedMontage.MontageTag.MatchesTagExact(MontageTag))
		{
			return TaggedMontage;
		}
	}

	return FTaggedMontage();
}

UNiagaraSystem* AAuraCharacterBase::GetBloodEffect_Implementation()
{
	return BloodEffect;
}

int32 AAuraCharacterBase::GetMinionCount_Implementation()
{
	return MinionCount;
}

void AAuraCharacterBase::IncrementMinionCount_Implementation(const int32 Amount)
{
	MinionCount += Amount;
}

ECharacterClass AAuraCharacterBase::GetCharacterClass_Implementation()
{
	return CharacterClass;
}

USkeletalMeshComponent* AAuraCharacterBase::GetWeapon_Implementation()
{
	return Weapon;
}

FOnDeathSignature& AAuraCharacterBase::GetOnDeathDelegate()
{
	return OnDeathDelegate;
}

UAnimMontage* AAuraCharacterBase::GetHitReactMontage_Implementation()
{
	return HitReactMontage;
}

void AAuraCharacterBase::Die()
{
	if (bDead) return;

	if (UAbilitySystemComponent* const ASC = GetAbilitySystemComponent())
	{
		if (ASC->IsOwnerActorAuthoritative())
		{
			FGameplayTagContainer BurnTags;
			BurnTags.AddTag(FAuraGameplayTags::Get().Debuff_Burn);
			ASC->RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(BurnTags));
		}
	}

	//将武器脱离
	Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
	MulticastHandleDeath();
}

void AAuraCharacterBase::MulticastHandleDeath_Implementation()
{
	bDead = true;

	UGameplayStatics::PlaySoundAtLocation(this,DeathSound,GetActorLocation(),GetActorRotation());
	//进入布娃娃状态
	if (Weapon)
	{
		Weapon->SetSimulatePhysics(true);
		Weapon->SetEnableGravity(true);
		Weapon->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	}

	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetEnableGravity(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dissolve();

	// 光束类技能等订阅者在此收到死亡通知（服务器与所有客户端都会执行）。
	OnDeathDelegate.Broadcast(this);
}

void AAuraCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// 蓝图子类可能保存过旧碰撞响应，运行时再次确保角色不会阻挡 EQS 视线检测。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_EQSLineOfSight,ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_EQSLineOfSight,ECR_Ignore);
	if (Weapon)
	{
		Weapon->SetCollisionResponseToChannel(ECC_EQSLineOfSight,ECR_Ignore);
	}
}

void AAuraCharacterBase::InitAbilityActorInfo()
{
	
}

void AAuraCharacterBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);
	FGameplayEffectContextHandle ContextHandle=GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle=GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass,Level,ContextHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(),GetAbilitySystemComponent());
}

void AAuraCharacterBase::InitializeDefaultAttributes() const
{
	ApplyEffectToSelf(DefaultPrimaryAttributes,1.f);
	ApplyEffectToSelf(DefaultSecondaryAttributes,1.f);
	ApplyEffectToSelf(DefaultVitalAttributes,1.f);
}

void AAuraCharacterBase::AddAbilityToCharacter()
{
	if (!HasAuthority()) return;
	UAuraAbilitySystemComponent* AuraASC=Cast<UAuraAbilitySystemComponent>(GetAbilitySystemComponent());

	AuraASC->AddAbilityToCharacter(StartUpAbilities);
	AuraASC->AddPassiveAbilityToCharacter(StartUpPassiveAbilities);
}

void AAuraCharacterBase::Dissolve()
{
	if (IsValid(DissolveMaterialInstance))
	{
		//创建动态材质
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicMatInst);
		StartDissolveTimeline(DynamicMatInst);
	}

	if (IsValid(WeaponDissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(WeaponDissolveMaterialInstance, this);
		Weapon->SetMaterial(0, DynamicMatInst);
		StartWeaponDissolveTimeline(DynamicMatInst);
	}
}




