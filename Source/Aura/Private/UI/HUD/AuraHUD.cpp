// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/AuraHUD.h"

#include "Blueprint/UserWidget.h"
#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/SpellMenuWigdetController.h"
#include "UI/Widgets/AuraUserWidget.h"


UOverlayWidgetController* AAuraHUD::GetOverlayWidgetController(const FWidgetControllerParams& WcParams)
{
	if (OverlayWidgetController!=nullptr) return OverlayWidgetController;
	OverlayWidgetController=NewObject<UOverlayWidgetController>(this,OverlayWidgetControllerClass);
	OverlayWidgetController->SetWidgetControllerParams(WcParams);
	OverlayWidgetController->BindCallbacksToDependencies();
	
	return OverlayWidgetController;
}

UAttributeMenuWidgetController* AAuraHUD::GetAttributeMenuWidgetController(const FWidgetControllerParams& WcParams)
{
	if (AttributeMenuWidgetController!=nullptr) return AttributeMenuWidgetController;
	AttributeMenuWidgetController=NewObject<UAttributeMenuWidgetController>(this,AttributeMenuWidgetControllerClass);
	AttributeMenuWidgetController->SetWidgetControllerParams(WcParams);
	AttributeMenuWidgetController->BindCallbacksToDependencies();
	
	return AttributeMenuWidgetController;
}

USpellMenuWigdetController* AAuraHUD::GetSpellMenuWidgetController(const FWidgetControllerParams& WcParams)
{
	if (SpellMenuWidgetController!=nullptr) return SpellMenuWidgetController;
	SpellMenuWidgetController=NewObject<USpellMenuWigdetController>(this,SpellMenuWidgetControllerClass);
	SpellMenuWidgetController->SetWidgetControllerParams(WcParams);
	SpellMenuWidgetController->BindCallbacksToDependencies();

	return SpellMenuWidgetController;
}

void AAuraHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAttributeSet* AS, UAbilitySystemComponent* ASC)
{
	checkf(OverlayWidgetClass,TEXT("未选择Overlay"));
	checkf(OverlayWidgetControllerClass,TEXT("未选择OverlayController"));
	
	UUserWidget	* Widget=CreateWidget<UUserWidget>(GetWorld(),OverlayWidgetClass);
	//初始化赋值Widget
	OverlayWidget=Cast<UAuraUserWidget>(Widget);
	
	FWidgetControllerParams WCParams(PC,PS,ASC,AS);
	UOverlayWidgetController* WidgetController=GetOverlayWidgetController(WCParams);
	//Menu在蓝图中设置
	OverlayWidget->SetWidgetController(WidgetController);
	
	WidgetController->BroadcastInitialValues();
	
	Widget->AddToViewport();
	

}

void AAuraHUD::BeginPlay()
{
	Super::BeginPlay();
	

}
