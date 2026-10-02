// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/WidgetController/AuraWigdetController.h"
#include "AttributeMenuWidgetController.generated.h"

class UAttributeInfo;
struct FAuraAttributeInfo;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAttributeInfoSignature, const FAuraAttributeInfo&, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerPointsChangedSignature, int32, NewValue);

/**
 * 
 */
UCLASS(Blueprintable,BlueprintType)
class AURA_API UAttributeMenuWidgetController : public UAuraWigdetController
{
	GENERATED_BODY()
public:
	virtual void BroadcastInitialValues()override;
	
	virtual void BindCallbacksToDependencies()override;
	
	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FAttributeInfoSignature AttributeInfoDelegate;

	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FOnPlayerPointsChangedSignature OnAttributePointsChangedDelegate;

	// Kept for the existing Attribute Menu widget binding. Spell Menu has its own delegate,
	// but removing this reflected property would invalidate the current Attribute Menu asset.
	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FOnPlayerPointsChangedSignature OnSpellPointsChangedDelegate;

	UFUNCTION(BlueprintCallable)
	void UpgradeAttribute(const FGameplayTag& AttributeTag);

protected:
	void BroadcastAttributeInfo(const FGameplayTag& AttributeTag, float AttributeValue);
	void OnAttributePointsChanged(int32 NewValue);
	void OnSpellPointsChanged(int32 NewValue);
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAttributeInfo>AttributeInfo;

private:
	bool bCallbacksBound = false;
};
