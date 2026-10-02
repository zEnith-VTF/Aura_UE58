// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AuraHUD.generated.h"

class UAttributeMenuWidgetController;
class USpellMenuWigdetController;
class UAbilitySystemComponent;
class UAttributeSet;
struct FWidgetControllerParams;
class UOverlayWidgetController;
class UAuraUserWidget;
/**
 * 
 */
UCLASS()
class AURA_API AAuraHUD : public AHUD
{
	GENERATED_BODY()
public:
	
	
	UFUNCTION()
	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams&WcParams);
	
	UFUNCTION()
	UAttributeMenuWidgetController* GetAttributeMenuWidgetController(const FWidgetControllerParams&WcParams);

	UFUNCTION()
	USpellMenuWigdetController* GetSpellMenuWidgetController(const FWidgetControllerParams&WcParams);
	
	void InitOverlay(APlayerController* PC,APlayerState *PS,UAttributeSet *AS,UAbilitySystemComponent *ASC);
protected:
	virtual void BeginPlay() override;	
	
	
private:
	UPROPERTY()
	TObjectPtr<UAuraUserWidget>OverlayWidget;
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<UAuraUserWidget>OverlayWidgetClass;

	UPROPERTY()
	TObjectPtr<UOverlayWidgetController>OverlayWidgetController;
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController>OverlayWidgetControllerClass;
	

	
	UPROPERTY()
	TObjectPtr<UAttributeMenuWidgetController>AttributeMenuWidgetController;
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<UAttributeMenuWidgetController>AttributeMenuWidgetControllerClass;

	UPROPERTY()
	TObjectPtr<USpellMenuWigdetController>SpellMenuWidgetController;

	UPROPERTY(EditAnywhere)
	TSubclassOf<USpellMenuWigdetController>SpellMenuWidgetControllerClass;

	
};
