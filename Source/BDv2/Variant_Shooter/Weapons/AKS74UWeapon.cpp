#include "AKS74UWeapon.h"

#include "AKS74UProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "ShooterEjectedCasing.h"
#include "Kismet/GameplayStatics.h"
#include "Variant_Shooter/ShooterCharacter.h"

namespace
{
	static constexpr const TCHAR* BodyMaterialPath = TEXT("/Game/Weapons/AKS74U/Materials/M_AKS74U_Body.M_AKS74U_Body");
	static constexpr const TCHAR* MagazineMaterialPath = TEXT("/Game/Weapons/AKS74U/Materials/M_AKS74U_Magazine.M_AKS74U_Magazine");
}

AAKS74UWeapon::AAKS74UWeapon()
{
	// The imported FBX is split into its original parts. Keep the magazine as a
	// real component so it can detach and fall during the reload animation.
	static const TCHAR* PartNames[] = { TEXT("receiver"), TEXT("hand_guard_top"), TEXT("bolt_carrier"), TEXT("pistol_grip"), TEXT("barrel"), TEXT("dust_cover_rear_sight"), TEXT("trigger"), TEXT("safety_switch_lever"), TEXT("stock"), TEXT("hand_guard_bottom") };
	for (const TCHAR* PartName : PartNames)
	{
		const FName ComponentName = FName(FString::Printf(TEXT("AKS74U_%s"), PartName));
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
		Part->SetupAttachment(GetFirstPersonMesh());
		Part->SetRelativeLocation(FVector(5.0f, -8.0f, 8.0f));
		Part->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		Part->SetRelativeScale3D(FVector(0.14f));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		const FString MeshPath = FString::Printf(TEXT("/Game/Weapons/AKS74U/Split/AKS74U_Parts_%s.AKS74U_Parts_%s"), PartName, PartName);
		if (UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *MeshPath)))
		{
			Part->SetStaticMesh(Mesh);
			if (UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, BodyMaterialPath)))
			{
				Part->SetMaterial(0, Mat);
			}
		}
	}

	MagazineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AKS74U_Magazine"));
	MagazineMesh->SetupAttachment(GetFirstPersonMesh());
	MagazineRelativeTransform = FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(5.0f, -8.0f, 8.0f), FVector(0.14f));
	MagazineMesh->SetRelativeTransform(MagazineRelativeTransform);
	MagazineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MagazineMesh->SetCastShadow(false);
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/Weapons/AKS74U/Split/AKS74U_Parts_magazine.AKS74U_Parts_magazine"))))
	{
		MagazineMesh->SetStaticMesh(Mesh);
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, MagazineMaterialPath)))
		{
			MagazineMesh->SetMaterial(0, Mat);
		}
	}
	MagazineInHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AKS74U_Magazine_InHand"));
	MagazineInHandMesh->SetupAttachment(GetFirstPersonMesh());
	MagazineInHandMesh->SetRelativeTransform(MagazineRelativeTransform);
	MagazineInHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MagazineInHandMesh->SetCastShadow(false);
	MagazineInHandMesh->SetVisibility(false);
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/Weapons/AKS74U/Split/AKS74U_Parts_magazine.AKS74U_Parts_magazine"))))
	{
		MagazineInHandMesh->SetStaticMesh(Mesh);
		if (UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, MagazineMaterialPath)))
		{
			MagazineInHandMesh->SetMaterial(0, Mat);
		}
	}
	ProjectileClass = AAKS74UProjectile::StaticClass();
	MagazineSize = 30;
	ReserveBullets = 90;
	ReloadTime = 2.4f;
	bFullAuto = true;
	RefireRate = 0.1f; // 600 rounds per minute
	AimVariance = 1.25f;
	FiringRecoil = -0.12f;
	MuzzleSocketName = TEXT("Muzzle");
	MuzzleOffset = 12.0f;
	ShotLoudness = 1.0f;
	ShotNoiseRange = 3500.0f;
}

void AAKS74UWeapon::BeginPlay()
{
	Super::BeginPlay();
	// The old combined mesh remains in the Blueprint for compatibility; hide it
	// now that the FBX parts are rendered independently.
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetAttachParent() == GetFirstPersonMesh())
		{
			Mesh->SetCastShadow(false);
		}
		if (Mesh && Mesh != MagazineMesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetPathName().Contains(TEXT("SM_AKS74U")))
		{
			Mesh->SetVisibility(false, true);
		}
	}
}

