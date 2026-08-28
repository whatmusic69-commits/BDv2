// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Weapons/ShooterDroppedWeapon.h"
#include "EnhancedInputComponent.h"
#include "Components/InputComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "ShooterGameMode.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimSequenceBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "InputCoreTypes.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "UObject/UObjectGlobals.h"
#include "UI/ShooterBulletCounterUI.h"

AShooterCharacter::AShooterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UInputAction> SprintInput(TEXT("/Game/BinaryDawn/Input/IA_BD_Sprint.IA_BD_Sprint"));
	if (SprintInput.Succeeded()) SprintAction = SprintInput.Object;
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> ReloadPose(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload.MM_Rifle_Reload"));
	if (ReloadPose.Succeeded())
	{
		ReloadAnimation = ReloadPose.Object;
	}
	// create the noise emitter component
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));

	// configure movement
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
}

void AShooterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (UCameraComponent* Camera = GetFirstPersonCameraComponent())
	{
		const float Speed = GetVelocity().Size2D();
		if (bIsSprinting && Speed > 20.0f && !GetCharacterMovement()->IsFalling())
		{
			Stamina = FMath::Max(0.0f, Stamina - StaminaDrainRate * DeltaSeconds);
			if (Stamina <= 0.0f)
			{
				bSprintExhausted = true;
				StopSprint();
			}
		}
		else if (!bIsSprinting)
		{
			Stamina = FMath::Min(MaxStamina, Stamina + StaminaRecoveryRate * DeltaSeconds);
			if (bSprintExhausted && Stamina >= ExhaustedRecoveryThreshold) bSprintExhausted = false;
		}

		const float TargetFOV = bIsAimingDownSights ? AimFieldOfView : HipFieldOfView;
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, 12.0f));
		const float BobAlpha = (bIsSprinting && Speed > 20.0f && !GetCharacterMovement()->IsFalling()) ? 1.0f : 0.0f;
		CameraBobTime += DeltaSeconds * FMath::Lerp(0.0f, bIsSprinting ? 10.0f : 7.0f, BobAlpha);
		const float AimScale = bIsAimingDownSights ? 0.15f : 1.0f;
		const FVector Bob(0.0f, FMath::Sin(CameraBobTime) * 0.35f * BobAlpha * AimScale, FMath::Abs(FMath::Cos(CameraBobTime)) * 0.45f * BobAlpha * AimScale);
		Camera->SetRelativeLocation(FMath::VInterpTo(Camera->GetRelativeLocation(), CameraBaseRelativeLocation + Bob, DeltaSeconds, 10.0f));

		FHitResult Hit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroppedWeaponAim), true, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Camera->GetComponentLocation(), Camera->GetComponentLocation() + Camera->GetForwardVector() * 500.0f, ECC_Visibility, QueryParams))
		{
			if (AShooterDroppedWeapon* Dropped = Cast<AShooterDroppedWeapon>(Hit.GetActor()))
			{
				NearbyDroppedWeapon = Dropped;
				DrawDebugString(GetWorld(), Dropped->GetVisualLocation() + FVector(0, 0, 35), TEXT("F  -  подобрать"), nullptr, FColor::White, 0.0f, true);
			}
		}

		NearbyDroppedWeapon.Reset();
		for (TActorIterator<AShooterDroppedWeapon> It(GetWorld()); It; ++It)
		{
			if (FVector::DistSquared(It->GetVisualLocation(), GetActorLocation()) <= FMath::Square(250.0f))
			{
				NearbyDroppedWeapon = *It;
				DrawDebugString(GetWorld(), It->GetVisualLocation() + FVector(0, 0, 35), TEXT("F  -  подобрать"), nullptr, FColor::White, 0.0f, true);
				break;
			}
		}
	}
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	// reset HP to max
	CurrentHP = MaxHP;

	// update the HUD
	OnDamaged.Broadcast(1.0f);
	HipFieldOfView = GetFirstPersonCameraComponent()->FieldOfView;
	DefaultFirstPersonAnimClass = GetFirstPersonMesh()->GetAnimClass();
	CameraBaseRelativeLocation = GetFirstPersonCameraComponent()->GetRelativeLocation();
	Stamina = MaxStamina;
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;

	if (DefaultWeaponClass)
	{
		AddWeaponClass(DefaultWeaponClass);
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AShooterCharacter::RefreshWeaponHUD);
	}
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// base class handles move, aim and jump inputs
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Firing
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartFiring);
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopFiring);
		}

		// Switch weapon
		if (SwitchWeaponAction)
		{
			EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Triggered, this, &AShooterCharacter::DoSwitchWeapon);
		}

		if (ReloadAction)
		{
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &AShooterCharacter::DoReload);
		}
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AShooterCharacter::StartSprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AShooterCharacter::StopSprint);
		}
	}

	// Desktop fallbacks keep the weapon usable in the main game's existing IMC.
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AShooterCharacter::DoStartFiring);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AShooterCharacter::DoStopFiring);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AShooterCharacter::StartAimingDownSights);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AShooterCharacter::StopAimingDownSights);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AShooterCharacter::DoReload);
	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &AShooterCharacter::DropCurrentWeapon);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AShooterCharacter::PickUpNearbyWeapon);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AShooterCharacter::StartSprint);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AShooterCharacter::StopSprint);

}

