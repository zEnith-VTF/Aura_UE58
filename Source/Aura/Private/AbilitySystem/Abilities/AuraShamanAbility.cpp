// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AuraShamanAbility.h"

#include "Kismet/KismetSystemLibrary.h"

TArray<FVector> UAuraShamanAbility::GetSpawnLocations()
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return {};
	}

	const FVector Forward = AvatarActor->GetActorForwardVector();
	const FVector Location = AvatarActor->GetActorLocation();
	if (NumMinions <= 0)
	{
		return {};
	}

	// 从扇形右边界开始，按召唤数量依次向左划分方向。
	const float DeltaSpread = SpawnSpread / NumMinions;
	const FVector RightOfSpread = Forward.RotateAngleAxis(SpawnSpread / 2.f, FVector::UpVector);
	TArray<FVector> SpawnLocations;
	SpawnLocations.Reserve(NumMinions);

	for (int32 Index = 0; Index < NumMinions; ++Index)
	{
		const FVector Direction = RightOfSpread.RotateAngleAxis(-DeltaSpread * Index, FVector::UpVector);
		const float SpawnDistance = FMath::FRandRange(MinSpawnDistance, MaxSpawnDistance);
		FVector ChosenSpawnLocation = Location + Direction * SpawnDistance;

		// 从候选点上方向下检测地面，使随从可以生成在斜坡或高低不平的地形上。
		FHitResult GroundHit;
		const FVector TraceStart = ChosenSpawnLocation + FVector::UpVector * 400.f;
		const FVector TraceEnd = ChosenSpawnLocation - FVector::UpVector * 400.f;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(AvatarActor);

		if (GetWorld()->LineTraceSingleByChannel(
			GroundHit,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams))
		{
			ChosenSpawnLocation = GroundHit.ImpactPoint;
		}

		SpawnLocations.Add(ChosenSpawnLocation);
	}

	return SpawnLocations;
}

TSubclassOf<APawn> UAuraShamanAbility::GetRandomMinionClass() const
{
	if (MinionClasses.IsEmpty())
	{
		return nullptr;
	}

	const int32 Selection = FMath::RandRange(0, MinionClasses.Num() - 1);
	return MinionClasses[Selection];
}
