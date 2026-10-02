#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AuraCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCharacterCameraCollisionTest,
	"Aura.Character.CameraCollision.IgnoresOwnCameraChannel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCharacterCameraCollisionTest::RunTest(const FString& Parameters)
{
	const AAuraCharacter* CharacterCDO = AAuraCharacter::StaticClass()->GetDefaultObject<AAuraCharacter>();
	TestNotNull(TEXT("AAuraCharacter class default object"), CharacterCDO);

	if (!CharacterCDO)
	{
		return false;
	}

	TestEqual(
		TEXT("Player character capsule ignores the Camera collision channel"),
		CharacterCDO->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Camera),
		ECR_Ignore);

	TestEqual(
		TEXT("Player character mesh ignores the Camera collision channel"),
		CharacterCDO->GetMesh()->GetCollisionResponseToChannel(ECC_Camera),
		ECR_Ignore);

	return true;
}

#endif
