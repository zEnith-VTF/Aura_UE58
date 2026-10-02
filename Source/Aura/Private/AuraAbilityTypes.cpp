

#include "AuraAbilityTypes.h"

bool FAuraGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	//先判断父类是否序列化成功
	bool bBaseSuccess = false;
	const bool bBaseResult = FGameplayEffectContext::NetSerialize(Ar, Map, bBaseSuccess);

	uint8 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (bIsBlockedHit)
		{
			RepBits |= 1 << 0;
		}
		if (bIsCriticalHit)
		{
			RepBits |= 1 << 1;
		}
	}

	Ar.SerializeBits(&RepBits, 2);

	if (Ar.IsLoading())
	{
		bIsBlockedHit = (RepBits & (1 << 0)) != 0;
		bIsCriticalHit = (RepBits & (1 << 1)) != 0;
	}

	bOutSuccess = bBaseSuccess && !Ar.IsError();
	return bBaseResult && bOutSuccess;
}