void AShooterCharacter::StartAimingDownSights()
{
	// ADS is only meaningful while a weapon is equipped.
	if (!IsDead() && CurrentWeapon)
	{
		bIsAimingDownSights = true;
		StopSprint();
		CurrentWeapon->SetAiming(true);
	}
}

void AShooterCharacter::StartSprint()
{
	if (IsDead() || bSprintExhausted || bIsAimingDownSights) return;
	bIsSprinting = true;
	if (CurrentWeapon) CurrentWeapon->SetSprinting(true);
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AShooterCharacter::StopSprint()
{
	bIsSprinting = false;
	if (CurrentWeapon) CurrentWeapon->SetSprinting(false);
	if (GetCharacterMovement()) GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
}

void AShooterCharacter::StopAimingDownSights()
{
	bIsAimingDownSights = false;
	if (CurrentWeapon)
	{
		CurrentWeapon->SetAiming(false);
	}
	// The ADS pose is intentionally kept stable here. The imported full-body
	// rifle clip is not additive and causes visible jitter when looped on arms.
}

void AShooterCharacter::PlayReloadAnimation()
{
	if (!ReloadAnimation || IsDead())
	{
		return;
	}

	// Play through an animation slot so the existing first-person AnimBP keeps
	// its hand IK/weapon pose while the reload sequence drives the arms.
	RuntimeReloadMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		ReloadAnimation, FName("Arms"), 0.08f, 0.12f, 1.0f, 1, -1.0f);
	if (RuntimeReloadMontage)
	{
		if (UAnimInstance* FirstPersonAnimInstance = GetFirstPersonMesh()->GetAnimInstance())
		{
			FirstPersonAnimInstance->Montage_Play(RuntimeReloadMontage, 1.0f);
		}
		if (UAnimInstance* ThirdPersonAnimInstance = GetMesh()->GetAnimInstance())
		{
			ThirdPersonAnimInstance->Montage_Play(RuntimeReloadMontage, 1.0f);
		}
	}
	GetWorld()->GetTimerManager().SetTimer(ReloadRestoreTimer, this, &AShooterCharacter::RestoreFirstPersonAnimation, ReloadAnimation->GetPlayLength(), false);
}

void AShooterCharacter::RestoreFirstPersonAnimation()
{
	if (DefaultFirstPersonAnimClass)
	{
		GetFirstPersonMesh()->SetAnimInstanceClass(CurrentWeapon && CurrentWeapon->GetFirstPersonAnimInstanceClass() ? CurrentWeapon->GetFirstPersonAnimInstanceClass() : DefaultFirstPersonAnimClass);
	}
}

