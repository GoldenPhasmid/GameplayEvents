#include "GameplayEventHandle.h"

#include "GameplayEventSubsystem.h"

bool FGameplayEventHandle::IsValid() const
{
	return HandleID != 0 && ::IsValid(Subsystem);
}

void FGameplayEventHandle::Invalidate() const
{
	HandleID = 0;
	Subsystem = nullptr;
}
