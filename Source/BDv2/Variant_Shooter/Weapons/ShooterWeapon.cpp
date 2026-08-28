// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterWeapon.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PointLightComponent.h"

AShooterWeapon::AShooterWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the first person mesh
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);

	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;
	// First-person arms/weapons must not cast a second world shadow offset from
	// the third-person character shadow.
	FirstPersonMesh->SetCastShadow(false);
	MuzzleFlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Muzzle Flash Light"));
	MuzzleFlashLight->SetupAttachment(FirstPersonMesh);
	MuzzleFlashLight->SetVisibility(false);
	MuzzleFlashLight->SetLightColor(FLinearColor(1.0f, 0.32f, 0.06f));
	MuzzleFlashLight->SetIntensity(5000.0f);
	MuzzleFlashLight->SetAttenuationRadius(350.0f);

	// create the third person mesh
	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);

	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;
}

void AShooterWeapon::BeginPlay()
{
	Super::BeginPlay();
	HipRelativeLocation = GetRootComponent()->GetRelativeLocation();
	HipRelativeRotation = GetRootComponent()->GetRelativeRotation();
	AimRelativeLocation = HipRelativeLocation + FVector(-8.0f, 0.0f, -3.0f);

	// subscribe to the owner's destroyed delegate
	if (!GetOwner())
	{
		return;
	}

	GetOwner()->OnDestroyed.AddDynamic(this, &AShooterWeapon::OnOwnerDestroyed);

	// cast the weapon owner
	WeaponOwner = Cast<IShooterWeaponHolder>(GetOwner());
	PawnOwner = Cast<APawn>(GetOwner());
	if (!WeaponOwner || !PawnOwner)
	{
		UE_LOG(LogTemp, Error, TEXT("ShooterWeapon %s requires a pawn owner implementing ShooterWeaponHolder"), *GetName());
		return;
	}

	// fill the first ammo clip
	CurrentBullets = MagazineSize;

	// attach the meshes to the owner
	WeaponOwner->AttachWeaponMeshes(this);
}

void AShooterWeapon::SetAiming(bool bAiming)
{
	bIsAiming = bAiming;
	if (GetRootComponent())
	{
		GetRootComponent()->SetRelativeLocation(bAiming ? AimRelativeLocation : HipRelativeLocation);
	}
}

void AShooterWeapon::SetSprinting(bool bSprinting)
{
	if (bIsAiming) return;
	const FVector SprintOffset = bSprinting ? FVector(-2.0f, 0.0f, 5.0f) : FVector::ZeroVector;
	const FRotator SprintRotation = bSprinting ? FRotator(-8.0f, 2.0f, 0.0f) : HipRelativeRotation;
	if (GetRootComponent())
	{
		GetRootComponent()->SetRelativeLocation(HipRelativeLocation + SprintOffset);
		GetRootComponent()->SetRelativeRotation(SprintRotation);
	}
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
	GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	// ensure this weapon is destroyed when the owner is destroyed
	Destroy();
}

void AShooterWeapon::ActivateWeapon(const FName& OwnerTag)
{
	// save the owner tag for perception noise detection
	NoiseOwnerTag = OwnerTag;

	// unhide this weapon
	SetActorHiddenInGame(false);

	// notify the owner
	WeaponOwner->OnWeaponActivated(this);
}

void AShooterWeapon::DeactivateWeapon()
{
	// ensure we're no longer firing this weapon while deactivated
	StopFiring();

	// hide the weapon
	SetActorHiddenInGame(true);

	// notify the owner
	WeaponOwner->OnWeaponDeactivated(this);
}

void AShooterWeapon::StartFiring()
{
	if (bIsReloading)
	{
		return;
	}

	if (CurrentBullets <= 0)
	{
		if (ReserveBullets <= 0)
		{
			WeaponOwner->OnWeaponDepleted();
			return;
		}
		Reload();
		return;
	}
	// raise the firing flag
	bIsFiring = true;

	// check how much time has passed since we last shot
	// this may be under the refire rate if the weapon shoots slow enough and the player is spamming the trigger
	const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;

	if (TimeSinceLastShot > RefireRate)
	{
		// fire the weapon right away
		Fire();

	} else {

		// if we're full auto, schedule the next shot
		if (bFullAuto)
		{
			const float RemainingCooldown = FMath::Max(KINDA_SMALL_NUMBER, RefireRate - TimeSinceLastShot);
			GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RemainingCooldown, false);
		}

	}
}

void AShooterWeapon::Reload()
{
	if (bIsReloading || CurrentBullets >= MagazineSize || ReserveBullets <= 0)
	{
		return;
	}

	StopFiring();
	bIsReloading = true;
	OnReloadStarted();
	if (WeaponOwner)
	{
		WeaponOwner->PlayReloadAnimation();
	}
	if (WeaponOwner && ReloadMontage)
	{
		WeaponOwner->PlayFiringMontage(ReloadMontage);
	}
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &AShooterWeapon::FinishReload, ReloadTime, false);
}

