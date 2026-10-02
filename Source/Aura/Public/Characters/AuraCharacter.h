// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/AuraCharacterBase.h"
#include "Interfaction/PlayerInterface.h"
#include "GameplayTagContainer.h"
#include "AuraCharacter.generated.h"

class UCameraComponent;
class UNiagaraComponent;
class USpringArmComponent;
class USoundBase;
/**
 * 
 */
UCLASS()
class AURA_API AAuraCharacter : public AAuraCharacterBase, public IPlayerInterface
{
	GENERATED_BODY()
public:
	AAuraCharacter();
	virtual void Die() override;
	
	//判断该Pawn的拥有者,服务器
	virtual void PossessedBy(AController* NewController) override;
	//判断拥有控制器，客户端
	virtual void OnRep_PlayerState() override;
	
	virtual int32 GetPlayerLevel_Implementation() override;
	virtual void AddToXP_Implementation(int32 InXP) override;
	virtual int32 GetXP_Implementation() const override;
	virtual int32 GetAttributePoints_Implementation() const override;
	virtual int32 GetAttributePointsReward_Implementation(int32 InPlayerLevel) const override;
	virtual int32 GetSpellPointsReward_Implementation(int32 InPlayerLevel) const override;
	virtual int32 FindLevelForXP_Implementation(int32 InXP) const override;
	virtual void AddToPlayerLevel_Implementation(int32 InPlayerLevel) override;
	virtual void AddToAttributePoints_Implementation(int32 InAttributePoints) override;
	virtual void AddToSpellPoints_Implementation(int32 InSpellPoints) override;
	virtual void LevelUp_Implementation() override;
	virtual void SaveProgress_Implementation(const FName& CheckpointTag) override;

	UFUNCTION(BlueprintCallable, Category = "Save")
	void ManualSave();

	void LoadProgress();

	bool SupportsAbilityVoice(FGameplayTag AbilityTag) const;
	uint64 GetAbilityVoiceGeneration() const { return AbilityVoiceGeneration; }
	void RecordSuccessfulAbilityVoiceCast(FGameplayTag AbilityTag, int32 EverySuccessfulCasts, uint64 ExpectedGeneration);

	/** Runs only on the local owning player when this skill reaches its configured cast threshold. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Voice")
	void OnAbilityVoiceTriggered(FGameplayTag AbilityTag);
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	TObjectPtr<UNiagaraComponent>LevelUpNiagaraComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Level Up")
	TObjectPtr<USoundBase> LevelUpSound;
protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo()override;
private:
	// Server-only, character-lifetime state; deliberately excluded from replication and SaveGame.
	TMap<FGameplayTag, int32> AbilityVoiceCastCounts;
	uint64 AbilityVoiceGeneration = 1;
	void ResetAbilityVoiceCounters();

	UFUNCTION(Client, Reliable)
	void ClientNotifyAbilityVoice(FGameplayTag AbilityTag);

	//======== 内部逻辑 ========
	void ConfigureCameraCollision();
	bool SaveProgressToSlot(const FName& CheckpointTag);
	bool TryManualSave(bool bUsePlayerInterface);
	bool bDeathReturnRequested = false;

	UFUNCTION(NetMulticast,Reliable)
	void MulticastLevelUpParticles()const;

	//======== 相机 ========
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> TopDownCameraComponent;
};