void AShooterCharacter::DoReload()
{
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->Reload();
	}
}

void AShooterCharacter::OnWeaponDepleted()
{
	if (CurrentWeapon && !IsDead())
	{
		DropCurrentWeapon();
	}
}

void AShooterCharacter::DropCurrentWeapon()
{
	if (!CurrentWeapon || IsDead() || !GetWorld()) return;
	StopAimingDownSights();

	const TSubclassOf<AShooterWeapon> DroppedClass = CurrentWeapon->GetClass();
	const FVector DropLocation = GetActorLocation() + GetActorForwardVector() * 100.0f + FVector(0, 0, 25.0f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AShooterDroppedWeapon* Dropped = GetWorld()->SpawnActor<AShooterDroppedWeapon>(AShooterDroppedWeapon::StaticClass(), DropLocation, GetActorRotation(), Params))
	{
		Dropped->Initialize(DroppedClass);
		Dropped->ThrowFromCharacter(GetFirstPersonCameraComponent()->GetForwardVector());
		OwnedWeapons.Remove(CurrentWeapon);
		CurrentWeapon->Destroy();
		CurrentWeapon = nullptr;
	if (UClass* UnarmedClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")))
		{
			GetFirstPersonMesh()->SetAnimInstanceClass(UnarmedClass);
			// The third-person mesh is what contributes the character's shadow.
			// Switch it to the unarmed graph as well so the rifle pose cannot remain
			// baked into the shadow after dropping the weapon.
			GetMesh()->SetAnimInstanceClass(UnarmedClass);
		}
		UpdateWeaponHUD(0, 0, 0);
	}
}

void AShooterCharacter::PickUpNearbyWeapon()
{
	if (NearbyDroppedWeapon.IsValid() && !IsDead())
	{
		NearbyDroppedWeapon->TryPickup(this);
	}
}

void AShooterCharacter::SetNearbyDroppedWeapon(AShooterDroppedWeapon* DroppedWeapon)
{
	NearbyDroppedWeapon = DroppedWeapon;
}

void AShooterCharacter::PickUpDroppedWeapon(AShooterDroppedWeapon* DroppedWeapon)
{
	if (!DroppedWeapon || !DroppedWeapon->GetWeaponClass()) return;
	AddWeaponClass(DroppedWeapon->GetWeaponClass());
	NearbyDroppedWeapon.Reset();
	DroppedWeapon->Destroy();
}

void AShooterCharacter::RefreshWeaponHUD()
{
	if (CurrentWeapon)
	{
		OnBulletCountUpdated.Broadcast(
			CurrentWeapon->GetMagazineSize(),
			CurrentWeapon->GetBulletCount(),
			CurrentWeapon->GetReserveBulletCount());
	}
}

float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// ignore if already dead
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// Reduce HP
	CurrentHP -= Damage;

	// Have we depleted HP?
	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	// update the HUD
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));

	return Damage;
}

void AShooterCharacter::DoAim(float Yaw, float Pitch)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoAim(Yaw, Pitch);
	}
}

void AShooterCharacter::DoMove(float Right, float Forward)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoMove(Right, Forward);
	}
}

void AShooterCharacter::DoJumpStart()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpStart();
	}
}

void AShooterCharacter::DoJumpEnd()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpEnd();
	}
}

void AShooterCharacter::DoStartFiring()
{
	// fire the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StartFiring();
	}
}

void AShooterCharacter::DoStopFiring()
{
	// stop firing the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StopFiring();
	}
}

