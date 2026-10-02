// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/AuraCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "NiagaraComponent.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Data/LevelUpInfo.h"
#include "AuraAbilityTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameMode/AURAGameModeBase.h"
#include "GameMode/AuraGameInstance.h"
#include "GameMode/LoadScreenSaveGame.h"
#include "Input/AuraPlayerController.h"
#include "Input/AuraPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/HUD/AuraHUD.h"
#include "AsyncLoadingScreenLibrary.h"
#include "Tags/AuraGameplayTags.h"

AAuraCharacter::AAuraCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("CameraBoom");
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bDoCollisionTest = false;

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>("TopDownCameraComponent");
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	LevelUpNiagaraComponent=CreateDefaultSubobject<UNiagaraComponent>("LevleuUp");
	LevelUpNiagaraComponent->SetupAttachment(GetRootComponent());
	LevelUpNiagaraComponent->bAutoActivate=false;
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	//调整旋转速度
	GetCharacterMovement()->RotationRate = FRotator(0.0f,400.0f,0.0f);
	//将角色的移动限制在
	GetCharacterMovement()->bConstrainToPlane = true;
	//开始游戏时把角色吸附在平面上
	GetCharacterMovement()->bSnapToPlaneAtStart = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	ConfigureCameraCollision();
	
	CharacterClass=ECharacterClass::Elementalist;
}

void AAuraCharacter::Die()
{
	if (!HasAuthority() || bDead || bDeathReturnRequested) return;

	bDeathReturnRequested = true;
	ResetAbilityVoiceCounters();
	const bool bReturnToMenu = GetNetMode() == NM_Standalone;
	const bool bSaved = bReturnToMenu && TryManualSave(false);
	Super::Die();

	if (!bReturnToMenu)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player death menu travel is only supported in standalone mode"));
		return;
	}

	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("Player death save failed; staying in the current level to avoid losing progress"));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	constexpr float DeathReturnDelay = 2.f;
	const TWeakObjectPtr<UWorld> WeakWorld(World);
	FTimerHandle ReturnToMenuTimer;
	World->GetTimerManager().SetTimer(ReturnToMenuTimer, FTimerDelegate::CreateLambda([WeakWorld]()
	{
		if (UWorld* CurrentWorld = WeakWorld.Get())
		{
			UGameplayStatics::OpenLevel(CurrentWorld, FName(TEXT("/Game/Maps/LoadMenu")));
		}
	}), DeathReturnDelay, false);
}

void AAuraCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 蓝图子类可能保存过碰撞覆盖值，运行时再确保玩家自身不会触发 SpringArm 相机碰撞回缩。
	ConfigureCameraCollision();
}

void AAuraCharacter::ConfigureCameraCollision()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	if (Weapon)
	{
		Weapon->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
}

void AAuraCharacter::LevelUp_Implementation()
{
	MulticastLevelUpParticles();
}


void AAuraCharacter::MulticastLevelUpParticles_Implementation() const
{
	if (IsValid(LevelUpNiagaraComponent))
	{
		const FVector CameraLocation = TopDownCameraComponent->GetComponentLocation();
		const FVector NiagaraSystemLocation = LevelUpNiagaraComponent->GetComponentLocation();
		const FRotator ToCameraRotation = (CameraLocation - NiagaraSystemLocation).Rotation();
		LevelUpNiagaraComponent->SetWorldRotation(ToCameraRotation);
		LevelUpNiagaraComponent->Activate(true);
	}

	if (IsValid(LevelUpSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, LevelUpSound, GetActorLocation());
	}
}

