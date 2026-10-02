// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AuraWigdetController.generated.h"

class UAttributeSet;
class UAbilitySystemComponent;
class AAuraPlayerController;
class AAuraPlayerState;
class UAuraAbilitySystemComponent;
class UAuraAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAbilityInfoSignature,const FAuraAbilityInfo&,Info);


USTRUCT(BlueprintType)
struct FWidgetControllerParams
{
	GENERATED_BODY()
	
	FWidgetControllerParams(){};
	FWidgetControllerParams(APlayerController* PC,APlayerState*PS,UAbilitySystemComponent*ASC,UAttributeSet*AS)
		:PC(PC),PS(PS),ASC(ASC),AS(AS){}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TObjectPtr<APlayerController>PC=nullptr;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TObjectPtr<APlayerState>PS=nullptr;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TObjectPtr<UAbilitySystemComponent>ASC=nullptr;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TObjectPtr<UAttributeSet>AS=nullptr;

};

/**
 * 
 */
UCLASS()
class AURA_API UAuraWigdetController : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void SetWidgetControllerParams(const FWidgetControllerParams& WCParams );
	
	UFUNCTION(BlueprintCallable)
	virtual void BroadcastInitialValues();
	
	virtual void BindCallbacksToDependencies();
	
	UPROPERTY(BlueprintAssignable,Category="Gas|Messages")
	FAbilityInfoSignature AbilityInfoDelegate;
protected:
	void BroadcastAbilityInfo();
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Widget Data")
	TObjectPtr<UAbilityInfo>AbilityInfo;
	
	AAuraPlayerController* GetAuraPC();
	AAuraPlayerState* GetAuraPS();
	UAuraAbilitySystemComponent* GetAuraASC();
	UAuraAttributeSet* GetAuraAS();
	
	UPROPERTY(BlueprintReadOnly,Category="Widget Controller")
	TObjectPtr<APlayerController>PlayerController;
	
	UPROPERTY(BlueprintReadOnly,Category="Widget Controller")
	TObjectPtr<UAbilitySystemComponent>AbilitySystemComponent;
	
	UPROPERTY(BlueprintReadOnly,Category="Widget Controller")
	TObjectPtr<UAttributeSet>AttributeSet;
	
	UPROPERTY(BlueprintReadOnly,Category="Widget Controller")
	TObjectPtr<APlayerState>PlayerState;
	
	UPROPERTY(BlueprintReadOnly,Category="Widget Controller")
	TObjectPtr<AAuraPlayerController>AuraPlayerController;
	
	UPROPERTY(BlueprintReadOnly,Category="Widget Controller")
	TObjectPtr<UAuraAbilitySystemComponent>AuraAbilitySystemComponent;
	
	UPROPERTY(BlueprintReadOnly,Category="Widget Controller")
	TObjectPtr<UAuraAttributeSet>AuraAttributeSet;
	
	UPROPERTY(BlueprintReadOnly,Category="Widget Controller")
	TObjectPtr<AAuraPlayerState>AuraPlayerState;

};
