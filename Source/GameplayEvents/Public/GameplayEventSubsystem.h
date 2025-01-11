#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayEventHandle.h"

#include "GameplayEventSubsystem.generated.h"

class UGameplayEventSubsystem;

template <typename T>
using TGameplayEventDelegate = TDelegate<void(const T&)>;

template <typename T>
using TGameplayEventWithTagDelegate = TDelegate<void(FGameplayTag, const T&)>;

template <typename TClass, typename T>
using TGameplayEventFunctor = void(TClass::*)(const T&);

template <typename TClass, typename T>
using TGameplayEventWithTagFunctor = void(TClass::*)(FGameplayTag, const T&);

UENUM(BlueprintType)
enum class ESendEventMode: uint8
{
	Immediate,	// event is sent immediately
	Async,		// event is going to be sent by the end of the frame
};

/**
 * This system allows event senders and receivers to register for events without
 * having to know about each other directly, though they must agree on the format
 * of the message (as a USTRUCT() type).
 *
 *
 * You can get to the message router from the game instance:
 *    UGameInstance::GetSubsystem<UGameplayEventSubsystem>(GameInstance)
 * or directly from anything that has a route to a world:
 *    UGameplayEventSubsystem::Get(WorldContextObject)
 *
 * Note that call order when there are multiple receivers for the same event channel is
 * not guaranteed and can change over time.
 */
UCLASS()
class GAMEPLAYEVENTS_API UGameplayEventSubsystem: public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
	friend class UAsyncAction_WaitGameplayEvent;
	
	using TRawGameplayEventDelegate		= TDelegate<void(FGameplayTag, const void*)>;
	using FGameplayEventPayloadDeleter	= TDelegate<void(const void*)>;

	/**
	 * Describes a single event by channel
	 * @OriginalChannel - channel it was send by. All sub-channels also receive the same event
	 * @Event - represented by EventType and Event data.
	 * @CustomDeleter responsible for freeing event data if it is owned by the ChannelEvent
	 */
	struct FChannelEvent
	{
		FChannelEvent(const FGameplayTag& InChannel, const UScriptStruct* InEventType, const void* InEvent)
			: OriginalChannel(InChannel), EventType(InEventType), Event(InEvent)
		{}
		
		FGameplayTag OriginalChannel = FGameplayTag::EmptyTag;
		const UScriptStruct* EventType = nullptr;
		const void* Event = nullptr;
	};
	
	struct FGameplayEventContainer
	{
		using FCallbackContainer = TMulticastDelegate<void(FGameplayTag, const void*)>;
		
		FDelegateHandle Add(const FGameplayTag& Channel, TRawGameplayEventDelegate&& Callback)
		{
			return Channels.FindOrAdd(Channel).Add(Forward<TRawGameplayEventDelegate>(Callback));
		}

		void Remove(const FGameplayEventHandle& Handle)
		{
			Channels.FindChecked(Handle.Channel).Remove(Handle.DelegateHandle);
		}

		void RemoveAll(const void* UserObject)
		{
			for (auto& [Channel, Container]: Channels)
			{
				Container.RemoveAll(UserObject);
			}
		}
		
		void Broadcast(const FChannelEvent& Event, const FGameplayTag& ChannelTag)
		{
			if (const FCallbackContainer* Container = Channels.Find(ChannelTag))
			{
				Container->Broadcast(Event.OriginalChannel, Event.Event);
			}
		}

		TMap<FGameplayTag, FCallbackContainer> Channels;
	};
	
	using FGameplayEventContainerPtr = TSharedPtr<FGameplayEventContainer>;
	using FGameplayEventContainerRef = TSharedRef<FGameplayEventContainer>;

