// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/ModMagCalc/MMC_MaxMana.h"

#include "AbilitySystem/AuraAttributeSet.h"
#include "Interfaction/CombatInterface.h"

UMMC_MaxMana::UMMC_MaxMana()
{
	IntelligenceDef.AttributeToCapture=UAuraAttributeSet::GetIntelligenceAttribute();
	IntelligenceDef.AttributeSource=EGameplayEffectAttributeCaptureSource::Target;
	IntelligenceDef.bSnapshot=false;
	
	RelevantAttributesToCapture.Add(IntelligenceDef);
	
}

float UMMC_MaxMana::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const FGameplayTagContainer *SourceTag=Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer *TargetTag=Spec.CapturedTargetTags.GetAggregatedTags();
	
	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags=SourceTag;
	EvaluateParameters.TargetTags=TargetTag;
	
	float Intelligence=0.f;
	GetCapturedAttributeMagnitude(IntelligenceDef,Spec,EvaluateParameters,Intelligence);
	Intelligence=FMath::Max<float>(Intelligence,0.f);
	
	UObject* SourceObject = Spec.GetContext().GetSourceObject();
	check(SourceObject && SourceObject->Implements<UCombatInterface>());
	const int32 PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(SourceObject);
	
	return 50.f + Intelligence * 2.5 + 15.f * PlayerLevel;
}
