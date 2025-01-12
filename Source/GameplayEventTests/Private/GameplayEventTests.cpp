#include "GameplayEventTests.h"
#include "GameplayEventSubsystem.h"
#include "GameplayTagsManager.h"

FGameplayTag ChannelTag;
FGameplayTag ChannelGameplayTag;
FGameplayTag ChannelUITag;
FGameplayTag ChannelParent;
FGameplayTag ChannelChild;
FGameplayTag ChannelLeaf;

namespace UE::GameplayEvents
{
	
struct FNativeGameplayTags: public FGameplayTagNativeAdder
{
	virtual ~FNativeGameplayTags() {}

	virtual void AddTags() override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		ChannelTag				= Manager.AddNativeGameplayTag(TEXT("Tests.GameplayEvents.Channel"));
		ChannelGameplayTag		= Manager.AddNativeGameplayTag(TEXT("Tests.GameplayEvents.Channel.Gameplay"));
		ChannelUITag			= Manager.AddNativeGameplayTag(TEXT("Tests.GameplayEvents.Channel.UI"));
		ChannelParent			= Manager.AddNativeGameplayTag(TEXT("Tests.GameplayEvents.Parent"));
		ChannelChild			= Manager.AddNativeGameplayTag(TEXT("Tests.GameplayEvents.Parent.Child"));
		ChannelLeaf				= Manager.AddNativeGameplayTag(TEXT("Tests.GameplayEvents.Parent.Child.Leaf"));
	}

	FORCEINLINE static FNativeGameplayTags& Get()
	{
		return Instance;
	}

	static FNativeGameplayTags Instance;
};
	
FNativeGameplayTags FNativeGameplayTags::Instance;
	
}
constexpr uint32 AutomationFlags = EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask;

void UGameplayEventTestReceiver::OnEventReceived(const FVector& Event)
{
}

void UGameplayEventTestReceiver::OnEventWithTagReceived(FGameplayTag EventTag, const FVector& Event)
{
}


UGameplayEventSubsystem* CreateSubsystem()
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UGameplayEventSubsystem* Subsystem = NewObject<UGameplayEventSubsystem>(GameInstance, NAME_None, RF_Transient);

	return Subsystem;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayEventSubsystemTest_Handle, "Game.GameplayEvents.Handle", AutomationFlags | EAutomationTestFlags::MediumPriority)

