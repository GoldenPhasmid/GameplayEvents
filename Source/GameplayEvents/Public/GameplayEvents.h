#pragma once

#include "CoreMinimal.h"

#include "GameplayEvents.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGameplayEvents, Log, All);

USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FGameplayEventBase
{
	GENERATED_BODY()
};

/**
 * Empty gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FEmptyEvent: public FGameplayEventBase
{
	GENERATED_BODY()
};

/**
 * Generic object gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FObjectGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> Object = nullptr;
};

/**
 * Generic actor gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FActorGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> Actor = nullptr;
};