void AAKS74UWeapon::OnReloadStarted()
{
	if (!MagazineMesh || !MagazineMesh->IsVisible() || !GetWorld()) return;
	// Let the hands reach the magazine before it is released. The rifle reload
	// clip is 2.2 seconds long: extraction happens around 0.5 s and insertion
	// around 1.8 s, leaving the final motion for the charging handle.
	GetWorld()->GetTimerManager().SetTimer(MagazineDetachTimer, this, &AAKS74UWeapon::DetachMagazineNow, 0.5f, false);
	GetWorld()->GetTimerManager().SetTimer(MagazineThrowTimer, this, &AAKS74UWeapon::ThrowMagazineFromHand, 1.05f, false);
	GetWorld()->GetTimerManager().SetTimer(MagazineHandTimer, this, &AAKS74UWeapon::ShowReplacementMagazineInHand, 1.2f, false);
	GetWorld()->GetTimerManager().SetTimer(MagazineInsertTimer, this, &AAKS74UWeapon::InsertMagazineNow, FMath::Max(0.5f, ReloadTime - 0.55f), false);
}

void AAKS74UWeapon::DetachMagazineNow()
{
	if (!MagazineMesh || !MagazineMesh->IsVisible()) return;
	AShooterCharacter* Character = Cast<AShooterCharacter>(PawnOwner);
	if (Character)
	{
		MagazineMesh->AttachToComponent(Character->GetFirstPersonMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("hand_l"));
		MagazineMesh->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		MagazineMesh->SetRelativeScale3D(FVector(0.14f));
	}
}

void AAKS74UWeapon::ThrowMagazineFromHand()
{
	if (!MagazineMesh) return;
	MagazineMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	MagazineMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MagazineMesh->SetSimulatePhysics(true);
	MagazineMesh->AddImpulse(GetActorRightVector() * 80.0f + GetActorUpVector() * 25.0f + GetActorForwardVector() * 10.0f, NAME_None, true);
}

void AAKS74UWeapon::ShowReplacementMagazineInHand()
{
	if (!MagazineInHandMesh) return;
	AShooterCharacter* Character = Cast<AShooterCharacter>(PawnOwner);
	if (Character)
	{
		MagazineInHandMesh->AttachToComponent(Character->GetFirstPersonMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("hand_l"));
		MagazineInHandMesh->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		MagazineInHandMesh->SetRelativeScale3D(FVector(0.14f));
		MagazineInHandMesh->SetVisibility(true, true);
	}
}

void AAKS74UWeapon::OnReloadFinished()
{
	if (!MagazineMesh) return;
	GetWorld()->GetTimerManager().ClearTimer(MagazineDetachTimer);
	GetWorld()->GetTimerManager().ClearTimer(MagazineInsertTimer);
	GetWorld()->GetTimerManager().ClearTimer(MagazineThrowTimer);
	GetWorld()->GetTimerManager().ClearTimer(MagazineHandTimer);
	if (MagazineInHandMesh) MagazineInHandMesh->SetVisibility(false, true);
	InsertMagazineNow();
}

void AAKS74UWeapon::InsertMagazineNow()
{
	if (!MagazineMesh) return;
	if (MagazineInHandMesh) MagazineInHandMesh->SetVisibility(false, true);
	MagazineMesh->SetSimulatePhysics(false);
	MagazineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MagazineMesh->AttachToComponent(GetFirstPersonMesh(), FAttachmentTransformRules::KeepRelativeTransform);
	MagazineMesh->SetRelativeTransform(MagazineRelativeTransform);
	MagazineMesh->SetVisibility(true, true);
}

void AAKS74UWeapon::SpawnEjectedCasing()
{
	if (!GetWorld()) return;
	const FVector Muzzle = GetFirstPersonMesh()->DoesSocketExist(MuzzleSocketName)
		? GetFirstPersonMesh()->GetSocketLocation(MuzzleSocketName)
		: GetFirstPersonMesh()->GetComponentLocation() + GetFirstPersonMesh()->GetForwardVector() * 45.0f;
	const FVector EjectLocation = Muzzle + GetActorRightVector() * 9.0f + FVector(0.0f, 0.0f, 2.0f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AShooterEjectedCasing* Casing = GetWorld()->SpawnActor<AShooterEjectedCasing>(AShooterEjectedCasing::StaticClass(), EjectLocation, GetActorRotation(), Params);
	if (Casing)
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Casing->GetRootComponent()))
		{
			Primitive->AddImpulse(GetActorRightVector() * 180.0f + FVector(0.0f, 0.0f, 80.0f) + GetActorForwardVector() * 25.0f);
			Primitive->AddAngularImpulseInRadians(FVector(0.0f, 30.0f, 15.0f), NAME_None, true);
		}
	}
}
