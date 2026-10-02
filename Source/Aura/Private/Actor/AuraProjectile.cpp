


#include "Actor/AuraProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "Aura/Aura.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interfaction/CombatInterface.h"
#include "Kismet/GameplayStatics.h"


AAuraProjectile::AAuraProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	SetRootComponent(Sphere);
	Sphere->SetCollisionObjectType(ECC_Projectile);
	Sphere->InitSphereRadius(15.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	
	Audio = CreateDefaultSubobject<UAudioComponent>("LoopingSound");
	Audio->SetupAttachment(Sphere);
	Audio->bAutoActivate = false;
	
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
	ProjectileMovement->InitialSpeed = 550.f;
	ProjectileMovement->MaxSpeed = 550.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void AAuraProjectile::Destroyed()
{
	if (!bHit&&!HasAuthority())
	{
		UGameplayStatics::PlaySoundAtLocation(this,ImpactSound,GetActorLocation(),FRotator::ZeroRotator);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,ImpactEffect,GetActorLocation());
	}
	Super::Destroyed();
	
}

void AAuraProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(LifeSpan);
	
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AAuraProjectile::OnSphereOverlap);
	
	if (Audio&&LoopingSound)
	{
		Audio->SetSound(LoopingSound);
		Audio->Play();
	}
}

void AAuraProjectile::OnSphereOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bHit || OtherActor == nullptr || OtherActor == this || OtherActor == GetInstigator())
	{
		return;
	}

	// 同阵营角色不承受伤害，也不会挡住彼此的投射物。
	if (UAuraAbilitySystemLibrary::AreActorsFriends(GetInstigator(), OtherActor))
	{
		return;
	}

	// 一个投射物可能同时与角色的 Capsule、Mesh 等多个组件重叠。
	// 首次确认命中后立即锁定，避免同一网络实例重复播放表现或应用伤害。
	bHit = true;

	// 只有实现战斗接口的角色才拥有血液效果；墙体、场景物体不会生成血液。
	if (OtherActor->Implements<UCombatInterface>())
	{
		if (UNiagaraSystem* BloodEffect = ICombatInterface::Execute_GetBloodEffect(OtherActor))
		{
			// SweepResult 有效时使用真实接触点和表面法线；否则回退到投射物位置。
			const FVector BloodLocation = bFromSweep && !SweepResult.ImpactPoint.IsNearlyZero()
				? FVector(SweepResult.ImpactPoint)
				: GetActorLocation();
			const FRotator BloodRotation = bFromSweep && !SweepResult.ImpactNormal.IsNearlyZero()
				? SweepResult.ImpactNormal.Rotation()
				: GetActorRotation();

			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this,
				BloodEffect,
				BloodLocation,
				BloodRotation);
		}
	}
	
	if (Audio)
	{
		Audio->FadeOut(0.1f, 0.f);
	}
	
	UGameplayStatics::PlaySoundAtLocation(this,ImpactSound,GetActorLocation(),FRotator::ZeroRotator);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,ImpactEffect,GetActorLocation());
	
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* TargetASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
		{
			DamageEffectParams.TargetAbilitySystemComponent=TargetASC;
			UAuraAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);	
		}
		
		Destroy();
	}
}