void AShooterCharacter::DoSwitchWeapon()
{
	// ensure we have at least two weapons two switch between
	if (OwnedWeapons.Num() > 1 && !IsDead())
	{
		// deactivate the old weapon
		CurrentWeapon->DeactivateWeapon();

		// find the index of the current weapon in the owned list
		int32 WeaponIndex = OwnedWeapons.Find(CurrentWeapon);

		// is this the last weapon?
		if (WeaponIndex == OwnedWeapons.Num() - 1)
		{
			// loop back to the beginning of the array
			WeaponIndex = 0;
		}
		else {
			// select the next weapon index
			++WeaponIndex;
		}

		// set the new weapon as current
		CurrentWeapon = OwnedWeapons[WeaponIndex];

		// activate the new weapon
		CurrentWeapon->ActivateWeapon(PlayerTag);
	}
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	Weapon->AttachToActor(this, AttachmentRule);

	// attach the weapon meshes
	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonWeaponSocket);
	
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}

	if (UAnimInstance* FirstPersonAnimInstance = GetFirstPersonMesh()->GetAnimInstance())
	{
		FirstPersonAnimInstance->Montage_Play(Montage);
	}

	if (UAnimInstance* ThirdPersonAnimInstance = GetMesh()->GetAnimInstance())
	{
		ThirdPersonAnimInstance->Montage_Play(Montage);
	}
}

void AShooterCharacter::AddWeaponRecoil(float Recoil)
{
	// apply the recoil as pitch input
	AddControllerPitchInput(Recoil);
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize, int32 ReserveAmmo)
{
	OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo, ReserveAmmo);

	TArray<UUserWidget*> HUDWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, HUDWidgets, UShooterBulletCounterUI::StaticClass(), false);
	for (UUserWidget* Widget : HUDWidgets)
	{
		if (UShooterBulletCounterUI* ShooterHUD = Cast<UShooterBulletCounterUI>(Widget))
		{
			ShooterHUD->UpdateAmmo(CurrentAmmo, ReserveAmmo);
		}
	}
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
	// trace ahead from the camera viewpoint
	FHitResult OutHit;

	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector End = Start + (GetFirstPersonCameraComponent()->GetForwardVector() * MaxAimDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	// do we already own this weapon?
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(WeaponClass);

	if (!OwnedWeapon)
	{
		// spawn the new weapon
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

		AShooterWeapon* AddedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

		if (AddedWeapon)
		{
			// add the weapon to the owned list
			OwnedWeapons.Add(AddedWeapon);

			// if we have an existing weapon, deactivate it
			if (CurrentWeapon)
			{
				CurrentWeapon->DeactivateWeapon();
			}

			// switch to the new weapon
			CurrentWeapon = AddedWeapon;
			CurrentWeapon->ActivateWeapon(PlayerTag);
		}
	}
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	// update the bullet counter
	OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount(), Weapon->GetReserveBulletCount());

	// set the character mesh AnimInstances
	if (Weapon->GetFirstPersonAnimInstanceClass())
	{
		GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
	}
	if (Weapon->GetThirdPersonAnimInstanceClass())
	{
		GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
	}
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	// unused
}

void AShooterCharacter::OnSemiWeaponRefire()
{
	// unused
}

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
	// check each owned weapon
	for (AShooterWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}

	// weapon not found
	return nullptr;

}

void AShooterCharacter::Die()
{
	// deactivate the weapon
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// increment the team score
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->IncrementTeamScore(TeamByte);
	}

	// grant the death tag to the character
	Tags.Add(DeathTag);
		
	// stop character movement
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// disable controls
	DisableInput(nullptr);

	// reset the bullet counter UI
	OnBulletCountUpdated.Broadcast(0, 0, 0);

	// call the BP handler
	BP_OnDeath();

	// schedule character respawn
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterCharacter::OnRespawn, RespawnTime, false);
}

void AShooterCharacter::OnRespawn()
{
	// destroy the character to force the PC to respawn
	Destroy();
}

bool AShooterCharacter::IsDead() const
{
	// the character is dead if their current HP drops to zero
	return CurrentHP <= 0.0f;
}

void AShooterCharacter::SetTeam(uint8 Team)
{
	TeamByte = Team;
}
