#include "GameplayEventHandle.h"

#include "GameplayEventSubsystem.h"

bool FGameplayEventHandle::IsValid() const
{
	return HandleID != 0 && ::IsValid(Subsystem);
}

void FGameplayEventHandle::Reset()
{
	if (::IsValid(Subsystem))
	{
		Subsystem->RemoveReceiver(*this);
	}
}

void FGameplayEventHandle::Invalidate() const
{
	HandleID = 0;
	Subsystem = nullptr;
}