bool FGameplayEventSubsystemTest_Handle::RunTest(const FString& Parameters)
{
	UGameplayEventSubsystem* Subsystem = CreateSubsystem();

	{
		FGameplayEventHandle EmptyHandle{};
		FGameplayEventHandle OtherEmptyHandle{};
		TestTrueExpr(!EmptyHandle.IsValid() && !OtherEmptyHandle.IsValid());
		TestTrueExpr(EmptyHandle == OtherEmptyHandle);
	}

	const FGameplayTag Channel = ChannelTag;
	const auto Callback = TGameplayEventWithTagDelegate<FVector>::CreateLambda([](FGameplayTag, const FVector&) { });
	{
		FGameplayEventHandle Handle = Subsystem->AddReceiver(Channel, Callback);
		TestTrueExpr(Handle.IsValid());
	}

	{
		FGameplayEventHandle Handle = Subsystem->AddReceiver(Channel, Callback);
		TestTrueExpr(Handle.IsValid());
		Subsystem->RemoveReceiver(Handle);
		TestTrueExpr(!Handle.IsValid());
	}

	{
		const FGameplayEventHandle Handle = Subsystem->AddReceiver(Channel, Callback);
		TestTrueExpr(Handle.IsValid());
		Subsystem->RemoveReceiver(Handle);
		TestTrueExpr(!Handle.IsValid());
	}

	{
		FGameplayEventHandle EmptyHandle1{}, EmptyHandle2{};
		FGameplayEventHandle Handle1 = Subsystem->AddReceiver(Channel, Callback);
		FGameplayEventHandle Handle2 = Subsystem->AddReceiver(Channel, Callback);

		TestTrueExpr(Handle1.IsValid() && Handle2.IsValid());
		TestTrueExpr(Handle1 != Handle2);
		TestTrueExpr(EmptyHandle1 == EmptyHandle2);
		TestTrueExpr(Handle1 != EmptyHandle1);
	}

	{
		FGameplayEventHandle Handle = Subsystem->AddReceiver(Channel, Callback);
		TestTrueExpr(Handle.IsValid());

		Subsystem->RemoveReceiver(Handle);
		TestTrueExpr(!Handle.IsValid());
		
		Subsystem->RemoveReceiver(Handle);
		TestTrueExpr(!Handle.IsValid());

		Subsystem->RemoveReceiver(FGameplayEventHandle{});
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayEventTest_Callbacks, "GameplayEvents.Callbacks", AutomationFlags | EAutomationTestFlags::MediumPriority)

bool FGameplayEventTest_Callbacks::RunTest(const FString& Parameters)
{
	struct FThunk
	{
		void CallbackWithTag(FGameplayTag Channel, const FVector& InValue)
		{
			++CallbackCount;
			Value = InValue;
		}

		void Callback(const FVector& InValue)
		{
			++CallbackCount;
			Value = InValue;
		}

		int32 CallbackCount = 0;
		FVector Value = FVector::ZeroVector;
	};

	FThunk Thunk;
	auto& [CallbackCount, Value] = Thunk;
	const auto Callback = TGameplayEventDelegate<FVector>::CreateRaw(&Thunk, &FThunk::Callback);
	const auto CallbackWithTag = TGameplayEventWithTagDelegate<FVector>::CreateRaw(&Thunk, &FThunk::CallbackWithTag);

	const FGameplayTag Channel = ChannelGameplayTag;

	UGameplayEventSubsystem* Subsystem = CreateSubsystem();
	{
		FGameplayEventHandle Handle = Subsystem->AddReceiver(Channel, Callback);
		
		// broadcast works
		const FVector V1{1, 1, 1};
		Subsystem->SendEvent(Channel, V1);

		UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 1);
		UTEST_EQUAL(TEXT("CallbackValue"), Value, (V1));

		Subsystem->RemoveReceiver(Handle);
	}

	{
		FGameplayEventHandle Handle = Subsystem->AddReceiver(Channel, CallbackWithTag);

		// broadcast works
		const FVector V2{2, 2, 2};
		Subsystem->SendEvent(Channel, V2);

		UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 2);
		UTEST_EQUAL(TEXT("CallbackValue"), Value, (V2));

		Subsystem->RemoveReceiver(Handle);
	}

	{
		Subsystem->AddReceiver(Channel, Callback);
		Subsystem->AddReceiver(Channel, CallbackWithTag);

		// broadcast works
		const FVector V3{3, 3, 3};
		Subsystem->SendEvent(Channel, V3);

		UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 4);
		UTEST_EQUAL(TEXT("CallbackValue"), Value, (V3));
	}

	// verify that AddReciever binds to UObject functions
	{
		UGameplayEventTestReceiver* Receiver = NewObject<UGameplayEventTestReceiver>();
		UGameplayEventTestReceiver* TagReceiver = NewObject<UGameplayEventTestReceiver>();

		Subsystem->AddReceiver<FVector>(Channel, Receiver, &UGameplayEventTestReceiver::OnEventReceived);
		Subsystem->AddReceiver<FVector>(Channel, TagReceiver, &UGameplayEventTestReceiver::OnEventWithTagReceived);
	}

	// verify that AddReceiver takes two lambda types
	{
		Subsystem->AddReceiver<FVector>(Channel, [](const FVector& Event)
		{
					
		});
		Subsystem->AddReceiver<FVector>(Channel, [](FGameplayTag Tag, const FVector& Event)
		{
			
		});
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayEventTest_Events, "GameplayEvents.Events", AutomationFlags | EAutomationTestFlags::MediumPriority)

