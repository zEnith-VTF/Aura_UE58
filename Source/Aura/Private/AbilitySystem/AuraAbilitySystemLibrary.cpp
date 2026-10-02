// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AuraAbilitySystemLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AuraAbilityTypes.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "GameMode/AURAGameModeBase.h"
#include "GameMode/LoadScreenSaveGame.h"
#include "Input/AuraPlayerState.h"
#include "Interfaction/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Tags/AuraGameplayTags.h"
#include "UI/HUD/AuraHUD.h"
#include "UI/WidgetController/AuraWigdetController.h"

namespace
{
	const FAuraGameplayEffectContext* GetAuraEffectContext(
		const FGameplayEffectContextHandle& EffectContextHandle)
	{
		const FGameplayEffectContext* Context = EffectContextHandle.Get();
		const UScriptStruct* ScriptStruct = Context ? Context->GetScriptStruct() : nullptr;
		if (!ScriptStruct || !ScriptStruct->IsChildOf(FAuraGameplayEffectContext::StaticStruct()))
		{
			return nullptr;
		}

		return static_cast<const FAuraGameplayEffectContext*>(Context);
	}

	FAuraGameplayEffectContext* GetAuraEffectContext(FGameplayEffectContextHandle& EffectContextHandle)
	{
		FGameplayEffectContext* Context = EffectContextHandle.Get();
		const UScriptStruct* ScriptStruct = Context ? Context->GetScriptStruct() : nullptr;
		if (!ScriptStruct || !ScriptStruct->IsChildOf(FAuraGameplayEffectContext::StaticStruct()))
		{
			return nullptr;
		}

		return static_cast<FAuraGameplayEffectContext*>(Context);
	}
}

UOverlayWidgetController* UAuraAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	FWidgetControllerParams WCParams;
	AAuraHUD* AuraHUD=nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, AuraHUD))
	{
		return AuraHUD->GetOverlayWidgetController(WCParams);
	}
	return nullptr;
}

UAttributeMenuWidgetController* UAuraAbilitySystemLibrary::GetAttributeMenuWidgetController(
	const UObject* WorldContextObject)
{
	FWidgetControllerParams WCParams;
	AAuraHUD* AuraHUD=nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, AuraHUD))
	{
		return AuraHUD->GetAttributeMenuWidgetController(WCParams);
	}
	return nullptr;

}

USpellMenuWigdetController* UAuraAbilitySystemLibrary::GetSpellMenuWidgetController(const UObject* WorldContextObject)
{
	FWidgetControllerParams WCParams;
	AAuraHUD* AuraHUD=nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, AuraHUD))
	{
		return AuraHUD->GetSpellMenuWidgetController(WCParams);
	}
	return nullptr;
}

bool UAuraAbilitySystemLibrary::MakeWidgetControllerParams(const UObject* WorldContextObject,
	FWidgetControllerParams& OutWCParams, AAuraHUD*& OutAuraHUD)
{
	if (APlayerController* PC=UGameplayStatics::GetPlayerController(WorldContextObject,0))
	{
		OutAuraHUD=Cast<AAuraHUD>(PC->GetHUD());
		if (OutAuraHUD)
		{
			AAuraPlayerState* PS=PC->GetPlayerState<AAuraPlayerState>();
			UAbilitySystemComponent* ASC=PS->GetAbilitySystemComponent();
			UAttributeSet* AS=PS->GetAttributeSet();

			OutWCParams.AS=AS;
			OutWCParams.ASC=ASC;
			OutWCParams.PS=PS;
			OutWCParams.PC=PC;
			return true;
		}
	}
	return false;
}

void UAuraAbilitySystemLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject,
	ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
	if (!ensureMsgf(IsValid(ASC), TEXT("InitializeDefaultAttributes requires a valid ASC"))) return;

	AActor* const OwnerActor = ASC->GetOwnerActor();
	AActor* const AvatarActor = ASC->GetAvatarActor();
	if (!ensureMsgf(IsValid(OwnerActor), TEXT("InitializeDefaultAttributes ASC has no valid owner")) ||
		!ensureMsgf(IsValid(AvatarActor), TEXT("InitializeDefaultAttributes ASC has no valid avatar")))
	{
		return;
	}
	if (!OwnerActor->HasAuthority()) return;

	UCharacterClassInfo* const CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (!ensureMsgf(IsValid(CharacterClassInfo), TEXT("CharacterClassInfo is not configured on Aura GameMode")))
	{
		return;
	}

	const FCharacterClassDefaultInfo* const ClassDefaultInfo =
		CharacterClassInfo->CharacterClassInformation.Find(CharacterClass);
	if (!ensureMsgf(ClassDefaultInfo,
		TEXT("CharacterClassInfo has no defaults for character class [%d]"), static_cast<int32>(CharacterClass)))
	{
		return;
	}

	const bool bHasPrimaryAttributes = ensureMsgf(ClassDefaultInfo->PrimaryAttributes,
		TEXT("PrimaryAttributes is not configured for character class [%d]"), static_cast<int32>(CharacterClass));
	const bool bHasSecondaryAttributes = ensureMsgf(CharacterClassInfo->SecondaryAttributes,
		TEXT("SecondaryAttributes is not configured on CharacterClassInfo"));
	const bool bHasVitalAttributes = ensureMsgf(CharacterClassInfo->VitalAttributes,
		TEXT("VitalAttributes is not configured on CharacterClassInfo"));
	if (!bHasPrimaryAttributes || !bHasSecondaryAttributes || !bHasVitalAttributes)
	{
		return;
	}

	FGameplayEffectContextHandle PrimaryAttributesContextHandle = ASC->MakeEffectContext();
	PrimaryAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle PrimaryAttributesSpecHandle =
		ASC->MakeOutgoingSpec(ClassDefaultInfo->PrimaryAttributes, Level, PrimaryAttributesContextHandle);

	FGameplayEffectContextHandle SecondaryAttributesContextHandle = ASC->MakeEffectContext();
	SecondaryAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle SecondaryAttributesSpecHandle =
		ASC->MakeOutgoingSpec(CharacterClassInfo->SecondaryAttributes, Level, SecondaryAttributesContextHandle);

	FGameplayEffectContextHandle VitalAttributesContextHandle = ASC->MakeEffectContext();
	VitalAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle VitalAttributesSpecHandle =
		ASC->MakeOutgoingSpec(CharacterClassInfo->VitalAttributes, Level, VitalAttributesContextHandle);

	if (!ensureMsgf(PrimaryAttributesSpecHandle.IsValid(), TEXT("Failed to create PrimaryAttributes spec")) ||
		!ensureMsgf(SecondaryAttributesSpecHandle.IsValid(), TEXT("Failed to create SecondaryAttributes spec")) ||
		!ensureMsgf(VitalAttributesSpecHandle.IsValid(), TEXT("Failed to create VitalAttributes spec")))
	{
		return;
	}

	// 三个 Spec 全部有效后再统一应用，避免角色只完成一部分属性初始化。
	ASC->ApplyGameplayEffectSpecToSelf(*PrimaryAttributesSpecHandle.Data.Get());
	ASC->ApplyGameplayEffectSpecToSelf(*SecondaryAttributesSpecHandle.Data.Get());
	ASC->ApplyGameplayEffectSpecToSelf(*VitalAttributesSpecHandle.Data.Get());
}

void UAuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ULoadScreenSaveGame* SaveGame)
{
	if (!ensureMsgf(IsValid(ASC) && IsValid(SaveGame), TEXT("Save data attribute initialization requires a valid ASC and save game"))) return;
	if (!ASC->GetOwnerActor() || !ASC->GetOwnerActor()->HasAuthority()) return;

	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (!ensureMsgf(IsValid(CharacterClassInfo), TEXT("CharacterClassInfo is not configured on Aura GameMode"))) return;

	const TSubclassOf<UGameplayEffect> SecondaryAttributesClass = CharacterClassInfo->SecondaryAttributes_Infinite
		? CharacterClassInfo->SecondaryAttributes_Infinite
		: CharacterClassInfo->SecondaryAttributes;
	if (!ensureMsgf(CharacterClassInfo->PrimaryAttributes_SetByCaller && SecondaryAttributesClass && CharacterClassInfo->VitalAttributes,
		TEXT("Save data attribute GameplayEffects are not configured on CharacterClassInfo"))) return;
	if (!CharacterClassInfo->SecondaryAttributes_Infinite)
	{
		UE_LOG(LogTemp, Warning, TEXT("SecondaryAttributes_Infinite is not configured; using SecondaryAttributes for save data initialization"));
	}

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	const AActor* SourceAvatarActor = ASC->GetAvatarActor();

	FGameplayEffectContextHandle EffectContextHandle = ASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(SourceAvatarActor);

	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(CharacterClassInfo->PrimaryAttributes_SetByCaller, 1.f, EffectContextHandle);

	FGameplayEffectContextHandle SecondaryAttributesContextHandle = ASC->MakeEffectContext();
	SecondaryAttributesContextHandle.AddSourceObject(SourceAvatarActor);
	const FGameplayEffectSpecHandle SecondaryAttributesSpecHandle = ASC->MakeOutgoingSpec(SecondaryAttributesClass, 1.f, SecondaryAttributesContextHandle);

	FGameplayEffectContextHandle VitalAttributesContextHandle = ASC->MakeEffectContext();
	VitalAttributesContextHandle.AddSourceObject(SourceAvatarActor);
	const FGameplayEffectSpecHandle VitalAttributesSpecHandle = ASC->MakeOutgoingSpec(CharacterClassInfo->VitalAttributes, 1.f, VitalAttributesContextHandle);

	if (!ensureMsgf(SpecHandle.IsValid() && SecondaryAttributesSpecHandle.IsValid() && VitalAttributesSpecHandle.IsValid(),
		TEXT("Failed to create one or more save data attribute GameplayEffect specs"))) return;

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Strength, SaveGame->Strength);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Intelligence, SaveGame->Intelligence);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Resilience, SaveGame->Resilience);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Attributes_Primary_Vigor, SaveGame->Vigor);

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	ASC->ApplyGameplayEffectSpecToSelf(*SecondaryAttributesSpecHandle.Data.Get());
	ASC->ApplyGameplayEffectSpecToSelf(*VitalAttributesSpecHandle.Data.Get());
}

void UAuraAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass)
{
	if (ASC == nullptr || ASC->GetOwnerActor() == nullptr || !ASC->GetOwnerActor()->HasAuthority()) return;
	
	AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (AuraGameMode == nullptr) return;

	UCharacterClassInfo* CharacterClassInfo = AuraGameMode->CharacterClassInfo;
	if (CharacterClassInfo == nullptr) return;

	const FCharacterClassDefaultInfo* const ClassDefaultInfo =
		CharacterClassInfo->CharacterClassInformation.Find(CharacterClass);
	if (!ensureMsgf(ClassDefaultInfo,
		TEXT("CharacterClassInfo has no startup abilities for character class [%d]"), static_cast<int32>(CharacterClass)))
	{
		return;
	}

	for (TSubclassOf<UGameplayAbility> AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		if (AbilityClass == nullptr) continue;
		
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		ASC->GiveAbility(AbilitySpec);
	}

	AActor* AvatarActor = ASC->GetAvatarActor();
	if (IsValid(AvatarActor) && AvatarActor->Implements<UCombatInterface>())
	{
		const int32 CharacterLevel = ICombatInterface::Execute_GetPlayerLevel(AvatarActor);
		for (TSubclassOf<UGameplayAbility> AbilityClass : ClassDefaultInfo->StartupAbilities)
		{
			if (AbilityClass == nullptr) continue;

			FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
			ASC->GiveAbility(AbilitySpec);
		}
	}
}

UCharacterClassInfo* UAuraAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (AuraGameMode == nullptr) return nullptr;
	return AuraGameMode->CharacterClassInfo;
}

UAbilityInfo* UAuraAbilitySystemLibrary::GetAbilityInfo(const UObject* WorldContextObject)
{
	AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (AuraGameMode == nullptr) return nullptr;
	return AuraGameMode->AbilityInfo;
}

int32 UAuraAbilitySystemLibrary::GetXPRewardForClassAndLevel(const UObject* WorldContextObject,
	ECharacterClass CharacterClass, int32 CharacterLevel)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return 0;

	const FCharacterClassDefaultInfo& Info = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
	const float XPReward = Info.XPReward.GetValueAtLevel(CharacterLevel);

	return static_cast<int32>(XPReward);
}

bool UAuraAbilitySystemLibrary::AreActorsFriends(const AActor* FirstActor, const AActor* SecondActor)
{
	if (!IsValid(FirstActor) || !IsValid(SecondActor))
	{
		return false;
	}

	static const FName AuraTag = TEXT("Aura");
	static const FName EnemyTag = TEXT("Enemy");

	const bool bBothAura = FirstActor->ActorHasTag(AuraTag) && SecondActor->ActorHasTag(AuraTag);
	const bool bBothEnemies = FirstActor->ActorHasTag(EnemyTag) && SecondActor->ActorHasTag(EnemyTag);

	return bBothAura || bBothEnemies;
}

