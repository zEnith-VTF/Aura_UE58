// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "AuraPlayerState.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class ULevelUpInfo;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerStatChange,int32)
/**
 * 
 */
UCLASS()
class AURA_API AAuraPlayerState : public APlayerState,public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	//======== 生命周期与 GAS 访问 ========
	AAuraPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	FORCEINLINE UAttributeSet* GetAttributeSet() const{return AttributeSet;}

	//======== 等级 ========
	FORCEINLINE int32 GetPlayerLevel()const{return Level;}
	void AddToLevel(int32 InLevel);
	void SetLevel(int32 InLevel);

	//======== 经验 ========
	FORCEINLINE int32 GetXP() const { return XP; }
	void AddToXP(int32 InXP);
	void SetXP(int32 InXP);

	//======== 属性点 ========
	FORCEINLINE int32 GetAttributePoints() const { return AttributePoints; }
	void AddToAttributePoints(int32 InAttributePoints);
	void SetAttributePoints(int32 InAttributePoints);

	//======== 技能点 ========
	FORCEINLINE int32 GetSpellPoints() const { return SpellPoints; }
	void AddToSpellPoints(int32 InSpellPoints);
	void SetSpellPoints(int32 InSpellPoints);

	//======== 配置与 GAS 组件 ========
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<ULevelUpInfo> LevelUpInfo;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent>AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet>AttributeSet;
	
	//======== 变更广播委托 ========
	FOnPlayerStatChange OnXPChangedDelegate;
	FOnPlayerStatChange OnLevelChangedDelegate;
	FOnPlayerStatChange OnAttributePointsChangedDelegate;
	FOnPlayerStatChange OnSpellPointsChangedDelegate;
private:
	//======== 复制状态 ========
	UPROPERTY(VisibleAnywhere,ReplicatedUsing=OnRep_Level)
	int32 Level=1;
	
	UPROPERTY(VisibleAnywhere,ReplicatedUsing=OnRep_XP)
	int32 XP=0;
	
	UPROPERTY(VisibleAnywhere,ReplicatedUsing=OnRep_AttributePoints)
	int32 AttributePoints=0;
	
	UPROPERTY(VisibleAnywhere,ReplicatedUsing=OnRep_SpellPoints)
	int32 SpellPoints=0;
	
	UFUNCTION()
	void OnRep_Level(int32 OldLevel);
	
	UFUNCTION()
	void OnRep_XP(int32 OldXP);
	
	UFUNCTION()
	void OnRep_AttributePoints(int32 OldAttributePoints);
	
	UFUNCTION()
	void OnRep_SpellPoints(int32 OldSpellPoints);
};
