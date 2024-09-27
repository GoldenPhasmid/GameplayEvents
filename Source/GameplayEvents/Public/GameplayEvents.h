#pragma once

#include "CoreMinimal.h"

#include "GameplayEvents.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGameplayEvents, Log, All);

/**
 * Empty gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FEmptyEvent
{
	GENERATED_BODY()
};

/**
 * Generic object gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FObjectGameplayEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	UObject* Object = nullptr;
};

/**
 * Generic actor gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FActorGameplayEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	AActor* Actor = nullptr;
};
