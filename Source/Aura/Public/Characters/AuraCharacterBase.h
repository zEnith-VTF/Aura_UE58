// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Interfaction/CombatInterface.h"
#include "AuraCharacterBase.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UAttributeSet;
class UAuraAttributeSet;
class UAbilitySystemComponent;
class UAnimMontage;
class UMaterialInstance;
class UMaterialInstanceDynamic;
class UNiagaraSystem;

UCLASS(Abstract)
class AURA_API AAuraCharacterBase : public ACharacter,public IAbilitySystemInterface,public ICombatInterface
{
	GENERATED_BODY()

public:

	AAuraCharacterBase();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	FORCEINLINE UAttributeSet* GetAttributeSet() const{return AttributeSet;}
	virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;
	virtual UAnimMontage* GetHitReactMontage_Implementation() override;
	virtual void Die() override;
	virtual bool IsDead_Implementation() const override;
	virtual AActor* GetAvatar_Implementation() const override;
	virtual TArray<FTaggedMontage> GetAttackMontages_Implementation() override;
	virtual FTaggedMontage GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag) override;
	virtual UNiagaraSystem* GetBloodEffect_Implementation() override;
	virtual int32 GetMinionCount_Implementation() override;
	virtual void IncrementMinionCount_Implementation(int32 Amount) override;
	virtual ECharacterClass GetCharacterClass_Implementation() override;
	virtual USkeletalMeshComponent* GetWeapon_Implementation() override;
	virtual FOnDeathSignature& GetOnDeathDelegate() override;

	// 死亡委托：MulticastHandleDeath 时于所有端广播。
	FOnDeathSignature OnDeathDelegate;

	UFUNCTION(NetMulticast,Reliable)
	virtual void MulticastHandleDeath();
	
	virtual void BeginPlay() override;
protected:
	//======== GAS 初始化 ========
	virtual void InitAbilityActorInfo();
	virtual void InitializeDefaultAttributes()const;
	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level)const;
	void AddAbilityToCharacter();

	//======== 死亡溶解表现 ========
	//溶解效果
	void Dissolve();

	UFUNCTION(BlueprintImplementableEvent)
	void StartDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	UFUNCTION(BlueprintImplementableEvent)
	void StartWeaponDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	//======== 战斗插槽与蒙太奇 ========
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category=Combat)
	TObjectPtr<USkeletalMeshComponent>Weapon;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category=Combat)
	FName WeaponTipSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName RightHandSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName LeftHandSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName TailSocketName;

	UPROPERTY(BlueprintReadOnly,EditAnywhere,Category=Combat)
	TObjectPtr<UAnimMontage>HitReactMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TArray<FTaggedMontage> AttackMontages;

	// 每个角色蓝图都可以配置自己的受击血液 Niagara 效果。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Effects")
	TObjectPtr<UNiagaraSystem> BloodEffect;

	UPROPERTY(BlueprintReadOnly, Category="Combat")
	bool bDead = false;
	
	//======== GAS 核心与默认属性 ========
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent>AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet>AttributeSet;
	
	UPROPERTY(BlueprintReadOnly,EditAnywhere,Category="Attributes")
	TSubclassOf<UGameplayEffect>DefaultPrimaryAttributes;
	
	UPROPERTY(BlueprintReadOnly,EditAnywhere,Category="Attributes")
	TSubclassOf<UGameplayEffect>DefaultSecondaryAttributes;
	
	UPROPERTY(BlueprintReadOnly,EditAnywhere,Category="Attributes")
	TSubclassOf<UGameplayEffect>DefaultVitalAttributes;
	
	//======== 死亡溶解资产 ========
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TObjectPtr<UMaterialInstance>DissolveMaterialInstance;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TObjectPtr<UMaterialInstance>WeaponDissolveMaterialInstance;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	USoundBase *DeathSound;
	
	//======== 随从与职业 ========
	int32 MinionCount=0;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Character Class Defaults",meta=(AllowPrivateAccess="true"))
	ECharacterClass CharacterClass = ECharacterClass::Warrior;
private:
	UPROPERTY(EditAnywhere,Category="Abilities")
	TArray<TSubclassOf<UGameplayAbility>>StartUpAbilities;
	
	UPROPERTY(EditAnywhere,Category="Abilities")
	TArray<TSubclassOf<UGameplayAbility>>StartUpPassiveAbilities;
};