void AAuraCharacter::InitAbilityActorInfo()
{
	AAuraPlayerState*AuraPlayerState=GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	//绑定OnwerActor和AvatarActor
	AuraPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(AuraPlayerState,this);
	UAuraAbilitySystemComponent*AuraAbilitySystemComponent=Cast<UAuraAbilitySystemComponent>(AuraPlayerState->GetAbilitySystemComponent());
	//绑定委托
	AuraAbilitySystemComponent->AbilityActorInfoSet();
	AbilitySystemComponent=AuraPlayerState->GetAbilitySystemComponent();
	AttributeSet=AuraPlayerState->GetAttributeSet();
	
	if (AAuraPlayerController*PlayerController=Cast<AAuraPlayerController>(GetController()))
	{
		//GetHUD内部函数
		if (AAuraHUD*AuraHUD= Cast<AAuraHUD>(PlayerController->GetHUD()))
		{
			AuraHUD->InitOverlay(PlayerController,AuraPlayerState,AttributeSet,AbilitySystemComponent);
		}
	}
	
}
//由于Playstate获得时机不同，所以不能像enemy一样在begin里面直接初始化
void AAuraCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	//服务器初始信息
	InitAbilityActorInfo();
	LoadProgress();
	if (AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(this)))
	{
		AuraGameMode->LoadWorldState(GetWorld());
	}
	UAsyncLoadingScreenLibrary::StopLoadingScreen();
}

void AAuraCharacter::OnRep_PlayerState() 
{
	Super::OnRep_PlayerState();
	//客户端初始信息
	InitAbilityActorInfo();
}

int32 AAuraCharacter::GetPlayerLevel_Implementation()
{
	AAuraPlayerState*AuraPlayerState=GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->GetPlayerLevel();
}

void AAuraCharacter::AddToXP_Implementation(int32 InXP)
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->AddToXP(InXP);
}

int32 AAuraCharacter::GetXP_Implementation() const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->GetXP();
}

int32 AAuraCharacter::GetAttributePoints_Implementation() const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->GetAttributePoints();
}

int32 AAuraCharacter::GetAttributePointsReward_Implementation(int32 InPlayerLevel) const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->LevelUpInfo->LevelUpInformation[InPlayerLevel].AttributePointAward;
}

int32 AAuraCharacter::GetSpellPointsReward_Implementation(int32 InPlayerLevel) const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	return AuraPlayerState->LevelUpInfo->LevelUpInformation[InPlayerLevel].SpellPointAward;
}

int32 AAuraCharacter::FindLevelForXP_Implementation(int32 InXP) const
{
	const AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState && AuraPlayerState->LevelUpInfo);
	return AuraPlayerState->LevelUpInfo->FindLevelForXP(InXP);
}

void AAuraCharacter::AddToPlayerLevel_Implementation(int32 InPlayerLevel)
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->AddToLevel(InPlayerLevel);
	if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		AuraASC->UpdateAbilityStatuses(AuraPlayerState->GetPlayerLevel());
	}
}

void AAuraCharacter::AddToAttributePoints_Implementation(int32 InAttributePoints)
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->AddToAttributePoints(InAttributePoints);
}

void AAuraCharacter::AddToSpellPoints_Implementation(int32 InSpellPoints)
{
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	check(AuraPlayerState);
	AuraPlayerState->AddToSpellPoints(InSpellPoints);
}

void AAuraCharacter::SaveProgress_Implementation(const FName& CheckpointTag)
{
	if (!SaveProgressToSlot(CheckpointTag))
	{
		UE_LOG(LogTemp, Error, TEXT("SaveProgress failed for %s"), *GetName());
	}
}

