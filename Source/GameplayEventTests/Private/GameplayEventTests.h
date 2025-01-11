#pragma once

#include "CoreMinimal.h"

#include "GameplayEventTests.generated.h"

struct FGameplayTag;

UCLASS(HideDropdown)
class UGameplayEventTestReceiver: public UObject
{
	GENERATED_BODY()
public:
	void OnEventReceived(const FVector& Event);
	void OnEventWithTagReceived(FGameplayTag EventTag, const FVector& Event);
};