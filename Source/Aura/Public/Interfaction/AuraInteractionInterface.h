// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AuraInteractionInterface.generated.h"

/** 一次交互请求的可靠近落点、交互半径与是否允许自动靠近。 */
USTRUCT(BlueprintType)
struct FAuraInteractionInfo
{
	GENERATED_BODY()

	/** 实现方当前是否愿意接受交互；为 false 时控制器不发起请求，走普通点击移动。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCanInteract = false;

	/** 玩家靠近时使用的站位，不是鼠标命中点。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FVector ApproachLocation = FVector::ZeroVector;

	/** 判定“已到达交互距离”的半径，单位厘米。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float InteractionRange = 150.f;

	/** 为假时超出范围直接消费本次点击，不自动靠近。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bAllowApproach = true;
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType)
class UAuraInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 可交互对象：先查询是否可交互与站位，再由玩家控制器发起服务器交互请求。
 * 查询无副作用；TryInteract 只在服务器被调用，交互状态与开门等实际效果都在这里改。
 */
class AURA_API IAuraInteractionInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/** 只读查询：填写站位、半径与是否允许自动靠近。无副作用，客户端会调用它读取站位。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool QueryInteraction(APawn* Interactor, UPARAM(ref) FAuraInteractionInfo& OutInfo);

	/** 再次检查条件并接受一次交互请求。只在服务器调用；返回 true 只表示本次请求被接受，不表示动画或流程已结束。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool TryInteract(APawn* Interactor);
};
