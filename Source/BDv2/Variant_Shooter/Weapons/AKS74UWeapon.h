#pragma once

#include "CoreMinimal.h"
#include "ShooterWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "AKS74UWeapon.generated.h"

/** Ready-to-configure AKS-74U weapon with sensible gameplay defaults. */
UCLASS(Blueprintable)
class BDV2_API AAKS74UWeapon : public AShooterWeapon
{
	GENERATED_BODY()

public:
	AAKS74UWeapon();

protected:
	virtual void BeginPlay() override;
	virtual void OnReloadStarted() override;
	virtual void OnReloadFinished() override;
	virtual void SpawnEjectedCasing() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UStaticMeshComponent* MagazineMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UStaticMeshComponent* MagazineInHandMesh;

	FTransform MagazineRelativeTransform;
	FTimerHandle MagazineDetachTimer;
	FTimerHandle MagazineInsertTimer;
	FTimerHandle MagazineThrowTimer;
	FTimerHandle MagazineHandTimer;

	void DetachMagazineNow();
	void ThrowMagazineFromHand();
	void ShowReplacementMagazineInHand();
	void InsertMagazineNow();
};
