#include "ShooterDroppedWeapon.h"

#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"

AShooterDroppedWeapon::AShooterDroppedWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon Mesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	WeaponMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	WeaponMesh->SetSimulatePhysics(true);
	WeaponMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	WeaponMesh->SetRelativeScale3D(FVector(0.14f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Weapons/AKS74U/Meshes/SM_AKS74U.SM_AKS74U"));
	if (Mesh.Succeeded()) WeaponMesh->SetStaticMesh(Mesh.Object);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Interaction Sphere"));
	InteractionSphere->SetupAttachment(WeaponMesh);
	InteractionSphere->SetSphereRadius(140.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AShooterDroppedWeapon::OnInteractionBegin);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AShooterDroppedWeapon::OnInteractionEnd);
}

void AShooterDroppedWeapon::Initialize(TSubclassOf<AShooterWeapon> InWeaponClass)
{
	WeaponClass = InWeaponClass;
}

void AShooterDroppedWeapon::ThrowFromCharacter(const FVector& Direction)
{
	if (WeaponMesh)
	{
		WeaponMesh->SetSimulatePhysics(true);
		WeaponMesh->AddImpulse(Direction.GetSafeNormal() * 260.0f + FVector(0.0f, 0.0f, 70.0f));
		WeaponMesh->AddAngularImpulseInRadians(FVector(0.0f, 0.0f, 35.0f), NAME_None, true);
	}
}

void AShooterDroppedWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void AShooterDroppedWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (NearbyCharacter.IsValid())
	{
		bool bAimedAt = false;
		if (UCameraComponent* Camera = NearbyCharacter->GetFirstPersonCameraComponent())
		{
			FHitResult Hit;
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroppedWeaponAim), true, NearbyCharacter.Get());
			bAimedAt = GetWorld()->LineTraceSingleByChannel(Hit, Camera->GetComponentLocation(), Camera->GetComponentLocation() + Camera->GetForwardVector() * 500.0f, ECC_Visibility, QueryParams) && Hit.GetActor() == this;
		}
		if (bAimedAt)
		{
			DrawDebugString(GetWorld(), WeaponMesh->GetComponentLocation() + FVector(0, 0, 35), TEXT("F  -  подобрать"), nullptr, FColor::White, 0.0f, true);
		}
	}
}

void AShooterDroppedWeapon::OnInteractionBegin(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (AShooterCharacter* Character = Cast<AShooterCharacter>(OtherActor))
	{
		NearbyCharacter = Character;
		Character->SetNearbyDroppedWeapon(this);
	}
}

void AShooterDroppedWeapon::OnInteractionEnd(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (AShooterCharacter* Character = Cast<AShooterCharacter>(OtherActor))
	{
		if (NearbyCharacter.Get() == Character)
		{
			NearbyCharacter.Reset();
			Character->SetNearbyDroppedWeapon(nullptr);
		}
	}
}

void AShooterDroppedWeapon::TryPickup(AShooterCharacter* Character)
{
	if (Character && WeaponClass)
	{
		Character->PickUpDroppedWeapon(this);
	}
}

FVector AShooterDroppedWeapon::GetVisualLocation() const
{
	return WeaponMesh ? WeaponMesh->GetComponentLocation() : GetActorLocation();
}