bool AAuraCharacter::SaveProgressToSlot(const FName& CheckpointTag)
{
	if (!HasAuthority()) return false;

	AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(this));
	AAuraPlayerState* AuraPlayerState = GetPlayerState<AAuraPlayerState>();
	UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent);
	UAbilityInfo* AbilityInfo = UAuraAbilitySystemLibrary::GetAbilityInfo(this);
	if (!IsValid(AbilityInfo))
	{
		UE_LOG(LogTemp, Error, TEXT("SaveProgress on %s requires valid AbilityInfo; player progress was not written"), *GetName());
		return false;
	}
	if (AuraGameMode && AuraPlayerState && AuraASC && GetAttributeSet())
	{
		ULoadScreenSaveGame* SaveData = AuraGameMode->RetrieveInGameSaveData();
		if (SaveData == nullptr) return false;

		TArray<FSavedAbility> SavedAbilities;
		TMap<FGameplayTag, int32> SavedAbilityIndices;
		bool bAbilityDataValid = true;
		const FAuraGameplayTags& SaveTags = FAuraGameplayTags::Get();
		FForEachAbility SaveAbilityDelegate;
		SaveAbilityDelegate.BindLambda([AuraASC, AbilityInfo, SaveData, &SavedAbilities, &SavedAbilityIndices, &bAbilityDataValid, &SaveTags](const FGameplayAbilitySpec& AbilitySpec)
		{
			if (!AbilitySpec.Ability) return;
			const FGameplayTag AbilityTag = AuraASC->GetAbilityTagFromSpec(AbilitySpec);
			if (!AbilityTag.IsValid()) return;
			const FGameplayTag Status = AuraASC->GetStatusFromSpec(AbilitySpec);
			const FAuraAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(AbilityTag);
			if (Info.AbilityTypeTag != SaveTags.Abilities_Type_Offensive && Info.AbilityTypeTag != SaveTags.Abilities_Type_Passive)
			{
				const bool bHasPersistentStatus = Status == SaveTags.Abilities_Status_Eligible ||
					Status == SaveTags.Abilities_Status_Unlocked || Status == SaveTags.Abilities_Status_Equipped;
				const bool bWasSaved = SaveData->SavedAbilities.ContainsByPredicate([&AbilityTag](const FSavedAbility& SavedAbility)
				{
					return SavedAbility.AbilityTag == AbilityTag;
				});
				if (Info.AbilityTag.IsValid() || bHasPersistentStatus || bWasSaved)
				{
					bAbilityDataValid = false;
					UE_LOG(LogTemp, Error, TEXT("Cannot save persistent ability [%s]: AbilityInfo type is missing or unsupported; player progress was not written"), *AbilityTag.ToString());
				}
				// Internal abilities without persistence metadata retain their existing skip behavior.
				return;
			}

			FSavedAbility SavedAbility;
			SavedAbility.GameplayAbility = AbilitySpec.Ability->GetClass();
			SavedAbility.AbilityLevel = AbilitySpec.Level;
			SavedAbility.AbilitySlot = AuraASC->GetInputTagFromSpec(AbilitySpec);
			SavedAbility.AbilityStatus = Status;
			SavedAbility.AbilityTag = AbilityTag;
			SavedAbility.AbilityType = Info.AbilityTypeTag;

			if (const int32* ExistingIndex = SavedAbilityIndices.Find(AbilityTag))
			{
				SavedAbilities[*ExistingIndex] = SavedAbility;
			}
			else
			{
				SavedAbilityIndices.Add(AbilityTag, SavedAbilities.Add(SavedAbility));
			}
		});
		AuraASC->ForEachAbility(SaveAbilityDelegate);
		if (!bAbilityDataValid) return false;

		SaveData->PlayerStartTag = CheckpointTag;
		SaveData->PlayerLevel = AuraPlayerState->GetPlayerLevel();
		SaveData->XP = AuraPlayerState->GetXP();
		SaveData->AttributePoints = AuraPlayerState->GetAttributePoints();
		SaveData->SpellPoints = AuraPlayerState->GetSpellPoints();
		SaveData->Strength = UAuraAttributeSet::GetStrengthAttribute().GetNumericValue(GetAttributeSet());
		SaveData->Intelligence = UAuraAttributeSet::GetIntelligenceAttribute().GetNumericValue(GetAttributeSet());
		SaveData->Resilience = UAuraAttributeSet::GetResilienceAttribute().GetNumericValue(GetAttributeSet());
		SaveData->Vigor = UAuraAttributeSet::GetVigorAttribute().GetNumericValue(GetAttributeSet());
		SaveData->SavedAbilities = MoveTemp(SavedAbilities);

		SaveData->bFirstTimeLoadIn = false;
		return AuraGameMode->SaveInGameProgressData(SaveData);
	}
	return false;
}

