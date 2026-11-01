using UnrealBuildTool;
using System.IO;

public class BtMmoClient : ModuleRules
{
	public BtMmoClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Networking", "SlateCore" });
		PublicIncludePaths.Add(Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../BtMmoProject/Shared")));

		PublicIncludePaths.AddRange(new string[] {
			"BtMmoClient",
			"BtMmoClient/Variant_Platforming",
			"BtMmoClient/Variant_Platforming/Animation",
			"BtMmoClient/Variant_Combat",
			"BtMmoClient/Variant_Combat/AI",
			"BtMmoClient/Variant_Combat/Animation",
			"BtMmoClient/Variant_Combat/Gameplay",
			"BtMmoClient/Variant_Combat/Interfaces",
			"BtMmoClient/Variant_Combat/UI",
			"BtMmoClient/Variant_SideScrolling",
			"BtMmoClient/Variant_SideScrolling/AI",
			"BtMmoClient/Variant_SideScrolling/Gameplay",
			"BtMmoClient/Variant_SideScrolling/Interfaces",
			"BtMmoClient/Variant_SideScrolling/UI"
		});

	}
}