bool FGameplayEventTest_Events::RunTest(const FString& Parameters)
{
	struct FThunk
	{
		void Callback(FGameplayTag Channel, const FVector& InValue)
		{
			++CallbackCount;
			Value = InValue;
		}

		int32 CallbackCount = 0;
		FVector Value = FVector::ZeroVector;
	};

	FThunk Thunk;
	auto& [CallbackCount, Value] = Thunk;
	const auto Callback = TGameplayEventWithTagDelegate<FVector>::CreateRaw(&Thunk, &FThunk::Callback);

	UGameplayEventSubsystem* Subsystem = CreateSubsystem();
	const FGameplayTag Channel = ChannelGameplayTag;
	
	FGameplayEventHandle Handle = Subsystem->AddReceiver(Channel, Callback);

	// broadcast works
	const FVector V1{1, 1, 1};
	Subsystem->SendEvent(Channel, V1);
		
	UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 1);
	UTEST_EQUAL(TEXT("CallbackValue"), Value, (V1));

	//broadcast async works
	FVector V2{2, 2, 2};
	Subsystem->SendEventDelayed(Channel, V2);

	UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 1);
	UTEST_EQUAL(TEXT("CallbackValue"), Value, (V1));

	Subsystem->Tick(1.0 / 60);

	UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 2);
	UTEST_EQUAL(TEXT("CallbackValue"), Value, (V2));

	// removing by handle works
	Subsystem->RemoveReceiver(Handle);
	Subsystem->SendEvent(Channel, FVector{3, 3, 3});

	UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 2); 
	UTEST_EQUAL(TEXT("CallbackValue"), Value, (V2));

	// removing by source works
	Subsystem->AddReceiver(Channel, Callback);
	Subsystem->AddReceiver(Channel, Callback);

	FVector V4{4, 4, 4};
	Subsystem->SendEvent(Channel, V4);
	
	UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 4); 
	UTEST_EQUAL(TEXT("CallbackValue"), Value, (V4));

#if 0
	Subsystem->RemoveAll(&Thunk);
	Subsystem->SendEvent(Channel, FVector::ZeroVector);

	UTEST_EQUAL(TEXT("CallbackCount"), CallbackCount, 4);
	UTEST_EQUAL(TEXT("CallbackValue"), Value, (V4));
#endif
	
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayEventTest_Channels, "GameplayEvents.Channels", AutomationFlags | EAutomationTestFlags::MediumPriority)

bool FGameplayEventTest_Channels::RunTest(const FString& Parameters)
{
	UGameplayEventSubsystem* Subsystem = CreateSubsystem();
	
	const FGameplayTag Gameplay = ChannelGameplayTag;
	const FGameplayTag UI		= ChannelUITag;

	int32 GameplayCount = 0;
	int32 UICount = 0;
	const FVector Value = FVector::ZeroVector;
	
	auto GameplayCallback = TGameplayEventWithTagDelegate<FVector>::CreateLambda([&GameplayCount](FGameplayTag, FVector){ ++GameplayCount; });
	auto UICallback = TGameplayEventWithTagDelegate<FVector>::CreateLambda([&UICount](FGameplayTag, FVector) { ++UICount; });
	
	Subsystem->AddReceiver(Gameplay, GameplayCallback);
	Subsystem->SendEvent(Gameplay, Value);

	UTEST_EQUAL(TEXT("GameplayCount"), GameplayCount, 1);
	UTEST_EQUAL(TEXT("UICount"), UICount, 0);

	Subsystem->AddReceiver(UI, UICallback);
	Subsystem->SendEvent(UI, Value);

	UTEST_EQUAL(TEXT("GameplayCount"), GameplayCount, 1);
	UTEST_EQUAL(TEXT("UICount"), UICount, 1);

	Subsystem->AddReceiver(Gameplay, GameplayCallback);
	Subsystem->SendEvent(Gameplay, Value);
	
	UTEST_EQUAL(TEXT("GameplayCount"), GameplayCount, 3);
	UTEST_EQUAL(TEXT("UICount"), UICount, 1);
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayEventTest_SubChannels, "GameplayEvents.SubChannels", AutomationFlags | EAutomationTestFlags::MediumPriority)