FGameplayEffectContextHandle UAuraAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams)
{
	// 施法者可能在投射物飞行途中死亡或被销毁，且本函数对蓝图开放，
	// 因此两端 ASC 与伤害 GE 类都必须在使用前重新校验。
	UAbilitySystemComponent* const SourceASC = DamageEffectParams.SourceAbilitySystemComponent;
	UAbilitySystemComponent* const TargetASC = DamageEffectParams.TargetAbilitySystemComponent;
	if (!IsValid(SourceASC) || !IsValid(TargetASC) || !DamageEffectParams.DamageGameplayEffectClass)
	{
		return FGameplayEffectContextHandle();
	}

	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();
	AActor* const SourceAvatarActor = SourceASC->GetAvatarActor();

	FGameplayEffectContextHandle EffectContextHandle = SourceASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(SourceAvatarActor);
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectParams.DamageGameplayEffectClass,
		DamageEffectParams.AbilityLevel,
		EffectContextHandle);
	if (!SpecHandle.IsValid())
	{
		return EffectContextHandle;
	}

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle,
		DamageEffectParams.DamageType,
		DamageEffectParams.BaseDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle,
		GameplayTags.Debuff_Chance,
		DamageEffectParams.bEnableDebuff ? DamageEffectParams.DebuffChance : 0.f);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle,
		GameplayTags.Debuff_Damage,
		DamageEffectParams.bEnableDebuff ? DamageEffectParams.DebuffDamage : 0.f);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle,
		GameplayTags.Debuff_Duration,
		DamageEffectParams.bEnableDebuff ? DamageEffectParams.DebuffDuration : 0.f);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle,
		GameplayTags.Debuff_Frequency,
		DamageEffectParams.bEnableDebuff ? DamageEffectParams.DebuffFrequency : 0.f);

	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return EffectContextHandle;
}

bool UAuraAbilitySystemLibrary::IsBlockedHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraContext = GetAuraEffectContext(EffectContextHandle))
	{
		return AuraContext->IsBlockedHit();
	}
	return false;
}

bool UAuraAbilitySystemLibrary::IsCriticalHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FAuraGameplayEffectContext* AuraContext = GetAuraEffectContext(EffectContextHandle))
	{
		return AuraContext->IsCriticalHit();
	}
	return false;
}

void UAuraAbilitySystemLibrary::SetIsBlockedHit(FGameplayEffectContextHandle& EffectContextHandle, bool bInIsBlockedHit)
{
	if (FAuraGameplayEffectContext* AuraContext = GetAuraEffectContext(EffectContextHandle))
	{
		AuraContext->SetIsBlockedHit(bInIsBlockedHit);
	}
}

void UAuraAbilitySystemLibrary::SetIsCriticalHit(FGameplayEffectContextHandle& EffectContextHandle, bool bInIsCriticalHit)
{
	if (FAuraGameplayEffectContext* AuraContext = GetAuraEffectContext(EffectContextHandle))
	{
		AuraContext->SetIsCriticalHit(bInIsCriticalHit);
	}
}

void UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(
	const UObject* WorldContextObject,
	TArray<AActor*>& OutOverlappingActors,
	const TArray<AActor*>& ActorsToIgnore,
	float Radius,
	const FVector& SphereOrigin)
{
	FCollisionQueryParams SphereParams;
	SphereParams.AddIgnoredActors(ActorsToIgnore);

	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull)
		: nullptr;
	if (!World || Radius <= 0.f) return;

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		SphereOrigin,
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects),
		FCollisionShape::MakeSphere(Radius),
		SphereParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlappingActor = Overlap.GetActor();
		if (!IsValid(OverlappingActor) || !OverlappingActor->Implements<UCombatInterface>()) continue;
		if (ICombatInterface::Execute_IsDead(OverlappingActor)) continue;

		AActor* AvatarActor = ICombatInterface::Execute_GetAvatar(OverlappingActor);
		if (IsValid(AvatarActor))
		{
			OutOverlappingActors.AddUnique(AvatarActor);
		}
	}
}

void UAuraAbilitySystemLibrary::GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, TArray<AActor*>& OutClosestTargets, const FVector& Origin)
{
	if (MaxTargets <= 0)
	{
		return;
	}

	if (Actors.Num() <= MaxTargets)
	{
		OutClosestTargets = Actors;
		return;
	}

	TArray<AActor*> ActorsToCheck = Actors;
	int32 NumTargetsFound = 0;

	while (NumTargetsFound < MaxTargets)
	{
		if (ActorsToCheck.Num() == 0) break;
		double ClosestDistance = TNumericLimits<double>::Max();
		AActor* ClosestActor = nullptr;
		for (AActor* PotentialTarget : ActorsToCheck)
		{
			if (!IsValid(PotentialTarget)) continue;
			const double Distance = (PotentialTarget->GetActorLocation() - Origin).Length();
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestActor = PotentialTarget;
			}
		}
		if (!ClosestActor) break;
		ActorsToCheck.Remove(ClosestActor);
		OutClosestTargets.AddUnique(ClosestActor);
		++NumTargetsFound;
	}
}
