#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include <type_traits>

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AuraFireBolt.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AbilitySystem/Abilities/AuraFireDelayedBlastAbility.h"
#include "Actor/AuraDelayedBlastActor.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectGlobals.h"

static_assert(std::is_same_v<decltype(&UAuraFireBolt::GetDescription), FString (UAuraFireBolt::*)(int32)>,
	"UAuraFireBolt::GetDescription must be declared on UAuraFireBolt");
static_assert(std::is_same_v<decltype(&UAuraFireBolt::GetNextLevelDescription), FString (UAuraFireBolt::*)(int32)>,
	"UAuraFireBolt::GetNextLevelDescription must be declared on UAuraFireBolt");
static_assert(std::is_same_v<decltype(&UAuraFireDelayedBlastAbility::GetDescription), FString (UAuraFireDelayedBlastAbility::*)(int32)>,
	"UAuraFireDelayedBlastAbility::GetDescription must be declared on UAuraFireDelayedBlastAbility");
static_assert(std::is_same_v<decltype(&UAuraFireDelayedBlastAbility::GetNextLevelDescription), FString (UAuraFireDelayedBlastAbility::*)(int32)>,
	"UAuraFireDelayedBlastAbility::GetNextLevelDescription must be declared on UAuraFireDelayedBlastAbility");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayAbilityDescriptionTest,
	"Aura.AbilitySystem.GameplayAbility.Description",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayAbilityDescriptionTest::RunTest(const FString& Parameters)
{
	UAuraGameplayAbility* AbilityCDO = UAuraGameplayAbility::StaticClass()->GetDefaultObject<UAuraGameplayAbility>();
	TestNotNull(TEXT("Aura gameplay ability class default object"), AbilityCDO);

	if (!AbilityCDO)
	{
		return false;
	}

	TestEqual(
		TEXT("Level 1 description uses the exact Chinese rich text output"),
		AbilityCDO->GetDescription(1),
		FString(TEXT("<Default>默认技能名称 - 技能描述占位文本，当前等级：</><Level>1 级</>")));

	TestEqual(
		TEXT("Level 2 next-level description uses the exact Chinese rich text output"),
		AbilityCDO->GetNextLevelDescription(2),
		FString(TEXT("<Default>下一等级：</><Level>2 级</>\n<Default>造成更多伤害。</>")));

	TestEqual(
		TEXT("Level 3 locked description uses the exact Chinese rich text output"),
		UAuraGameplayAbility::GetLockedDescription(3),
		FString(TEXT("<Default>技能将在角色等级达到 </><Level>3 级</><Default> 时解锁。</>")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraAbilitySystemComponentDescriptionMissingContextTest,
	"Aura.AbilitySystem.AbilitySystemComponent.DescriptionsByAbilityTag.MissingContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraAbilitySystemComponentDescriptionMissingContextTest::RunTest(const FString& Parameters)
{
	UAuraAbilitySystemComponent* AbilitySystemComponent = NewObject<UAuraAbilitySystemComponent>(GetTransientPackage());
	TestNotNull(TEXT("Transient Aura ability system component"), AbilitySystemComponent);

	if (!AbilitySystemComponent)
	{
		return false;
	}

	FString Description(TEXT("Stale description"));
	FString NextLevelDescription(TEXT("Stale next-level description"));

	const bool bFoundDescriptions = AbilitySystemComponent->GetDescriptionsByAbilityTag(
		FGameplayTag(), nullptr, Description, NextLevelDescription);

	TestFalse(TEXT("Break: failure to clear outputs or safely reject missing context"), bFoundDescriptions);
	TestTrue(TEXT("Description output is cleared"), Description.IsEmpty());
	TestTrue(TEXT("Next-level description output is cleared"), NextLevelDescription.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraFireBoltDescriptionTest,
	"Aura.AbilitySystem.FireBolt.Description",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraFireBoltDescriptionTest::RunTest(const FString& Parameters)
{
	UAuraFireBolt* AbilityCDO = UAuraFireBolt::StaticClass()->GetDefaultObject<UAuraFireBolt>();
	TestNotNull(TEXT("Aura FireBolt class default object"), AbilityCDO);

	if (!AbilityCDO)
	{
		return false;
	}

	// The current GA_FIreBolt graph calls SpawnProjectile once. The native CDO
	// has no damage curve, cost effect or cooldown effect, so those values are zero.
	for (const int32 Level : {1, 2, 7})
	{
		for (const bool bNextLevel : {false, true})
		{
			const FString Description = bNextLevel
				? AbilityCDO->GetNextLevelDescription(Level) : AbilityCDO->GetDescription(Level);
			const FString Context = FString::Printf(TEXT("Level %d, next=%d: "), Level, bNextLevel);
			TestTrue(Context + TEXT("correct title"), Description.Contains(bNextLevel
				? TEXT("<Title>下一等级：</>") : TEXT("<Title>火焰弹</>")));
			TestTrue(Context + TEXT("requested level"), Description.Contains(
				FString::Printf(TEXT("<Level>%d</>"), Level)));
			TestTrue(Context + TEXT("one projectile matches the current cast"),
				Description.Contains(TEXT("发射一枚火焰弹")) || Description.Contains(TEXT("发射 1 枚火焰弹")));
			TestTrue(Context + TEXT("damage field"), Description.Contains(TEXT("<Damage>0</>")));
			TestTrue(Context + TEXT("mana field"), Description.Contains(TEXT("<ManaCost>0.0</>")));
			TestTrue(Context + TEXT("cooldown field"), Description.Contains(TEXT("<Cooldown>0.0</>")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraFireDelayedBlastDescriptionTest,
	"Aura.AbilitySystem.FireDelayedBlast.Description",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraFireDelayedBlastDescriptionTest::RunTest(const FString& Parameters)
{
	UAuraFireDelayedBlastAbility* AbilityCDO = UAuraFireDelayedBlastAbility::StaticClass()->GetDefaultObject<UAuraFireDelayedBlastAbility>();
	TestNotNull(TEXT("Aura fire delayed blast class default object"), AbilityCDO);
	if (!AbilityCDO)
	{
		return false;
	}

	const AAuraDelayedBlastActor* ActorCDO = AAuraDelayedBlastActor::StaticClass()->GetDefaultObject<AAuraDelayedBlastActor>();
	TestNotNull(TEXT("Delayed blast actor class default object"), ActorCDO);
	if (!ActorCDO)
	{
		return false;
	}

	auto ExpectDescription = [this, AbilityCDO, ActorCDO](int32 Level, bool bNextLevel)
	{
		const FString Description = bNextLevel
			? AbilityCDO->GetNextLevelDescription(Level)
			: AbilityCDO->GetDescription(Level);
		const FString Context = FString::Printf(TEXT("Native CDO level %d next=%d: "), Level, bNextLevel);
		const FString ExpectedDelay = FString::Printf(TEXT("<Cooldown>%.1f</>"), ActorCDO->GetWarningDelay());
		const FString ExpectedRadius = FString::Printf(TEXT("<Level>%.1f</>"), ActorCDO->GetBlastRadius() / 100.f);

		TestTrue(Context + TEXT("title"), Description.Contains(bNextLevel ? TEXT("<Title>下一等级：</>") : TEXT("<Title>延时爆破</>")));
		TestTrue(Context + TEXT("requested level"), Description.Contains(FString::Printf(TEXT("<Level>%d</>"), Level)));
		TestFalse(Context + TEXT("does not increment level"), Description.Contains(FString::Printf(TEXT("<Level>%d</>"), Level + 1)));
		TestTrue(Context + TEXT("native CDO damage is data-driven zero"), Description.Contains(TEXT("<Damage>0</>")));
		TestTrue(Context + TEXT("native CDO mana is data-driven zero"), Description.Contains(TEXT("<ManaCost>0.0</>")));
		TestTrue(Context + TEXT("delay from actor CDO"), Description.Contains(*ExpectedDelay));
		TestTrue(Context + TEXT("radius meters from actor CDO"), Description.Contains(*ExpectedRadius));
		TestFalse(Context + TEXT("no placeholder copy"), Description.Contains(TEXT("占位")));
	};

	ExpectDescription(1, false);
	ExpectDescription(2, true);

	UBlueprint* DelayBlastBlueprint = LoadObject<UBlueprint>(
		nullptr,
		TEXT("/Game/Blueprints/AbilitySystem/Aura/Abililties/DelayedBlast/GA_DelayBlast.GA_DelayBlast"));
	if (DelayBlastBlueprint && DelayBlastBlueprint->GeneratedClass &&
		DelayBlastBlueprint->GeneratedClass->IsChildOf(UAuraFireDelayedBlastAbility::StaticClass()))
	{
		UAuraFireDelayedBlastAbility* AssetCDO = DelayBlastBlueprint->GeneratedClass->GetDefaultObject<UAuraFireDelayedBlastAbility>();
		TestNotNull(TEXT("GA_DelayBlast generated CDO"), AssetCDO);
		if (AssetCDO)
		{
			const FString Description = AssetCDO->GetDescription(1);
			const FString NextDescription = AssetCDO->GetNextLevelDescription(2);
			TestTrue(TEXT("GA description is not placeholder"), !Description.Contains(TEXT("占位")));
			TestTrue(TEXT("GA description uses fire delayed blast title"), Description.Contains(TEXT("<Title>延时爆破</>")));
			TestTrue(TEXT("GA description differs from native zero damage"), !Description.Contains(TEXT("<Damage>0</>")));
			TestTrue(TEXT("GA next-level uses passed level 2"), NextDescription.Contains(TEXT("<Level>2</>")));
			TestFalse(TEXT("GA next-level does not add one"), NextDescription.Contains(TEXT("<Level>3</>")));
		}
	}

	return true;
}

#endif