bool FGameplayEventTest_SubChannels::RunTest(const FString& Parameters)
{
	// disable non-leaf event channels to test them
	IConsoleVariable* AllowNonLeafEventChannels = IConsoleManager::Get().FindConsoleVariable(TEXT("GameplayEvents.AllowSendingNonLeafEventChannels"));
	const bool bAllowNonLeafEventChannels = AllowNonLeafEventChannels->GetBool();
	AllowNonLeafEventChannels->Set(false);

	IConsoleVariable* DumpCallstack = IConsoleManager::Get().FindConsoleVariable(TEXT("GameplayEvents.ShouldDumpCallstack"));
	bool bDumpCallstack = DumpCallstack->GetBool();
	DumpCallstack->Set(false);

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	UGameplayEventSubsystem* Subsystem = NewObject<UGameplayEventSubsystem>(GameInstance, NAME_None, RF_Transient);
	
	AddExpectedError(TEXT("Broadcasting non-leaf tags is disabled."), EAutomationExpectedErrorFlags::Contains, 2);

	// should produce an expected error
	Subsystem->SendEvent(ChannelParent, FVector::ZeroVector);
	// should produce an expected error
	Subsystem->SendEvent(ChannelChild, FVector::ZeroVector);
	// fine
	Subsystem->SendEvent(ChannelLeaf, FVector::ZeroVector);

	AllowNonLeafEventChannels->Set(bAllowNonLeafEventChannels);
	DumpCallstack->Set(bDumpCallstack);
	
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayEventTest_EventLogging, "GameplayEvents.EventLogging", AutomationFlags | EAutomationTestFlags::MediumPriority)

