// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "TimerManager.h"
#include "UObject/Interface.h"

class AActor;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

#include "AuraOcclusionFadeComponent.generated.h"

/** 源材质没有 Fade 标量时，改用淡出材质创建动态材质。 */
USTRUCT(BlueprintType)
struct FAuraOcclusionFadeMaterialMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	TObjectPtr<UMaterialInterface> SourceMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	TObjectPtr<UMaterialInterface> FadeMaterial = nullptr;
};

/** 一个静态网格材质槽的运行时淡出状态。只存在于发起淡出的本地玩家组件上。 */
USTRUCT()
struct FAuraOcclusionFadeSlotState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(Transient)
	int32 MaterialSlot = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FadeMID = nullptr;

	/** 命中的带标签 Actor。网格可能在它的 Child Actor 上。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> OccluderActor;

	UPROPERTY(Transient)
	float CurrentFade = 1.f;

	UPROPERTY(Transient)
	float TargetFade = 1.f;

	UPROPERTY(Transient)
	bool bControlledByComponent = false;
};

/** 一次淡出期间被改为 ECR_Ignore 的网格及其 ECC_Visibility 原响应，按组件而不是按材质槽缓存。 */
USTRUCT()
struct FAuraOcclusionFadeMeshVisibilityState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<UStaticMeshComponent> MeshComponent;

	/** 触发这次淡出的带标签 Actor；该 Actor 的槽位全部释放后才恢复。 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> OccluderActor;

	UPROPERTY(Transient)
	TEnumAsByte<ECollisionResponse> OriginalResponse = ECR_Block;
};

/**
 * 本地玩家遮挡淡出。每个本地控制的 Pawn 一份，不挂在遮挡物上。
 * 查询带 FadeOccluder Actor 标签的物体并渐变 Fade；实现 ExcludedInterface 的 Actor 不进入这条路径。
 * 不复制，不改碰撞开关、导航、Hidden 或存档。淡出期间只把该网格的 ECC_Visibility 响应临时改为 ECR_Ignore，
 * 使点击移动的 Visibility 光标追踪不再命中遮挡物；该 Actor 全部槽位走完可见后按缓存原值恢复。
 */
