// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/ModMagCalc/MMC_MaxHealth.h"

#include "AbilitySystem/AuraAttributeSet.h"
#include "Interfaction/CombatInterface.h"

UMMC_MaxHealth::UMMC_MaxHealth()
{
	VigorDef.AttributeToCapture=UAuraAttributeSet::GetVigorAttribute();
	VigorDef.AttributeSource=EGameplayEffectAttributeCaptureSource::Target;
	VigorDef.bSnapshot=false;
	
	RelevantAttributesToCapture.Add(VigorDef);
}

float UMMC_MaxHealth::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const FGameplayTagContainer *SourceTag=Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer *TargetTag=Spec.CapturedTargetTags.GetAggregatedTags();
	
	FAggregatorEvaluateParameters EvaluateParameters;
	EvaluateParameters.SourceTags=SourceTag;
	EvaluateParameters.TargetTags=TargetTag;
	
	float Vigor=0.f;
	GetCapturedAttributeMagnitude(VigorDef,Spec,EvaluateParameters,Vigor);
	Vigor=FMath::Max<float>(Vigor,0.f);
	
	UObject* SourceObject = Spec.GetContext().GetSourceObject();
	check(SourceObject && SourceObject->Implements<UCombatInterface>());
	const int32 PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(SourceObject);
	
	return 80.f + Vigor * 2.5 + 10.f * PlayerLevel;
}
