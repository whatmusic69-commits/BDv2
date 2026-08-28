#include "ShooterEjectedCasing.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AShooterEjectedCasing::AShooterEjectedCasing()
{
	PrimaryActorTick.bCanEverTick = false;
	UStaticMeshComponent* Casing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Casing"));
	RootComponent = Casing;
	Casing->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Casing->SetCollisionProfileName(TEXT("PhysicsActor"));
	Casing->SetSimulatePhysics(true);
	Casing->SetRelativeScale3D(FVector(0.035f, 0.035f, 0.09f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Mesh.Succeeded()) Casing->SetStaticMesh(Mesh.Object);
	InitialLifeSpan = 5.0f;
}
