#pragma once

#include "CoreMinimal.h"
#include "ShooterProjectile.h"
#include "AKS74UProjectile.generated.h"

/** Ballistic projectile defaults for the AKS-74U's 5.45x39 mm cartridge. */
UCLASS(Blueprintable)
class BDV2_API AAKS74UProjectile : public AShooterProjectile
{
	GENERATED_BODY()

	/** Small visible projectile core so shots are readable in-game. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile", meta=(AllowPrivateAccess="true"))
	TObjectPtr<class UStaticMeshComponent> BulletVisual;

public:
	AAKS74UProjectile();
};
