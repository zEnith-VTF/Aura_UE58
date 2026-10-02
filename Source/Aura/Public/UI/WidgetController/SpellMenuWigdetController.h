// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tags/AuraGameplayTags.h"
#include "UI/WidgetController/AuraWigdetController.h"
#include "SpellMenuWigdetController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpellPointsChangedSignature, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FSpellGlobeSelectedSignature,
	bool, bSpendPointsButtonEnabled,
	bool, bEquipButtonEnabled, FString, OutDescription, FString, OutNextLevelDescription);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipButtonPressedSignature,const FGameplayTag&, AbilityTypeTag);

struct FSelectedAbility
{
	FGameplayTag Ability = FGameplayTag();
	FGameplayTag Status = FGameplayTag();
};

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class AURA_API USpellMenuWigdetController : public UAuraWigdetController
{
	GENERATED_BODY()
public: 
	virtual void BroadcastInitialValues()override;
	virtual void BindCallbacksToDependencies()override;

	//======== 蓝图调用入口 ========
	UFUNCTION(BlueprintCallable)
	void SpellGlobeSelected(const FGameplayTag& AbilityTag);

	UFUNCTION(BlueprintCallable)
	void GlobeDeselect();

	UFUNCTION(BlueprintCallable)
	void SpendPointButtonPressed();

	UFUNCTION(BlueprintCallable)
	void EquipButtonPressed();

	UFUNCTION(BlueprintCallable)
	void SpellRowGlobePressed(const FGameplayTag& SlotTag, const FGameplayTag& AbilityType);
	
	//======== 广播给 UI 的委托 ========
	UPROPERTY(BlueprintAssignable, Category="Gas|SpellMenu")
	FOnEquipButtonPressedSignature OnEquipButtonPressedSignature;
	
	UPROPERTY(BlueprintAssignable, Category="Gas|SpellMenu")
	FOnEquipButtonPressedSignature StopEquipButtonPressedSignature;
	
	UPROPERTY(BlueprintAssignable, Category="Gas|SpellMenu")
	FOnSpellPointsChangedSignature OnSpellPointsChangedDelegate;

	UPROPERTY(BlueprintAssignable, Category="Gas|SpellMenu")
	FSpellGlobeSelectedSignature SpellGlobeSelectedDelegate;
private:
	//======== 内部辅助 ========
	static void ShouldEnableButtons(const FGameplayTag& AbilityStatus, int32 SpellPoints,
		bool& bShouldEnableSpellPointsButton, bool& bShouldEnableEquipButton);
	void OnAbilityEquipped(const FGameplayTag& AbilityTag, const FGameplayTag& Status,
		const FGameplayTag& Slot, const FGameplayTag& PreviousSlot);

	//======== 选中状态 ========
	bool bCallbacksBound = false;
	FSelectedAbility SelectedAbility{
		FAuraGameplayTags::Get().Abilities_None,
		FAuraGameplayTags::Get().Abilities_Status_Locked};
	int32 CurrentSpellPoints = 0;
	bool bWaitForEquipSelection=false;
	FGameplayTag SelectedSlot;
};
