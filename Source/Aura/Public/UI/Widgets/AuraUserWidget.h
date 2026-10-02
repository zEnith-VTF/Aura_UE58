 // Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AuraUserWidget.generated.h"

class UAuraWigdetController;
/**
 * 
 */
UCLASS()
class AURA_API UAuraUserWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void SetWidgetController(UObject* Controller);
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject>WidgetController;
	
	
protected:
	//只在Cpp里面声明，不在里面实现
	UFUNCTION(BlueprintImplementableEvent)
	void WidgetControllerSet();
	

};
