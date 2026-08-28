#include "AKS74UProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AAKS74UProjectile::AAKS74UProjectile()
{
	BulletVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bullet Visual"));
	BulletVisual->SetupAttachment(GetCollisionComponent());
	BulletVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BulletVisual->SetCastShadow(false);
	// Elongated tracer-like core so the projectile is readable at gameplay speed.
	BulletVisual->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.6f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BulletMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (BulletMesh.Succeeded())
	{
		BulletVisual->SetStaticMesh(BulletMesh.Object);
	}

	HitDamage = 34.0f;
	PhysicsForce = 300.0f;
	bExplodeOnHit = false;
	DeferredDestructionTime = 0.0f;

	GetCollisionComponent()->SetSphereRadius(2.0f);

	UProjectileMovementComponent* Movement = GetProjectileMovementComponent();
	Movement->InitialSpeed = 73500.0f;
	Movement->MaxSpeed = 73500.0f;
	Movement->ProjectileGravityScale = 0.12f;
	Movement->bShouldBounce = false;
	Movement->bForceSubStepping = true;
}
