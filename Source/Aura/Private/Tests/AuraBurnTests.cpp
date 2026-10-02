// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/AuraDebuffConfig.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDebuffChanceFormulaTest,
	"Aura.AbilitySystem.Burn.ChanceFormula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDebuffChanceFormulaTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Chance 0 never triggers"), UAuraAttributeSet::CalculateDebuffTriggerChance(0.f, 0.f), 0.f);
	TestEqual(TEXT("Chance 100 with 0 resistance"), UAuraAttributeSet::CalculateDebuffTriggerChance(100.f, 0.f), 100.f);
	TestEqual(TEXT("Resistance 100 zeros chance"), UAuraAttributeSet::CalculateDebuffTriggerChance(100.f, 100.f), 0.f);
	TestEqual(TEXT("Chance 40 resistance 25"), UAuraAttributeSet::CalculateDebuffTriggerChance(40.f, 25.f), 30.f);
	TestEqual(TEXT("Chance is clamped to 100"), UAuraAttributeSet::CalculateDebuffTriggerChance(150.f, 0.f), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraDebuffConfigRowValidationTest,
	"Aura.AbilitySystem.Burn.ConfigRow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraDebuffConfigRowValidationTest::RunTest(const FString& Parameters)
{
	FAuraDebuffConfigRow Valid;
	Valid.Chance = 20.f;
	Valid.Damage = 5.f;
	Valid.Duration = 5.f;
	Valid.Frequency = 1.f;
	TestTrue(TEXT("migrated FireBolt values are valid"), Valid.IsValidConfig());

	FAuraDebuffConfigRow ZeroChance = Valid;
	ZeroChance.Chance = 0.f;
	TestTrue(TEXT("chance 0 remains a valid row"), ZeroChance.IsValidConfig());

	FAuraDebuffConfigRow BadDuration = Valid;
	BadDuration.Duration = 0.f;
	TestFalse(TEXT("duration 0 is invalid"), BadDuration.IsValidConfig());

	FAuraDebuffConfigRow BadChance = Valid;
	BadChance.Chance = 120.f;
	TestFalse(TEXT("chance above 100 is invalid"), BadChance.IsValidConfig());
	return true;
}

#endif
