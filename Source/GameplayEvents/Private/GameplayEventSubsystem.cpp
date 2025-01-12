#include "GameplayEventSubsystem.h"

#include "GameplayEvents.h"
#include "GameplayTagsManager.h"

static bool GShouldLogGameplayEvents = false;
static FAutoConsoleVariableRef CVarShouldLogGameplayEvents(
	TEXT("GameplayEvents.LogEvents"),
	GShouldLogGameplayEvents,
	TEXT("Whether events sent through gameplay events subsystem should logged")
);

static bool GAllowSendingNonLeafEventChannels = false;
static FAutoConsoleVariableRef CVarAllowSendingNonLeafEventChannels(
	TEXT("GameplayEvents.AllowSendingNonLeafEventChannels"),
	GAllowSendingNonLeafEventChannels,
	TEXT("Whether non-leaf tag channels should be allowed for SendEvent")
);

static bool GShouldDumpCallstack = true;
static FAutoConsoleVariableRef CVarShouldDumpCallstack(
	TEXT("GameplayEvents.ShouldDumpCallstack"),
	GShouldDumpCallstack,
	TEXT("Whether dump callstack for invalid channel tags")
);

uint32 UGameplayEventSubsystem::HandleID = 1;

UGameplayEventSubsystem* UGameplayEventSubsystem::Get(const UObject* WorldContextObject)
{
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		return UGameInstance::GetSubsystem<UGameplayEventSubsystem>(World->GetGameInstance());
	}

	return nullptr;
}

UGameplayEventSubsystem& UGameplayEventSubsystem::GetChecked(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	return *UGameInstance::GetSubsystem<UGameplayEventSubsystem>(World->GetGameInstance());
}

void UGameplayEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EventContainers.Reset();
}

void UGameplayEventSubsystem::Deinitialize()
{
	for (const FSimpleDelegate& Deleter: PendingEventDeleters)
	{
		Deleter.Execute();
	}
	
	EventContainers.Reset();
	PendingEvents.Reset();
	PendingEventDeleters.Reset();
	
	Super::Deinitialize();
}

void UGameplayEventSubsystem::Tick(float DeltaTime)
{
	TArray<FChannelEvent> EventsToSend{MoveTemp(PendingEvents)};
	TArray<FSimpleDelegate> Deleters{MoveTemp(PendingEventDeleters)};
	
	for (const FChannelEvent& ChannelEvent: EventsToSend)
	{
		SendEventInternal(ChannelEvent, ESendEventMode::Delayed);
	}
	
	for (const FSimpleDelegate& Deleter: Deleters)
	{
		Deleter.Execute();
	}
}

void UGameplayEventSubsystem::K2_SendEvent(const UObject* WorldContextObject, FGameplayTag Channel, const int32& Event, ESendEventMode SendEventMode)
{
	// Exec version should be hit instead
	checkNoEntry();	
}

DEFINE_FUNCTION(UGameplayEventSubsystem::execK2_SendEvent)
{
	P_GET_OBJECT(const UObject, WorldContextObject);
	P_GET_STRUCT(FGameplayTag, Channel);
	
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const void* EventPtr = Stack.MostRecentPropertyAddress;
	const FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_GET_ENUM(ESendEventMode, SendEventMode);

	P_FINISH;

	P_NATIVE_BEGIN;
	if (ensure((StructProperty != nullptr) && (StructProperty->Struct != nullptr) && (EventPtr != nullptr)))
	{
		UScriptStruct* Struct = StructProperty->Struct;
		if (UGameplayEventSubsystem* EventSubsystem = UGameplayEventSubsystem::Get(WorldContextObject))
		{
			if (SendEventMode == ESendEventMode::Delayed)
			{
				void* Event = FMemory::Malloc(Struct->GetStructureSize());
				Struct->CopyScriptStruct(Event, EventPtr);

				FChannelEvent ChannelEvent{Channel, Struct, Event};
				EventSubsystem->AddPendingEventInternal(ChannelEvent, FSimpleDelegate::CreateLambda([Event, Struct]
				{
					Struct->DestroyStruct(Event);
					FMemory::Free(Event);
				}));
			}
			else if (SendEventMode == ESendEventMode::Immediate)
			{
				FChannelEvent ChannelEvent{Channel, Struct,  EventPtr};
				EventSubsystem->SendEventInternal(ChannelEvent, SendEventMode);
			}
		}
	}
	P_NATIVE_END;
}

