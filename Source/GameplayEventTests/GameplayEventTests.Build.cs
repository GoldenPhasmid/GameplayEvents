using UnrealBuildTool;

public class GameplayEventTests : ModuleRules
{
    public GameplayEventTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "GameplayEvents",
                "GameplayTags",
            }
        );

        if (Target.Version.MinorVersion >= 5)
        {
            PublicDefinitions.Add("AUTOTEST_APPLICATION_MASK=EAutomationTestFlags_ApplicationContextMask");
        }
        else
        {
            PublicDefinitions.Add("AUTOTEST_APPLICATION_MASK=EAutomationTestFlags::ApplicationContextMask");
        }
    }
}