#pragma once

#include "GameplayTagContainer.h"

class UGameplayEventSubsystem;

/**
 * Gameplay Event Handle
 * Points to a gameplay event binding, should be used to remove the binding later
 */
struct GAMEPLAYEVENTS_API FGameplayEventHandle
{
	friend class UGameplayEventSubsystem;
	friend class UAsyncAction_WaitGameplayEvent;
public:
	
	FGameplayEventHandle() = default;

	/** @return true if event handle is still valid */
	bool IsValid() const;

	friend FORCEINLINE bool operator==(const FGameplayEventHandle& Lhs, const FGameplayEventHandle& Rhs)
	{
		return Lhs.HandleID == Rhs.HandleID;
	}

	friend FORCEINLINE bool operator!=(const FGameplayEventHandle& Lhs, const FGameplayEventHandle& Rhs)
	{
		return !(Lhs == Rhs);
	}

	friend FORCEINLINE uint32 GetTypeHash(const FGameplayEventHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.Channel), GetTypeHash(Handle.DelegateHandle));
	}
	
private:

	/** invalidates event handle so it no longer points to a live binding. Called only as a result of RemoveReceiver call */
	void Invalidate() const;

	FGameplayEventHandle(UGameplayEventSubsystem* InSubsystem, uint32 InID, const FGameplayTag& InChannel, const UScriptStruct* InEventType)
		: Subsystem(InSubsystem)
		, HandleID(InID)
		, Channel(InChannel)
		, EventType(InEventType)
	{ }
	
	mutable UGameplayEventSubsystem* Subsystem = nullptr;
	mutable uint32 HandleID = 0;
	
	FGameplayTag Channel;
	TWeakObjectPtr<const UScriptStruct> EventType;
	
	FDelegateHandle DelegateHandle;
};