void UGameplayEventSubsystem::AddPendingEventInternal(const FChannelEvent& ChannelEvent, FSimpleDelegate EventDeleter)
{
	PendingEvents.Add(ChannelEvent);
	PendingEventDeleters.Add(MoveTemp(EventDeleter));
}

void UGameplayEventSubsystem::SendEventInternal(const FChannelEvent& ChannelEvent, ESendEventMode SendMode)
{
	FGameplayTag Channel = ChannelEvent.OriginalChannel;
	if (!Channel.IsValid())
	{
		return;
	}
	
#if !UE_BUILD_SHIPPING
	if (!GAllowSendingNonLeafEventChannels)
	{
		const FGameplayTagContainer LeafTags = UGameplayTagsManager::Get().RequestGameplayTagChildren(Channel);
		if (!LeafTags.IsEmpty())
		{
			UE_LOG(LogGameplayEvents, Error, TEXT("Broadcasting non-leaf tags is disabled. Possible channels: [%s]. Actual channel: [%s]"), *LeafTags.ToString(), *Channel.ToString());
			if (GShouldDumpCallstack)
			{
				constexpr uint32 DumpCallstackSize = 65535;
				ANSICHAR DumpCallstack[DumpCallstackSize] = { 0 };
				const FString ScriptStack = FFrame::GetScriptCallstack(true);
				FPlatformStackWalk::StackWalkAndDump(DumpCallstack, DumpCallstackSize, 0);

				UE_LOG(LogGameplayEvents, Error, TEXT("--- Callstack ---"));
				UE_LOG(LogGameplayEvents, Error, TEXT("Script Stack:\n%s"), *ScriptStack);
				UE_LOG(LogGameplayEvents, Error, TEXT("Callstack:\n%s"), ANSI_TO_TCHAR(DumpCallstack));
			}
		}
	}
	
	if (GShouldLogGameplayEvents)
	{
		FString ContextString = GetPathNameSafe(this);
#if WITH_EDITOR
		if (GIsEditor)
		{
			// forward declare GPlayInEditorContextString from UnrealEngine.cpp
			extern ENGINE_API FString GPlayInEditorContextString;
			ContextString = GPlayInEditorContextString;
		}
#endif

		const FString SendModeString = SendMode == ESendEventMode::Immediate ? TEXT("Immediate") : TEXT("Async");
		
		FString EventString;
		ChannelEvent.EventType->ExportText(EventString, ChannelEvent.Event, nullptr, nullptr, PPF_None, nullptr);
		
		UE_LOG(LogGameplayEvents, Display, TEXT("Gameplay Event: Context: [%s], Channel: [%s], Event: [%s], Mode: [%s]"),
			*ContextString, *Channel.ToString(), *EventString, *SendModeString);
	}
#endif

	FGameplayEventContainerRef EventContainer = GetOrCreateEventContainer(ChannelEvent.EventType);
	for (FGameplayTag Tag = Channel; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		EventContainer->Broadcast(ChannelEvent, Tag);
	}
}

FDelegateHandle UGameplayEventSubsystem::AddReceiverInternal(const FGameplayTag& Channel, const UScriptStruct* EventType, TRawGameplayEventDelegate&& InnerCallback)
{
	FGameplayEventContainerRef EventContainer = GetOrCreateEventContainer(EventType);
	return EventContainer->Add(Channel, MoveTemp(InnerCallback));
}

void UGameplayEventSubsystem::RemoveReceiverInternal(const FGameplayEventHandle& EventHandle)
{
	if (!EventHandle.IsValid())
	{
		return;
	}
	
	FGameplayEventContainerRef EventContainer = GetOrCreateEventContainer(EventHandle.EventType.Get());
	EventContainer->Remove(EventHandle);

	EventHandle.Invalidate();
}

UGameplayEventSubsystem::FGameplayEventContainerRef UGameplayEventSubsystem::GetOrCreateEventContainer(const UScriptStruct* EventType)
{
	FGameplayEventContainerPtr Container;
	if (TSharedPtr<FGameplayEventContainer>* ContainerPtr = EventContainers.Find(EventType))
	{
		Container = StaticCastSharedPtr<FGameplayEventContainer>(*ContainerPtr);
	}
	else
	{
		Container = MakeShared<FGameplayEventContainer>();
		EventContainers.Add(EventType, Container);
	}

	return Container.ToSharedRef();
}

uint32 UGameplayEventSubsystem::GenerateNewID()
{
	uint32 Result = ++HandleID;

	if (UNLIKELY(Result == 0))
	{
		Result = ++HandleID;
	}

	return Result;
}
