// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "AttributeInfo.generated.h"

USTRUCT(BlueprintType)
struct FAuraAttributeInfo
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly)
	FGameplayTag AttributeTag=FGameplayTag();
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly)
	FText AttributeName;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly)
	FText AttributeDescription;
	
	UPROPERTY(BlueprintReadOnly)
	float AttributeValue=0.f;
};
/**
* @brief 表示一种数据资产，用于存储和管理属性元数据或配置信息。 
*
* UAttributeInfo 是一个数据驱动型类，充当属性相关信息的容器，
* 从而实现在框架内部对属性数据进行高效的访问与操作。 
* 该类通常用于定义那些可在多个对象或实例之间共享、复用的属性或配置项。 
*/
UCLASS()
class AURA_API UAttributeInfo : public UDataAsset
{
	GENERATED_BODY()
public:
	
	FAuraAttributeInfo FindAttributeInfo(const FGameplayTag& AttributeTag,bool bNotFound=false)const;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly)
	TArray<FAuraAttributeInfo> AttributeInformation;
};
