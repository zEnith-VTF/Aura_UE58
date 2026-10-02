


#include "Actor/AuraEffectActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"


AAuraEffectActor::AAuraEffectActor()
{
 	
	PrimaryActorTick.bCanEverTick = false;
	
	SetRootComponent(CreateDefaultSubobject<USceneComponent>("RootComponent"));
}



void AAuraEffectActor::BeginPlay()
{
	Super::BeginPlay();

}

void AAuraEffectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (auto HandlePair : ActiveEffectHandles)
	{
		if (IsValid(HandlePair.Value))
		{
			HandlePair.Value->RemoveActiveGameplayEffect(HandlePair.Key, 1);
		}
	}

	ActiveEffectHandles.Empty();
	ActiveOverlapCounts.Empty();

	Super::EndPlay(EndPlayReason);
}

void AAuraEffectActor::ApplyEffectToTarget(AActor* Actor, TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	if (!IsValid(Actor)) return;

	static const FName EnemyTag = TEXT("Enemy");
	if (Actor->ActorHasTag(EnemyTag) && !bApplyEffectToEnemies) return;

	//第一种方法：重写接口实现方法
	// IAbilitySystemInterface * ASCInterface=Cast<IAbilitySystemInterface>(Actor);
	// if (ASCInterface==nullptr)return;
	// ASCInterface->GetAbilitySystemComponent();
	
	//第二种：运用库函数
	
	
	UAbilitySystemComponent* TargetASC= UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (TargetASC==nullptr)return;
	
	if (GameplayEffectClass == nullptr) return;
	//创建 GameplayEffect 上下文
	FGameplayEffectContextHandle EffectContextHandle= TargetASC->MakeEffectContext();
	//把当前这个对象作为来源对象
	EffectContextHandle.AddSourceObject(this);
	//创建本次 GameplayEffect 的 SpecHandle，Handle是Spec的包装盒，Spec不适合直接生成
	FGameplayEffectSpecHandle EffectSpecHandle=TargetASC->MakeOutgoingSpec(GameplayEffectClass,ActorLevel,EffectContextHandle);
	if (!EffectSpecHandle.IsValid()) return;
	//应用到目标自己身上,Data是智能指针，不能直接使用，通过Get()转换为普通指针
	FActiveGameplayEffectHandle ActiveGameplayEffectHandle=TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
	
	const bool bIsInfinite=EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy==EGameplayEffectDurationType::Infinite;
	if (bIsInfinite&&InfiniteEffectRemovalPolicy==EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		ActiveEffectHandles.Add(ActiveGameplayEffectHandle,TargetASC);
	}

	if (!bIsInfinite)
	{
		Destroy();
	}
}

void AAuraEffectActor::OnBeginOverlap(AActor* TargetActor)
{
	if (!IsValid(TargetActor)) return;

	static const FName EnemyTag = TEXT("Enemy");
	if (TargetActor->ActorHasTag(EnemyTag) && !bApplyEffectToEnemies) return;

	if (!BeginTargetOverlap(TargetActor))
	{
		return;
	}

	if (InstantEffectApplicationPolicy==EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor,InstantGameplayEffectClass);
	}
	if (DurationEffectApplicationPolicy==EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor,DurationGameplayEffectClass);
	}
	if (InfiniteEffectApplicationPolicy==EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor,InfiniteGameplayEffectClass);
	}
	
}

void AAuraEffectActor::OnEndOverlap(AActor* TargetActor)
{
	if (!IsValid(TargetActor)) return;

	static const FName EnemyTag = TEXT("Enemy");
	if (TargetActor->ActorHasTag(EnemyTag) && !bApplyEffectToEnemies) return;

	if (!EndTargetOverlap(TargetActor))
	{
		return;
	}

	if (InstantEffectApplicationPolicy==EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor,InstantGameplayEffectClass);
	}
	if (DurationEffectApplicationPolicy==EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor,DurationGameplayEffectClass);
	}
	if (InfiniteEffectApplicationPolicy==EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor,InfiniteGameplayEffectClass);
	}
	if (InfiniteEffectRemovalPolicy==EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		UAbilitySystemComponent *TargetASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (TargetASC==nullptr)return;
		
		TArray<FActiveGameplayEffectHandle> HandlesToRemove;
		for (auto HandlePair:ActiveEffectHandles)
		{
			
			if (TargetASC==HandlePair.Value)
			{
				TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
				HandlesToRemove.Add(HandlePair.Key);
			}
		}
		for (auto Handle:HandlesToRemove)
		{
			ActiveEffectHandles.FindAndRemoveChecked(Handle);
		}
		
	}
}

bool AAuraEffectActor::BeginTargetOverlap(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor)==nullptr)
	{
		return false;
	}

	const TWeakObjectPtr<AActor> TargetKey(TargetActor);
	int32& OverlapCount = ActiveOverlapCounts.FindOrAdd(TargetKey);
	++OverlapCount;

	return OverlapCount == 1;
}

bool AAuraEffectActor::EndTargetOverlap(AActor* TargetActor)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const TWeakObjectPtr<AActor> TargetKey(TargetActor);
	int32* OverlapCount = ActiveOverlapCounts.Find(TargetKey);
	if (OverlapCount == nullptr)
	{
		return false;
	}

	--(*OverlapCount);
	if (*OverlapCount > 0)
	{
		return false;
	}

	ActiveOverlapCounts.Remove(TargetKey);
	return true;
}




