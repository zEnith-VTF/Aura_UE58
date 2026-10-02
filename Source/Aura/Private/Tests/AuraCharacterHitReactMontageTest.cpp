#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimMontage.h"
#include "Characters/AuraCharacterBase.h"
#include "Interfaction/CombatInterface.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCharacterHitReactMontageTest,
	"Aura.Character.CombatInterface.ReturnsConfiguredHitReactMontage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCharacterHitReactMontageTest::RunTest(const FString& Parameters)
{
	AAuraCharacterBase* CharacterCDO = AAuraCharacterBase::StaticClass()->GetDefaultObject<AAuraCharacterBase>();
	TestNotNull(TEXT("Aura character base class default object"), CharacterCDO);

	if (!CharacterCDO)
	{
		return false;
	}

	FProperty* HitReactMontageProperty = FindFProperty<FProperty>(
		AAuraCharacterBase::StaticClass(),
		TEXT("HitReactMontage"));
	FObjectPropertyBase* HitReactMontageObjectProperty = CastField<FObjectPropertyBase>(HitReactMontageProperty);
	TestNotNull(TEXT("HitReactMontage reflected object property"), HitReactMontageObjectProperty);

	if (!HitReactMontageObjectProperty)
	{
		return false;
	}

	UObject* OriginalMontage = HitReactMontageObjectProperty->GetObjectPropertyValue_InContainer(CharacterCDO);
	UAnimMontage* ExpectedMontage = NewObject<UAnimMontage>(GetTransientPackage());

	HitReactMontageObjectProperty->SetObjectPropertyValue_InContainer(CharacterCDO, ExpectedMontage);
	const UAnimMontage* ActualMontage = ICombatInterface::Execute_GetHitReactMontage(CharacterCDO);

	TestTrue(
		TEXT("Combat interface returns the configured hit react montage"),
		ActualMontage == ExpectedMontage);

	HitReactMontageObjectProperty->SetObjectPropertyValue_InContainer(CharacterCDO, OriginalMontage);

	return true;
}

#endif
