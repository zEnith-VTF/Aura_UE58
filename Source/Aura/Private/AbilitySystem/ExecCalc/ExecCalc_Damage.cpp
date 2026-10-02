// 在项目设置的描述页面填写版权声明。


#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"

#include "AbilitySystemComponent.h"
#include "AuraAbilityTypes.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Engine/CurveTable.h"
#include "Interfaction/CombatInterface.h"
#include "Tags/AuraGameplayTags.h"

struct AuraDamageStatics
{
	// 先声明本次伤害计算需要捕获的属性。
	DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
	DECLARE_ATTRIBUTE_CAPTUREDEF(ArmorPenetration);
	DECLARE_ATTRIBUTE_CAPTUREDEF(BlockChance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalHitChance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalHitDamage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(CriticalHitResistance);

	DECLARE_ATTRIBUTE_CAPTUREDEF(FireResistance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(LightningResistance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(ArcaneResistance);
	DECLARE_ATTRIBUTE_CAPTUREDEF(PhysicalResistance);

	AuraDamageStatics()
	{
		// 真正定义捕获规则：防御属性来自目标，攻击属性来自伤害来源。
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, Armor, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, ArmorPenetration, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, BlockChance, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, CriticalHitChance, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, CriticalHitDamage, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, CriticalHitResistance, Target, false);

		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, FireResistance, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, LightningResistance, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, ArcaneResistance, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UAuraAttributeSet, PhysicalResistance, Target, false);
	}

	const FGameplayEffectAttributeCaptureDefinition* FindResistanceCaptureDef(const FGameplayTag& ResistanceTag) const
	{
		// 只有真正执行伤害时才会首次创建这张表；此时 DamageTypesToResistances 已经完成初始化，
		// 因而不会像在 ExecCalc CDO 构造阶段那样把无效 GameplayTag 缓存成 Key。
		const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
		static const TMap<FGameplayTag, FGameplayEffectAttributeCaptureDefinition> ResistanceCaptureDefs =
		{
			{ Tags.Attributes_Resistance_Fire, FireResistanceDef },
			{ Tags.Attributes_Resistance_Lightning, LightningResistanceDef },
			{ Tags.Attributes_Resistance_Arcane, ArcaneResistanceDef },
			{ Tags.Attributes_Resistance_Physical, PhysicalResistanceDef }
		};

		return ResistanceCaptureDefs.Find(ResistanceTag);
	}
};

static const AuraDamageStatics& DamageStatics()
{
	static AuraDamageStatics DStatics;
	return DStatics;
}

static bool RollPercentage(const float Chance)
{
	const float ClampedChance = FMath::Clamp(Chance, 0.f, 100.f);
	return FMath::FRand() * 100.f < ClampedChance;
}

UExecCalc_Damage::UExecCalc_Damage()
{
	// 告诉 GAS：这个 ExecCalc_Damage 执行计算时，需要提前准备好当前属性的数据。
	// ArmorDef 代表捕获 Armor 的规则。
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
	RelevantAttributesToCapture.Add(DamageStatics().BlockChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArmorPenetrationDef);
	RelevantAttributesToCapture.Add(DamageStatics().CriticalHitChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().CriticalHitDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().CriticalHitResistanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().FireResistanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().LightningResistanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArcaneResistanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().PhysicalResistanceDef);
}

void UExecCalc_Damage::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

	AActor* SourceAvatar = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	AActor* TargetAvatar = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	if (!IsValid(SourceAvatar) || !IsValid(TargetAvatar)) return;

	// 最终伤害入口保险：即使上层漏掉目标过滤，同阵营也不会产生 IncomingDamage。
	if (UAuraAbilitySystemLibrary::AreActorsFriends(SourceAvatar, TargetAvatar)) return;

	if (!ensureMsgf(SourceAvatar->Implements<UCombatInterface>() && TargetAvatar->Implements<UCombatInterface>(),
		TEXT("ExecCalc_Damage requires source [%s] and target [%s] to implement CombatInterface"),
		*GetNameSafe(SourceAvatar), *GetNameSafe(TargetAvatar)))
	{
		return;
	}

	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// 先一次性验证所有配置依赖，失败时不消费随机数，也不写入部分命中状态。
	const UCharacterClassInfo* CharacterClassInfo =
		UAuraAbilitySystemLibrary::GetCharacterClassInfo(SourceAvatar);
	if (!ensureMsgf(IsValid(CharacterClassInfo), TEXT("ExecCalc_Damage cannot find CharacterClassInfo")) ||
		!ensureMsgf(IsValid(CharacterClassInfo->DamageCalculationCoefficient),
			TEXT("DamageCalculationCoefficient is not configured on CharacterClassInfo")))
	{
		return;
	}

	static const FName ArmorPenetrationCurveName(TEXT("ArmorPenetration"));
	static const FName EffectiveArmorCurveName(TEXT("EffectiveArmor"));
	static const FName CriticalHitResistanceCurveName(TEXT("CriticalHitResistance"));
	const FString CurveLookupContext(TEXT("ExecCalc_Damage"));
	const FRealCurve* ArmorPenetrationCurve = CharacterClassInfo->DamageCalculationCoefficient->FindCurve(
		ArmorPenetrationCurveName, CurveLookupContext);
	const FRealCurve* EffectiveArmorCurve = CharacterClassInfo->DamageCalculationCoefficient->FindCurve(
		EffectiveArmorCurveName, CurveLookupContext);
	const FRealCurve* CriticalHitResistanceCurve = CharacterClassInfo->DamageCalculationCoefficient->FindCurve(
		CriticalHitResistanceCurveName, CurveLookupContext);