UCLASS(ClassGroup=(Aura), meta=(BlueprintSpawnableComponent))
class AURA_API UAuraOcclusionFadeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraOcclusionFadeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginDestroy() override;

	/** 角色上已有检测盒的组件名；默认使用 BP_AuraCharacter 的 CollBox。设为 None 才使用下方旧引用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	FName DetectionBoxComponentName = TEXT("CollBox");

	/** 旧的直接组件引用；填写了 DetectionBoxComponentName 时，运行时会解析并覆盖它。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	TObjectPtr<UBoxComponent> DetectionBox = nullptr;

	/** 稍后指定 BI_FadeInterface。实现该接口的 Actor 继续走旧淡出，不由本组件改材质。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	TSubclassOf<UInterface> ExcludedInterface = nullptr;

	/** 重叠查询间隔（秒）。小于等于 0 时按 0.08 秒查询，避免每帧重扫。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade", meta = (ClampMin = "0.0"))
	float QueryInterval = 0.08f;

	/** 走完可见与淡出差值所需的时间（秒）。小于等于 0 时立即到达目标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade", meta = (ClampMin = "0.0"))
	float FadeDuration = 0.4f;

	/** AActor::Tags 里的普通 Actor 标签，不是 GameplayTag。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	FName OccluderTag = TEXT("FadeOccluder");

	/** 材质标量参数名。现有材质用 Fade 驱动 DitherTemporalAA。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	FName FadeParameterName = TEXT("Fade");

	/** 完全可见。现有 Fade 材质以 1 为可见。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	float VisibleFade = 1.f;

	/** 被遮挡时的 Fade。现有材质以 0 为淡出。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	float OccludedFade = 0.f;

	/** 为真时除 WorldStatic 外再查询 WorldDynamic。不修改目标碰撞。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	bool bIncludeWorldDynamic = false;

	/** 沿父链找不到 Fade 标量时，按源材质查找淡出材质。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion Fade")
	TArray<FAuraOcclusionFadeMaterialMapping> MaterialMappings;

private:
	bool IsDedicatedServerWorld() const;
	bool CanRunLocalFade() const;
	bool ImplementsExcludedInterface(const AActor* Actor) const;
	bool IsActorHiddenForFade(const AActor* Actor) const;
	struct FDesiredOcclusionSlot
	{
		UStaticMeshComponent* Mesh = nullptr;
		int32 SlotIndex = INDEX_NONE;
		AActor* Occluder = nullptr;
	};

	bool MaterialChainHasFadeScalar(const UMaterialInterface* Material) const;
	UMaterialInterface* FindMappedFadeMaterial(const UMaterialInterface* Material) const;
	FString GetMaterialWarningKey(const UMaterialInterface* Material) const;
	void WarnUnmappedMaterialOnce(const UMaterialInterface* Material);
	void WarnSlotReplacedOnce(const UStaticMeshComponent* Mesh, const FAuraOcclusionFadeSlotState& State);
	void CollectStaticMeshes(AActor* Actor, TArray<UStaticMeshComponent*>& OutMeshes, TSet<const AActor*>& VisitedActors) const;
	void HandleOcclusionQuery();
	void GatherDesiredSlots(TArray<FDesiredOcclusionSlot>& OutSlots) const;
	void SynchronizeSlots(const TArray<FDesiredOcclusionSlot>& DesiredSlots);
	void UpdateTransitions(float DeltaTime);
	bool IsMeshSlotUsable(const UStaticMeshComponent* Mesh, int32 MaterialSlot) const;
	bool SlotStillHasOurMID(const UStaticMeshComponent* Mesh, const FAuraOcclusionFadeSlotState& State) const;
	void ApplyFadeValue(const FAuraOcclusionFadeSlotState& State) const;
	void RestoreControlledSlot(FAuraOcclusionFadeSlotState& State);
	void RestoreAllControlledSlots();
	int32 FindSlotStateIndex(const UStaticMeshComponent* Mesh, int32 MaterialSlot) const;
	bool TryBeginSlot(UStaticMeshComponent* Mesh, int32 MaterialSlot, AActor* Occluder);
	void RequestFadeTick();

	/** 首次真正淡出该网格时缓存 ECC_Visibility 原响应并改为 Ignore；按组件缓存，不按材质槽重复。 */
	void CacheMeshVisibilityResponseIfNeeded(UStaticMeshComponent* Mesh, AActor* Occluder);
	/** 该 Actor 上仍有本组件控制的槽（淡出中或尚未回到 VisibleFade）时返回 true，用于延后 Visibility 恢复。 */
	bool IsActorStillFadeControlled(const AActor* Actor) const;
	/** 该 Actor 上所有槽位走完可见并释放后，按缓存值恢复对应网格的 Visibility 响应；释放后自动重试。 */
	void RefreshPendingVisibilityRestores();
	/** EndPlay、组件销毁、丢失本地控制、Actor 隐藏或网格失效时，不加条件地恢复全部缓存，避免永久 Ignore。 */
	void RestoreAllMeshVisibilityResponses();

	UPROPERTY(Transient)
	TArray<FAuraOcclusionFadeSlotState> SlotStates;

	/** 淡出期间已改为 ECR_Ignore 的网格及其 ECC_Visibility 原响应；不参与复制。 */
	UPROPERTY(Transient)
	TArray<FAuraOcclusionFadeMeshVisibilityState> PendingVisibilityRestores;

	TSet<FString> WarnedMissingMaterialKeys;
	TSet<FString> WarnedReplacedSlotKeys;
	FTimerHandle QueryTimerHandle;
};
