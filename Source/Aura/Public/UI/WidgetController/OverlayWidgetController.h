// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/WidgetController/AuraWigdetController.h"
#include "OverlayWidgetController.generated.h"
class UAuraUserWidget;
struct FOnAttributeChangeData;

//拾取物品提示，包
USTRUCT(BlueprintType)
struct FUIWidgetRow: public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	FGameplayTag MessageTag=FGameplayTag();
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	FText Message=FText();
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	TSubclassOf<UAuraUserWidget>UserWidget;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly)
	UTexture2D* Image=nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStatChangedSignature,int32,NewValue);
//声明动态委托，包含一个参数
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeChangedSignature,float,Attribute);
//动态委托必须加参数名Row
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMessageWidgetRowSignature,FUIWidgetRow,Row);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnXPChangedSignature,float ,NewXP);
/**
 * @class UOverlayWidgetController
 * @brief Manages the interaction between game logic and UI elements for overlay widgets.
 *
 * This class provides functionality to broadcast attribute changes and manage UI updates for
 * health, mana, and game message widgets. It extends the base `UAuraWidgetController`
 * to include specific behavior for overlay widgets.
 */
UCLASS(BlueprintType, Blueprintable)
class AURA_API UOverlayWidgetController : public UAuraWigdetController
{
	GENERATED_BODY()
	
	
public:
	virtual void BroadcastInitialValues()override;
	virtual void BindCallbacksToDependencies()override; 
	
	//BlueprintAssignable 的作用是让蓝图可以看到这个委托，并且可以操作
	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FOnAttributeChangedSignature OnHealthChanged;
	
	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FOnAttributeChangedSignature OnMaxHealthChanged;
	
	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FOnAttributeChangedSignature OnManaChanged;
	
	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FOnAttributeChangedSignature OnMaxManaChanged;
	
	UPROPERTY(BlueprintAssignable,Category="Gas|Messages")
	FMessageWidgetRowSignature MessageWidgetRowDelegate;
	
	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FOnXPChangedSignature OnXPPercentChangedDelegate;
	
	UPROPERTY(BlueprintAssignable,Category="Gas|Attributes")
	FOnPlayerStatChangedSignature OnPlayerLevelChangedDelegate;
protected:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Widget Data")
	TObjectPtr<UDataTable>MessageWidgetTable;
	
	template<class T>
	T*GetDataTableRowByTag(UDataTable* DataTable,const FGameplayTag & Tag);
	
	void OnXPChanged(int32 NewXP);
	
	//技能装备后同步 HUD 技能栏：先清空旧槽位，再刷新新槽位
	void OnAbilityEquipped(const FGameplayTag& AbilityTag, const FGameplayTag& Status,
		const FGameplayTag& Slot, const FGameplayTag& PreviousSlot);
};

template <class T>
T* UOverlayWidgetController::GetDataTableRowByTag(UDataTable* DataTable, const FGameplayTag& Tag)
{
	return DataTable->FindRow<T>(Tag.GetTagName(),TEXT(""));
}