	if (!ensureMsgf(ArmorPenetrationCurve, TEXT("Missing ArmorPenetration damage coefficient curve")) ||
		!ensureMsgf(EffectiveArmorCurve, TEXT("Missing EffectiveArmor damage coefficient curve")) ||
		!ensureMsgf(CriticalHitResistanceCurve, TEXT("Missing CriticalHitResistance damage coefficient curve")))
	{
		return;
	}

	// 创建聚合器评估参数，让属性捕获计算时可以读取来源和目标身上的 GameplayTag。
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	// 从 SetByCaller 中读取每一种伤害类型，累加得到本次攻击的基础总伤害。
	float Damage = 0.f;
	for (const TPair<FGameplayTag, FGameplayTag>& DamageTypeToResistancePair :
		FAuraGameplayTags::Get().DamageTypesToResistances)
	{
		const FGameplayTag& DamageTypeTag = DamageTypeToResistancePair.Key;
		const FGameplayTag& ResistanceTag = DamageTypeToResistancePair.Value;

		const FGameplayEffectAttributeCaptureDefinition* ResistanceCaptureDef =
			DamageStatics().FindResistanceCaptureDef(ResistanceTag);
		if (!ensureMsgf(
			ResistanceCaptureDef,
			TEXT("No capture definition registered for resistance tag [%s] in ExecCalc_Damage"),
			*ResistanceTag.ToString()))
		{
			return;
		}

		// 某些技能只携带部分伤害类型，未设置的 SetByCaller 按0 处理且不输出警告。
		float DamageTypeValue = Spec.GetSetByCallerMagnitude(DamageTypeTag, false, 0.f);

		float Resistance = 0.f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
			*ResistanceCaptureDef,
			EvaluationParameters,
			Resistance);
		Resistance = FMath::Clamp(Resistance, 0.f, 100.f);

		// 每一种伤害只由它匹配的抗性减免。
		DamageTypeValue *= (100.f - Resistance) / 100.f;
		Damage += DamageTypeValue;
	}

	// 捕获目标的格挡几率，并随机判断本次攻击是否被格挡。
	// 如果格挡成功，本次伤害减半。
	float TargetBlockChance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().BlockChanceDef, EvaluationParameters, TargetBlockChance);
	TargetBlockChance = FMath::Clamp(TargetBlockChance, 0.f, 100.f);

	const bool bBlocked = RollPercentage(TargetBlockChance);
	
	FGameplayEffectContextHandle EffectContextHandle= Spec.GetContext();
	UAuraAbilitySystemLibrary::SetIsBlockedHit(EffectContextHandle,bBlocked);
	
	Damage = bBlocked ? Damage / 2.f : Damage;

	// 捕获目标护甲。
	float TargetArmor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvaluationParameters, TargetArmor);
	TargetArmor = FMath::Max<float>(TargetArmor, 0.f);

	// 捕获伤害来源的护甲穿透。
	float SourceArmorPenetration = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorPenetrationDef, EvaluationParameters, SourceArmorPenetration);
	SourceArmorPenetration = FMath::Max<float>(SourceArmorPenetration, 0.f);

	const float ArmorPenetrationCoefficient = ArmorPenetrationCurve->Eval(
		ICombatInterface::Execute_GetPlayerLevel(SourceAvatar));

	// 护甲穿透会忽略目标一定比例的护甲。
	const float EffectiveArmor = TargetArmor * (100.f - SourceArmorPenetration * ArmorPenetrationCoefficient) / 100.f;

	const float EffectiveArmorCoefficient = EffectiveArmorCurve->Eval(
		ICombatInterface::Execute_GetPlayerLevel(TargetAvatar));

	// 有效护甲会抵消一定比例的最终伤害。
	Damage *= (100.f - EffectiveArmor * EffectiveArmorCoefficient) / 100.f;

	// 捕获暴击相关属性：来源提供暴击率和暴击伤害，目标提供暴击抗性。
	float SourceCriticalHitChance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CriticalHitChanceDef, EvaluationParameters, SourceCriticalHitChance);
	SourceCriticalHitChance = FMath::Max<float>(SourceCriticalHitChance, 0.f);

	float SourceCriticalHitDamage = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CriticalHitDamageDef, EvaluationParameters, SourceCriticalHitDamage);
	SourceCriticalHitDamage = FMath::Max<float>(SourceCriticalHitDamage, 0.f);

	float TargetCriticalHitResistance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CriticalHitResistanceDef, EvaluationParameters, TargetCriticalHitResistance);
	TargetCriticalHitResistance = FMath::Max<float>(TargetCriticalHitResistance, 0.f);

	const float CriticalHitResistanceCoefficient = CriticalHitResistanceCurve->Eval(
		ICombatInterface::Execute_GetPlayerLevel(TargetAvatar));

	// 暴击抗性会降低本次攻击实际触发暴击的概率。
	const float EffectiveCriticalHitChance = FMath::Clamp(
		SourceCriticalHitChance - TargetCriticalHitResistance * CriticalHitResistanceCoefficient,
		0.f,
		100.f);
	const bool bCriticalHit = RollPercentage(EffectiveCriticalHitChance);
	
	UAuraAbilitySystemLibrary::SetIsCriticalHit(EffectContextHandle,bCriticalHit);
	// 暴击成功时：先翻倍当前已经结算过防御的伤害，再加上来源的额外暴击伤害。
	Damage = bCriticalHit ? 2.f * Damage + SourceCriticalHitDamage : Damage;
	Damage = FMath::Max(Damage, 0.f);

	const FGameplayModifierEvaluatedData EvaluatedData(UAuraAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage);
	OutExecutionOutput.AddOutputModifier(EvaluatedData);
}