public:

	/** @return event subsystem based on the world context */
	static UGameplayEventSubsystem* Get(const UObject* WorldContextObject);
	/** @return event subsystem and verify that it actually exists */
	static UGameplayEventSubsystem& GetChecked(const UObject* WorldContextObject);

	//~Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End USubsystem interface

	//~Begin TickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(GameplayEventsStats, STATGROUP_Tickables); }
	//~End TickableGameObject interface

	/**
	 * Send event on a specified channel, constructing event data using variadic template
	 * @param Channel event channel to send event to
	 * @param Args arguments to construct the event
	 */
	template <typename TEvent, typename ...TArgs>
	void SendEvent(const FGameplayTag& Channel, TArgs&&... Args)
	{
		TEvent Event{Forward<TArgs>(Args)...};
		FChannelEvent ChannelEvent{Channel, TBaseStructure<TEvent>::Get(), &Event};
		
		SendEventInternal(ChannelEvent, ESendEventMode::Immediate);
	}

	/**
	 * Send event on a specified channel
	 * @param Channel event channel to send event to
	 * @param Event event to send
	 */
	template <typename TEvent>
	void SendEvent(const FGameplayTag& Channel, const TEvent& Event)
	{
		FChannelEvent ChannelEvent{Channel, TBaseStructure<TEvent>::Get(), &Event};
		SendEventInternal(ChannelEvent, ESendEventMode::Immediate);
	}

	/**
	 * Send delayed event on a specified channel, so that subscribers don't receive immediate notification
	 * @note delayed events are processed at the end of the frame
	 * @param Channel event channel to send event to
	 * @param Args arguments to construct event from
	 */
	template <typename TEvent, typename ...TArgs>
	void SendEventAsync(const FGameplayTag& Channel, TArgs&&... Args)
	{
		TEvent* EventPtr = new TEvent{Forward<TArgs>(Args)...};
		FChannelEvent ChannelEvent{Channel, TBaseStructure<TEvent>::Get(), EventPtr};
		
		AddPendingEventInternal(ChannelEvent, FSimpleDelegate::CreateLambda([EventPtr] { delete EventPtr; }));
	}

	/**
	 * Send delayed event on a specified channel, so that subscribers don't receive immediate notification
	 * @note delayed events are processed at the end of the frame
	 * @param Channel event channel to send event to
	 * @param Event event to send
	 */
	template <typename TEvent>
	void SendEventAsync(const FGameplayTag& Channel, const TEvent& Event)
	{
		TEvent* EventPtr = new TEvent{Event};
		FChannelEvent ChannelEvent{Channel, TBaseStructure<TEvent>::Get(), EventPtr};

		AddPendingEventInternal(ChannelEvent, FSimpleDelegate::CreateLambda([EventPtr] { delete EventPtr; }));
	}


	/** Bind lambda to receive gameplay events on a specified channel, void(const TEvent&) callback format */
	template <typename TEvent>
	FGameplayEventHandle AddReceiver(FGameplayTag Channel, TFunction<void(const FVector&)> Callback)
	{
		return AddReceiver(Channel, TGameplayEventDelegate<TEvent>::CreateLambda(Callback));
	}

	/** Bind lambda to receive gameplay events on a specified channel, void(FGameplayTag Tag, const TEvent&) callback format */
	template <typename TEvent>
	FGameplayEventHandle AddReceiver(FGameplayTag Channel, TFunction<void(FGameplayTag, const FVector&)> Callback)
	{
		return AddReceiver(Channel, TGameplayEventWithTagDelegate<TEvent>::CreateLambda(Callback));
	}

	/** Bind UObject function to receive gameplay events on a specified channel, void(const TEvent&) callback format */
	template <typename TEvent, typename TObject>
	FGameplayEventHandle AddReceiver(FGameplayTag Channel, TObject* Receiver, TGameplayEventFunctor<TObject, TEvent> Callback)
	{
		return AddReceiver(Channel, TGameplayEventDelegate<TEvent>::CreateUObject(Receiver, Callback));
	}

	/** Bind UObject function to receive gameplay events on a specified channel, void(FGameplayTag Tag, const TEvent&) callback format */
	template <typename TEvent, typename TObject>
	FGameplayEventHandle AddReceiver(FGameplayTag Channel, TObject* Receiver, TGameplayEventWithTagFunctor<TObject, TEvent> Callback)
	{
		return AddReceiver(Channel, TGameplayEventWithTagDelegate<TEvent>::CreateUObject(Receiver, Callback));
	}
	
	/**
	 * Add receiver to a specified channel to receive events of a specified template type
	 * This is a version that receives void(const TEvent&) callback format
	 * @param Channel event channel to listen
	 * @param Callback callback function to call with the event when someone sends it
	 * 
	 * @return gameplay event handle, which can be used to remove this receiver
	 */
	template <typename TEvent>
	FGameplayEventHandle AddReceiver(FGameplayTag Channel, TGameplayEventDelegate<TEvent>&& Callback)
	{
		const UScriptStruct* EventType = TBaseStructure<TEvent>::Get();
		FGameplayEventHandle EventHandle{this, GenerateNewID(), Channel, EventType};

		TRawGameplayEventDelegate ThunkCallback = TRawGameplayEventDelegate::CreateLambda([InnerCallback = Forward<TGameplayEventDelegate<TEvent>>(Callback)]
			(FGameplayTag Channel, const void* Event)
		{
			if (InnerCallback.IsBound())
			{
				InnerCallback.Execute(*static_cast<const TEvent*>(Event));
			}
		});
		
		EventHandle.DelegateHandle = AddReceiverInternal(Channel, EventType, MoveTemp(ThunkCallback));

		return EventHandle;
	}

	/**
	 * Add receiver to a specified channel to receive events of a specified template type
	 * This is a version that receives void(FGameplayTag Tag, const TEvent&) callback format
	 * @param Channel event channel to listen
	 * @param Callback callback function to call with the event when someone sends it
	 * 
	 * @return gameplay event handle, which can be used to remove this receiver
	 */
	template <typename TEvent>
	FGameplayEventHandle AddReceiver(FGameplayTag Channel, const TGameplayEventDelegate<TEvent>& Callback)
	{
		const UScriptStruct* EventType = TBaseStructure<TEvent>::Get();
		FGameplayEventHandle EventHandle{this, GenerateNewID(), Channel, EventType};
		
		TRawGameplayEventDelegate ThunkCallback = TRawGameplayEventDelegate::CreateLambda([InnerCallback = Callback]
			(FGameplayTag Channel, const void* Event)
		{
			if (InnerCallback.IsBound())
			{
				InnerCallback.Execute(*static_cast<const TEvent*>(Event));
			}
		});
		
		EventHandle.DelegateHandle = AddReceiverInternal(Channel, EventType, MoveTemp(ThunkCallback));

		return EventHandle;
	}


	/**
	 * Add receiver to a specified channel to receive events of a specified template type
	 * This is a version that receives void(FGameplayTag, const TEvent&) callback format
	 * 
	 * @param Channel event channel to listen
	 * @param Callback callback function to call with the event when someone sends it
	 * 
	 * @return gameplay event handle, which can be used to remove this receiver
	 */
	template <typename TEvent>
	FGameplayEventHandle AddReceiver(FGameplayTag Channel, TGameplayEventWithTagDelegate<TEvent>&& Callback)
	{
		const UScriptStruct* EventType = TBaseStructure<TEvent>::Get();
		FGameplayEventHandle EventHandle{this, GenerateNewID(), Channel, EventType};

		TRawGameplayEventDelegate ThunkCallback = TRawGameplayEventDelegate::CreateLambda([InnerCallback = Forward<TGameplayEventWithTagDelegate<TEvent>>(Callback)]
			(FGameplayTag Channel, const void* Event)
		{
			if (InnerCallback.IsBound())
			{
				InnerCallback.Execute(Channel, *static_cast<const TEvent*>(Event));
			}
		});
		
		EventHandle.DelegateHandle = AddReceiverInternal(Channel, EventType, MoveTemp(ThunkCallback));

		return EventHandle;
	}

	/**
	 * Add receiver to a specified channel to receive events of a specified template type
	 * This is a version that receives void(FGameplayTag, const TEvent&) callback format
	 * 
	 * @param Channel event channel to listen
	 * @param Callback callback function to call with the event when someone sends it
	 * 
	 * @return gameplay event handle, which can be used to remove this receiver
	 */
	template <typename TEvent>
	FGameplayEventHandle AddReceiver(FGameplayTag Channel, const TGameplayEventWithTagDelegate<TEvent>& Callback)
	{
		const UScriptStruct* EventType = TBaseStructure<TEvent>::Get();
		FGameplayEventHandle EventHandle{this, GenerateNewID(), Channel, EventType};
		
		TRawGameplayEventDelegate ThunkCallback = TRawGameplayEventDelegate::CreateLambda([InnerCallback = Callback]
			(FGameplayTag Channel, const void* Event)
		{
			if (InnerCallback.IsBound())
			{
				InnerCallback.Execute(Channel, *static_cast<const TEvent*>(Event));
			}
		});
		
		EventHandle.DelegateHandle = AddReceiverInternal(Channel, EventType, MoveTemp(ThunkCallback));

		return EventHandle;
	}

	/**
	 * Remove previously registered event receiver
	 * @param EventHandle handle returned by @AddReceiver
	 */ 
	void RemoveReceiver(const FGameplayEventHandle& EventHandle)
	{
		RemoveReceiverInternal(EventHandle);
	}

	/**
	 * Remove previous registered event receivers
	 * @param EventHandles handles to gameplay events
	 */
	void RemoveReceivers(TConstArrayView<FGameplayEventHandle> EventHandles)
	{
		for (const FGameplayEventHandle& EventHandle : EventHandles)
		{
			RemoveReceiverInternal(EventHandle);
		}
	}

