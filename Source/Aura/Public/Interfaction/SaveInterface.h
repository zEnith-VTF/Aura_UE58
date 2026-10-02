// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SaveInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType)
class USaveInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class AURA_API ISaveInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	// 读档时是否恢复该 Actor 的 Transform；关卡里固定摆放的对象通常返回 false。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool ShouldLoadTransform();

	// 读档时恢复该 Actor 自身的状态（例如检查点已点亮时要立刻显示发光）。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void LoadActor();
};