void AShooterWeapon::FinishReload()
{
	const int32 RequestedBullets = MagazineSize - CurrentBullets;
	const int32 LoadedBullets = FMath::Min(RequestedBullets, ReserveBullets);
	CurrentBullets += LoadedBullets;
	ReserveBullets -= LoadedBullets;
	bIsReloading = false;
	OnReloadFinished();
	UpdateAmmoHUD();
}

void AShooterWeapon::UpdateAmmoHUD()
{
	if (WeaponOwner)
	{
		WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize, ReserveBullets);
	}
}

void AShooterWeapon::StopFiring()
{
	// lower the firing flag
	bIsFiring = false;

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AShooterWeapon::Fire()
{
	// ensure the player still wants to fire. They may have let go of the trigger
	if (!bIsFiring || bIsReloading)
	{
		return;
	}

	if (CurrentBullets <= 0)
	{
		Reload();
		return;
	}
	
	// fire a projectile at the target
	FireProjectile(WeaponOwner->GetWeaponTargetLocation());

	// update the time of our last shot
	TimeOfLastShot = GetWorld()->GetTimeSeconds();

	// make noise so the AI perception system can hear us
	MakeNoise(ShotLoudness, PawnOwner, PawnOwner->GetActorLocation(), ShotNoiseRange, NoiseOwnerTag);

	// are we full auto?
	if (bFullAuto)
	{
		// schedule the next shot
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, RefireRate, false);
	} else {

		// for semi-auto weapons, schedule the cooldown notification
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, RefireRate, false);

	}
}

void AShooterWeapon::FireCooldownExpired()
{
	// notify the owner
	WeaponOwner->OnSemiWeaponRefire();
}

void AShooterWeapon::FireProjectile(const FVector& TargetLocation)
{
	// get the projectile transform
	FTransform ProjectileTransform = CalculateProjectileSpawnTransform(TargetLocation);
	
	// spawn the projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = PawnOwner;

	AShooterProjectile* Projectile = GetWorld()->SpawnActor<AShooterProjectile>(ProjectileClass, ProjectileTransform, SpawnParams);

	// set the noise tag on the projectile
	if (Projectile)
	{
		Projectile->SetNoiseTag(NoiseOwnerTag);
	}

	PlayFireEffects();
	SpawnEjectedCasing();

	// consume bullets
	--CurrentBullets;
	if (CurrentBullets <= 0 && ReserveBullets <= 0)
	{
		WeaponOwner->UpdateWeaponHUD(0, MagazineSize, 0);
		WeaponOwner->OnWeaponDepleted();
		return;
	}

	// if the magazine is depleted, begin an actual timed reload
	if (CurrentBullets <= 0)
	{
		Reload();
	}

	// update the weapon HUD
	WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize, ReserveBullets);
}

void AShooterWeapon::PlayFireEffects()
{
	if (MuzzleFlashLight)
	{
		const FVector MuzzleLoc = FirstPersonMesh->DoesSocketExist(MuzzleSocketName)
			? FirstPersonMesh->GetSocketLocation(MuzzleSocketName)
			: FirstPersonMesh->GetComponentLocation() + FirstPersonMesh->GetForwardVector() * 45.0f;
		MuzzleFlashLight->SetWorldLocation(MuzzleLoc);
		MuzzleFlashLight->SetVisibility(true);
		GetWorld()->GetTimerManager().ClearTimer(MuzzleFlashTimer);
		GetWorld()->GetTimerManager().SetTimer(MuzzleFlashTimer, [this]()
		{
			if (MuzzleFlashLight) MuzzleFlashLight->SetVisibility(false);
		}, 0.055f, false);
	}
	if (WeaponOwner)
	{
		WeaponOwner->PlayFiringMontage(FiringMontage);
		WeaponOwner->AddWeaponRecoil(FiringRecoil);
	}

	if (FiringSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FiringSound, FirstPersonMesh->GetSocketLocation(MuzzleSocketName));
	}
}

FTransform AShooterWeapon::CalculateProjectileSpawnTransform(const FVector& TargetLocation) const
{
	// find the muzzle location
	const FVector MuzzleLoc = FirstPersonMesh->DoesSocketExist(MuzzleSocketName)
		? FirstPersonMesh->GetSocketLocation(MuzzleSocketName)
		: FirstPersonMesh->GetComponentLocation() + FirstPersonMesh->GetForwardVector() * 45.0f;

	// calculate the spawn location ahead of the muzzle
	const FVector SpawnLoc = MuzzleLoc + ((TargetLocation - MuzzleLoc).GetSafeNormal() * MuzzleOffset);

	// find the aim rotation vector while applying some variance to the target 
	const FRotator AimRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, TargetLocation + (UKismetMathLibrary::RandomUnitVector() * AimVariance));

	// return the built transform
	return FTransform(AimRot, SpawnLoc, FVector::OneVector);
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetFirstPersonAnimInstanceClass() const
{
	return FirstPersonAnimInstanceClass;
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetThirdPersonAnimInstanceClass() const
{
	return ThirdPersonAnimInstanceClass;
}
