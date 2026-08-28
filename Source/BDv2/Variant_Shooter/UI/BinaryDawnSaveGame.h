#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BinaryDawnSaveGame.generated.h"

UCLASS()
class BDV2_API UBinaryDawnSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite) FString MapName;
	UPROPERTY(BlueprintReadWrite) FTransform PlayerTransform;
	UPROPERTY(BlueprintReadWrite) FString District;
	UPROPERTY(BlueprintReadWrite) float GameTimeSeconds = 0.0f;
	UPROPERTY(BlueprintReadWrite) FDateTime SavedAt;
};