protected:
	
	/**
	 * Send gameplay event on specified channel with payload.
	 * @param WorldContextObject
	 * @param Channel event channel
	 * @param Event event payload
	 * @param SendMode send mode, either immediate or async
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Events", meta = (DefaultToSelf = "WorldContextObject", CustomStructureParam = "Event", AllowAbstract = "false", DisplayName = "Send Event"))
	static void K2_SendEvent(const UObject* WorldContextObject, FGameplayTag Channel, const int32& Event, ESendEventMode SendMode);

	DECLARE_FUNCTION(execK2_SendEvent);

	/** add pending event with a custom event deleter */
	void AddPendingEventInternal(const FChannelEvent& ChannelEvent, FSimpleDelegate EventDeleter);
	
	/**
	 * Send event with given type on a given channel
	 * @param ChannelEvent event data
	 * @param SendMode send mode, either immediate or async
	 */
	void SendEventInternal(const FChannelEvent& ChannelEvent, ESendEventMode SendMode);

	/** add event receiver for given event type */
	FDelegateHandle AddReceiverInternal(const FGameplayTag& Channel, const UScriptStruct* EventType, TRawGameplayEventDelegate&& InnerCallback);

	/** Remove event receiver using event handle */
	void RemoveReceiverInternal(const FGameplayEventHandle& EventHandle);
	
	/** @return gameplay event container for given event type */
	FGameplayEventContainerRef GetOrCreateEventContainer(const UScriptStruct* EventType);

	static uint32 GenerateNewID();

	TMap<const UStruct*, TSharedPtr<FGameplayEventContainer>> EventContainers;
	TArray<FChannelEvent> PendingEvents;
	TArray<FSimpleDelegate> PendingEventDeleters;
	static uint32 HandleID;
};
