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

USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FStringEvent: public FGameplayEventBase
{
	GENERATED_BODY()
	
	FStringEvent() = default;
	FStringEvent(const FString& InEvent)
		: Event(InEvent)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Event;
};


/**
 * Generic class gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FClassGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UClass> Object = nullptr;
};

USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FSoftClassGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<UClass> Object = nullptr;
};

/**
 * Generic object gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FObjectGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()

	FObjectGameplayEvent() = default;
	FObjectGameplayEvent(UObject* InObject)
		: Object(InObject)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> Object = nullptr;
};

USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FSoftObjectGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UObject> Object = nullptr;
};

/**
 * Generic actor gameplay event
 */
USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FActorGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()

	FActorGameplayEvent() = default;
	FActorGameplayEvent(AActor* InActor)
		: Actor(InActor)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> Actor = nullptr;
};

USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FSoftActorGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<AActor> Actor = nullptr;
};

USTRUCT(BlueprintType)
struct GAMEPLAYEVENTS_API FPlayerStateGameplayEvent: public FGameplayEventBase
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APlayerState> PlayerState = nullptr;
};
