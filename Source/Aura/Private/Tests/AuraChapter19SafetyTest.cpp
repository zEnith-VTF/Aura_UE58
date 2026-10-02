#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AuraAbilitySystemGlobals.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Actor/AuraProjectile.h"
#include "AuraAbilityTypes.h"
#include "Characters/AuraCharacter.h"
#include "Characters/AuraCharacterBase.h"
#include "Characters/AuraEnemy.h"
#include "Interfaction/CombatInterface.h"
#include "Interfaction/EnemyInterface.h"
#include "Tags/AuraGameplayTags.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraEffectContextTypeSafetyTest,
	"Aura.AbilitySystem.EffectContext.TypeSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraEffectContextTypeSafetyTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Ability System Globals uses the Aura subclass configured in DefaultGame.ini"),
		UAbilitySystemGlobals::Get().IsA<UAuraAbilitySystemGlobals>());

	// 基类 Context 可能来自错误配置或第三方系统。Aura 访问器必须安全拒绝它。
	FGameplayEffectContextHandle BaseContext(new FGameplayEffectContext());
	UAuraAbilitySystemLibrary::SetIsBlockedHit(BaseContext, true);
	UAuraAbilitySystemLibrary::SetIsCriticalHit(BaseContext, true);
	TestFalse(TEXT("Base GameplayEffectContext is not treated as blocked"),
		UAuraAbilitySystemLibrary::IsBlockedHit(BaseContext));
	TestFalse(TEXT("Base GameplayEffectContext is not treated as critical"),
		UAuraAbilitySystemLibrary::IsCriticalHit(BaseContext));

	// 正确的 Aura Context 仍需完整保留自定义命中标记。
	FGameplayEffectContextHandle AuraContext(new FAuraGameplayEffectContext());
	UAuraAbilitySystemLibrary::SetIsBlockedHit(AuraContext, true);
	UAuraAbilitySystemLibrary::SetIsCriticalHit(AuraContext, true);
	TestTrue(TEXT("Aura GameplayEffectContext stores blocked-hit state"),
		UAuraAbilitySystemLibrary::IsBlockedHit(AuraContext));
	TestTrue(TEXT("Aura GameplayEffectContext stores critical-hit state"),
		UAuraAbilitySystemLibrary::IsCriticalHit(AuraContext));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCombatSocketGameplayTagsTest,
	"Aura.GameplayTags.CombatSockets.Registered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCombatSocketGameplayTagsTest::RunTest(const FString& Parameters)
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	TestEqual(TEXT("Weapon combat socket tag"),
		GameplayTags.CombatSocket_Weapon.ToString(), FString(TEXT("CombatSocket.Weapon")));
	TestEqual(TEXT("Right-hand combat socket tag"),
		GameplayTags.CombatSocket_RightHand.ToString(), FString(TEXT("CombatSocket.RightHand")));
	TestEqual(TEXT("Left-hand combat socket tag"),
		GameplayTags.CombatSocket_LeftHand.ToString(), FString(TEXT("CombatSocket.LeftHand")));
	TestEqual(TEXT("Tail combat socket tag"),
		GameplayTags.CombatSocket_Tail.ToString(), FString(TEXT("CombatSocket.Tail")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraAttackMontageGameplayTagsTest,
	"Aura.GameplayTags.AttackMontages.RegisteredAndExposeSocketTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraAttackMontageGameplayTagsTest::RunTest(const FString& Parameters)
{
	const FAuraGameplayTags& GameplayTags = FAuraGameplayTags::Get();

	TestEqual(TEXT("Attack montage tag 1"),
		GameplayTags.Montage_Attack_1.ToString(), FString(TEXT("Montage.Attack.1")));
	TestEqual(TEXT("Attack montage tag 2"),
		GameplayTags.Montage_Attack_2.ToString(), FString(TEXT("Montage.Attack.2")));
	TestEqual(TEXT("Attack montage tag 3"),
		GameplayTags.Montage_Attack_3.ToString(), FString(TEXT("Montage.Attack.3")));
	TestEqual(TEXT("Attack montage tag 4"),
		GameplayTags.Montage_Attack_4.ToString(), FString(TEXT("Montage.Attack.4")));

	const UFunction* TaggedMontageLookupFunction = UCombatInterface::StaticClass()->FindFunctionByName(
		TEXT("GetTaggedMontageByTag"));
	TestNotNull(TEXT("CombatInterface exposes GetTaggedMontageByTag"), TaggedMontageLookupFunction);

	AAuraCharacterBase* CharacterBaseCDO =
		AAuraCharacterBase::StaticClass()->GetDefaultObject<AAuraCharacterBase>();
	TestNotNull(TEXT("Aura character base class default object"), CharacterBaseCDO);
	if (CharacterBaseCDO)
	{
		const FTaggedMontage MissingMontage = ICombatInterface::Execute_GetTaggedMontageByTag(
			CharacterBaseCDO,
			GameplayTags.Montage_Attack_1);
		TestNull(TEXT("Missing montage tag returns an empty montage"), MissingMontage.Montage.Get());
		TestFalse(TEXT("Missing montage tag returns an empty socket tag"), MissingMontage.SocketTag.IsValid());
	}

	const FProperty* SocketTagProperty = FindFProperty<FProperty>(
		FTaggedMontage::StaticStruct(),
		TEXT("SocketTag"));
	TestNotNull(TEXT("FTaggedMontage exposes SocketTag through reflection"), SocketTagProperty);
	if (SocketTagProperty)
	{
		TestTrue(TEXT("SocketTag is editable on montage defaults"),
			SocketTagProperty->HasAnyPropertyFlags(CPF_Edit));
		TestTrue(TEXT("SocketTag is visible to Blueprint"),
			SocketTagProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraProjectileConfigurationTest,
	"Aura.Projectile.Configuration.NetworkAndDamageSpec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraProjectileConfigurationTest::RunTest(const FString& Parameters)
{
	const AAuraProjectile* ProjectileCDO = AAuraProjectile::StaticClass()->GetDefaultObject<AAuraProjectile>();
	TestNotNull(TEXT("Aura projectile class default object"), ProjectileCDO);
	if (!ProjectileCDO)
	{
		return false;
	}

	TestTrue(TEXT("Projectile Actor replicates"), ProjectileCDO->GetIsReplicated());
	TestTrue(TEXT("Projectile movement replicates"), ProjectileCDO->IsReplicatingMovement());

	const FProperty* DamageSpecProperty = FindFProperty<FProperty>(
		AAuraProjectile::StaticClass(),
		TEXT("DamageEffectParams"));
	TestNotNull(TEXT("DamageEffectParams is reflected"), DamageSpecProperty);
	if (!DamageSpecProperty)
	{
		return false;
	}

	TestTrue(TEXT("DamageEffectParams is visible to Blueprint"),
		DamageSpecProperty->HasAnyPropertyFlags(CPF_BlueprintVisible));
	TestTrue(TEXT("DamageEffectParams can be assigned when spawning"),
		DamageSpecProperty->HasAnyPropertyFlags(CPF_ExposeOnSpawn));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCombatTargetLifecycleTest,
	"Aura.AI.CombatTarget.RejectsDeadTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCombatTargetLifecycleTest::RunTest(const FString& Parameters)
{
	AAuraEnemy* EnemyCDO = AAuraEnemy::StaticClass()->GetDefaultObject<AAuraEnemy>();
	AAuraCharacter* TargetCDO = AAuraCharacter::StaticClass()->GetDefaultObject<AAuraCharacter>();
	TestNotNull(TEXT("Aura enemy class default object"), EnemyCDO);
	TestNotNull(TEXT("Aura character target class default object"), TargetCDO);

	FBoolProperty* DeadProperty = FindFProperty<FBoolProperty>(
		AAuraCharacterBase::StaticClass(),
		TEXT("bDead"));
	FObjectPropertyBase* CombatTargetProperty = CastField<FObjectPropertyBase>(FindFProperty<FProperty>(
		AAuraEnemy::StaticClass(),
		TEXT("CombatTarget")));
	TestNotNull(TEXT("bDead reflected bool property"), DeadProperty);
	TestNotNull(TEXT("CombatTarget reflected object property"), CombatTargetProperty);

	if (!EnemyCDO || !TargetCDO || !DeadProperty || !CombatTargetProperty)
	{
		return false;
	}

	const bool bOriginalDead = DeadProperty->GetPropertyValue_InContainer(TargetCDO);
	UObject* const OriginalCombatTarget = CombatTargetProperty->GetObjectPropertyValue_InContainer(EnemyCDO);

	DeadProperty->SetPropertyValue_InContainer(TargetCDO, false);
	IEnemyInterface::Execute_SetCombatTarget(EnemyCDO, TargetCDO);
	TestTrue(TEXT("Living combat target is retained"),
		IEnemyInterface::Execute_GetCombatTarget(EnemyCDO) == TargetCDO);

	DeadProperty->SetPropertyValue_InContainer(TargetCDO, true);
	TestNull(TEXT("A target that dies is no longer returned"),
		IEnemyInterface::Execute_GetCombatTarget(EnemyCDO));

	IEnemyInterface::Execute_SetCombatTarget(EnemyCDO, TargetCDO);
	TestNull(TEXT("A dead target cannot be assigned again"),
		IEnemyInterface::Execute_GetCombatTarget(EnemyCDO));

	// 自动化测试使用 CDO，结束前恢复所有临时状态，避免污染后续测试。
	DeadProperty->SetPropertyValue_InContainer(TargetCDO, bOriginalDead);
	CombatTargetProperty->SetObjectPropertyValue_InContainer(EnemyCDO, OriginalCombatTarget);

	return true;
}

#endif