void AAuraCharacter::ManualSave()
{
	TryManualSave(true);
}

bool AAuraCharacter::TryManualSave(bool bUsePlayerInterface)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s requires authority"), *GetName());
		return false;
	}

	if (!IsValid(GetWorld()))
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s aborted: world is invalid"), *GetName());
		return false;
	}

	AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(this));
	if (!IsValid(AuraGameMode))
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s aborted: GameMode is not AAURAGameModeBase"), *GetName());
		return false;
	}

	if (!IsValid(GetPlayerState<AAuraPlayerState>()))
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s aborted: PlayerState is missing"), *GetName());
		return false;
	}

	if (!IsValid(Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent)))
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s aborted: Aura ability system component is missing"), *GetName());
		return false;
	}

	if (!IsValid(GetAttributeSet()))
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s aborted: AttributeSet is missing"), *GetName());
		return false;
	}

	const UAuraGameInstance* AuraGameInstance = Cast<UAuraGameInstance>(GetGameInstance());
	if (!IsValid(AuraGameInstance) || AuraGameInstance->LoadSlotName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s aborted: GameInstance has no LoadSlotName"), *GetName());
		return false;
	}

	if (!UGameplayStatics::DoesSaveGameExist(AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s aborted: save slot %s (%d) does not exist"),
			*GetName(), *AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex);
		return false;
	}

	FName PreservedPlayerStartTag;
	{
		// SaveWorldState 会重读并回写槽位。这里只留下已有 PlayerStartTag，不保留 SaveGame 对象。
		ULoadScreenSaveGame* SaveData = AuraGameMode->RetrieveInGameSaveData();
		if (!IsValid(SaveData))
		{
			UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s aborted: RetrieveInGameSaveData returned null for slot %s (%d)"),
				*GetName(), *AuraGameInstance->LoadSlotName, AuraGameInstance->LoadSlotIndex);
			return false;
		}

		PreservedPlayerStartTag = SaveData->PlayerStartTag;
	}

	if (PreservedPlayerStartTag.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("ManualSave on %s is preserving PlayerStartTag None from the existing save"), *GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("ManualSave on %s preserving PlayerStartTag [%s]"),
		*GetName(), *PreservedPlayerStartTag.ToString());

	// Keep the existing interface dispatch for Blueprint callers of ManualSave.
	if (bUsePlayerInterface)
	{
		if (!AuraGameMode->SaveWorldState(GetWorld())) return false;
		IPlayerInterface::Execute_SaveProgress(this, PreservedPlayerStartTag);
		return true;
	}

	// SaveWorldState reloads the slot, then player progress is written to the same slot.
	if (!AuraGameMode->SaveWorldState(GetWorld())) return false;
	return SaveProgressToSlot(PreservedPlayerStartTag);
}

bool AAuraCharacter::SupportsAbilityVoice(const FGameplayTag AbilityTag) const
{
	const FAuraGameplayTags& AbilityTags = FAuraGameplayTags::Get();
	return AbilityTag == AbilityTags.Abilities_Fire_FireBolt ||
		AbilityTag == AbilityTags.Abilities_Lightning_Electrocute ||
		AbilityTag == AbilityTags.Abilities_Fire_DelayedBlast ||
		AbilityTag == AbilityTags.Abilities_Fire_FireNado;
}

void AAuraCharacter::ResetAbilityVoiceCounters()
{
	if (!HasAuthority()) return;
	AbilityVoiceCastCounts.Reset();
	++AbilityVoiceGeneration;
}