bool FGameplayEventTest_EventLogging::RunTest(const FString& Parameters)
{
	// enable gameplay event logging
	IConsoleVariable* LogEvents = IConsoleManager::Get().FindConsoleVariable(TEXT("GameplayEvents.LogEvents"));
	check(LogEvents);
	const bool bLogEvents = LogEvents->GetBool();
	LogEvents->Set(true);

	UGameplayEventSubsystem* Subsystem = CreateSubsystem();
	const FGameplayTag Gameplay = ChannelGameplayTag;
	const FGameplayTag UI = ChannelUITag;
	
	AddExpectedMessage(TEXT("Tests\\.GameplayEvents\\.Channel\\.Gameplay.+Immediate"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
	Subsystem->SendEvent(Gameplay, FVector::ZeroVector);
	
	AddExpectedMessage(TEXT("Tests\\.GameplayEvents\\.Channel\\.UI.+Async"), ELogVerbosity::Display, EAutomationExpectedMessageFlags::Contains, 1);
	Subsystem->SendEventDelayed(UI, FVector::ZeroVector);
	Subsystem->Tick(1.f);
	
	LogEvents->Set(bLogEvents);
	
	return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayEventTest_ActualChannels, "GameplayEvents.ActualChannels", AutomationFlags | EAutomationTestFlags::MediumPriority)

bool FGameplayEventTest_ActualChannels::RunTest(const FString& Parameters)
{
	UGameplayEventSubsystem* Subsystem = CreateSubsystem();

	int32 Count = 0;
	FVector Value = FVector::ZeroVector;
	FGameplayTag Channel = FGameplayTag::EmptyTag;

	// allow non-leaf event channels to test them
	IConsoleVariable* AllowNonLeafEventChannels = IConsoleManager::Get().FindConsoleVariable(TEXT("GameplayEvents.AllowSendingNonLeafEventChannels"));
	const bool bAllowNonLeafEventChannels = AllowNonLeafEventChannels->GetBool();
	AllowNonLeafEventChannels->Set(true);
	
	const auto Callback = TGameplayEventWithTagDelegate<FVector>::CreateLambda([&](FGameplayTag InChannel, const FVector& InValue)
	{
		Channel = InChannel; Value = InValue; ++Count;
	});

	Count = 0;
	Value = FVector::ZeroVector;
	Channel = FGameplayTag::EmptyTag;
		
	FGameplayEventHandle Handle = Subsystem->AddReceiver(ChannelParent, Callback);

	{
		Subsystem->SendEvent(ChannelParent, FVector{1, 1, 1});
		UTEST_TRUE(TEXT("Count"), Count == 1);
		UTEST_TRUE(TEXT("Value"), (Value == FVector{1, 1, 1}));
		UTEST_TRUE(TEXT("Channel"), (Channel.MatchesTagExact(ChannelParent)));
	}

	{
		Subsystem->SendEvent(ChannelChild, FVector{2, 2, 2});
		UTEST_TRUE(TEXT("Count"), Count == 2);
		UTEST_TRUE(TEXT("Value"), (Value == FVector{2, 2, 2}));
		UTEST_TRUE(TEXT("Channel"), (Channel.MatchesTagExact(ChannelChild)));
	}

	{
		Subsystem->SendEvent(ChannelLeaf, FVector{3, 3, 3});
		UTEST_TRUE(TEXT("Count"), Count == 3);
		UTEST_TRUE(TEXT("Value"), (Value == FVector{3, 3, 3}));
		UTEST_TRUE(TEXT("Channel"), (Channel.MatchesTagExact(ChannelLeaf)));
	}
	Subsystem->RemoveReceiver(Handle);

	Count = 0;
	Value = FVector::ZeroVector;
	Channel = FGameplayTag::EmptyTag;

	Handle = Subsystem->AddReceiver(ChannelChild, Callback);
	{
		Subsystem->SendEvent(ChannelParent, FVector{1, 1, 1});
		UTEST_TRUE(TEXT("Count"), Count == 0);
		UTEST_TRUE(TEXT("Value"), (Value == FVector::ZeroVector));
		UTEST_TRUE(TEXT("Channel"), (Channel == FGameplayTag::EmptyTag));
	}

	{
		Subsystem->SendEvent(ChannelChild, FVector{2, 2, 2});
		UTEST_TRUE(TEXT("Count"), Count == 1);
		UTEST_TRUE(TEXT("Value"), (Value == FVector{2, 2, 2}));
		UTEST_TRUE(TEXT("Channel"), (Channel.MatchesTagExact(ChannelChild)));
	}

	{
		Subsystem->SendEvent(ChannelLeaf, FVector{3, 3, 3});
		UTEST_TRUE(TEXT("Count"), Count == 2);
		UTEST_TRUE(TEXT("Value"), (Value == FVector{3, 3, 3}));
		UTEST_TRUE(TEXT("Channel"), (Channel.MatchesTagExact(ChannelLeaf)));
	}
	Subsystem->RemoveReceiver(Handle);

	Count = 0;
	Value = FVector::ZeroVector;
	Channel = FGameplayTag::EmptyTag;

	Handle = Subsystem->AddReceiver(ChannelLeaf, Callback);
	{
		Subsystem->SendEvent(ChannelParent, FVector{1, 1, 1});
		UTEST_TRUE(TEXT("Count"), Count == 0);
		UTEST_TRUE(TEXT("Value"), (Value == FVector::ZeroVector));
		UTEST_TRUE(TEXT("Channel"), (Channel == FGameplayTag::EmptyTag));
	}

	{
		Subsystem->SendEvent(ChannelChild, FVector{2, 2, 2});
		UTEST_TRUE(TEXT("Count"), Count == 0);
		UTEST_TRUE(TEXT("Value"), (Value == FVector::ZeroVector));
		UTEST_TRUE(TEXT("Channel"), (Channel == FGameplayTag::EmptyTag));
	}

	{
		Subsystem->SendEvent(ChannelLeaf, FVector{3, 3, 3});
		UTEST_TRUE(TEXT("Count"), Count == 1);
		UTEST_TRUE(TEXT("Value"), (Value == FVector{3, 3, 3}));
		UTEST_TRUE(TEXT("Channel"), (Channel.MatchesTagExact(ChannelLeaf)));
	}
	Subsystem->RemoveReceiver(Handle);

	AllowNonLeafEventChannels->Set(bAllowNonLeafEventChannels);

	return !HasAnyErrors();
}
