#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShooterDroppedWeapon.generated.h"

class AShooterCharacter;
class AShooterWeapon;
class USphereComponent;
class UStaticMeshComponent;

/** A weapon dropped into the world and waiting for an explicit F pickup. */
UCLASS(Blueprintable)
class BDV2_API AShooterDroppedWeapon : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> WeaponMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	TSubclassOf<AShooterWeapon> WeaponClass;
	TWeakObjectPtr<AShooterCharacter> NearbyCharacter;

public:
	AShooterDroppedWeapon();
	void Initialize(TSubclassOf<AShooterWeapon> InWeaponClass);
	TSubclassOf<AShooterWeapon> GetWeaponClass() const { return WeaponClass; }
	void TryPickup(AShooterCharacter* Character);
	void ThrowFromCharacter(const FVector& Direction);
	FVector GetVisualLocation() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION()
	void OnInteractionBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnInteractionEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