void AAuraCharacter::RecordSuccessfulAbilityVoiceCast(const FGameplayTag AbilityTag,
	const int32 EverySuccessfulCasts, const uint64 ExpectedGeneration)
{
	if (!HasAuthority() || bDead || bDeathReturnRequested || IsActorBeingDestroyed() ||
		ExpectedGeneration != AbilityVoiceGeneration || !SupportsAbilityVoice(AbilityTag) ||
		!IsValid(AbilitySystemComponent) || AbilitySystemComponent->GetAvatarActor() != this)
	{
		return;
	}

	if (EverySuccessfulCasts <= 0)
	{
		AbilityVoiceCastCounts.Remove(AbilityTag);
		return;
	}

	int32& Count = AbilityVoiceCastCounts.FindOrAdd(AbilityTag);
	// Compare before incrementing so even MAX_int32 thresholds cannot overflow the counter.
	if (Count >= EverySuccessfulCasts - 1)
	{
		Count = 0;
		ClientNotifyAbilityVoice(AbilityTag);
	}
	else
	{
		++Count;
	}
}

void AAuraCharacter::ClientNotifyAbilityVoice_Implementation(const FGameplayTag AbilityTag)
{
	const APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	if (GetNetMode() == NM_DedicatedServer || bDead || IsActorBeingDestroyed() ||
		!IsLocallyControlled() || !IsValid(PlayerController) || PlayerController->GetPawn() != this ||
		!SupportsAbilityVoice(AbilityTag))
	{
		return;
	}

	OnAbilityVoiceTriggered(AbilityTag);
}

void AAuraCharacter::LoadProgress()
{
	ResetAbilityVoiceCounters();
	AAURAGameModeBase* AuraGameMode = Cast<AAURAGameModeBase>(UGameplayStatics::GetGameMode(this));
	if (AuraGameMode)
	{
		ULoadScreenSaveGame* SaveData = AuraGameMode->RetrieveInGameSaveData();
		if (SaveData == nullptr) return;

		if (SaveData->bFirstTimeLoadIn)
		{
			InitializeDefaultAttributes();
			AddAbilityToCharacter();
		}
		else
		{
			if (AAuraPlayerState* AuraPlayerState = Cast<AAuraPlayerState>(GetPlayerState()))
			{
				AuraPlayerState->SetLevel(SaveData->PlayerLevel);
				AuraPlayerState->SetXP(SaveData->XP);
				AuraPlayerState->SetAttributePoints(SaveData->AttributePoints);
				AuraPlayerState->SetSpellPoints(SaveData->SpellPoints);
			}

			UAuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData(this, AbilitySystemComponent, SaveData);
			if (UAuraAbilitySystemComponent* AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent))
			{
				AuraASC->AddCharacterAbilitiesFromSaveData(SaveData);
				AddAbilityToCharacter();
			}
		}

		// Existing saves predate FireNado. Refresh only this new skill here: a broad
		// UpdateAbilityStatuses call would also grant unrelated omitted abilities.
		if (AAuraPlayerState* const AuraPlayerState = GetPlayerState<AAuraPlayerState>())
		{
			UAuraAbilitySystemComponent* const AuraASC = Cast<UAuraAbilitySystemComponent>(AbilitySystemComponent);
			const UAbilityInfo* const AbilityInfo = UAuraAbilitySystemLibrary::GetAbilityInfo(this);
			const FGameplayTag FireNadoTag = FAuraGameplayTags::Get().Abilities_Fire_FireNado;
			if (IsValid(AuraASC) && IsValid(AbilityInfo) && !AuraASC->GetSpecFromAbilityTag(FireNadoTag))
			{
				const FAuraAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(FireNadoTag);
				if (Info.Ability && AuraPlayerState->GetPlayerLevel() >= Info.LevelRequirement)
				{
					FGameplayAbilitySpec Spec(Info.Ability, 1);
					Spec.GetDynamicSpecSourceTags().AddTag(FAuraGameplayTags::Get().Abilities_Status_Eligible);
					const FGameplayAbilitySpecHandle SpecHandle = AuraASC->GiveAbility(Spec);
					if (FGameplayAbilitySpec* const GrantedSpec = AuraASC->FindAbilitySpecFromHandle(SpecHandle))
					{
						AuraASC->MarkAbilitySpecDirty(*GrantedSpec);
					}
				}
			}
		}
	}
}

