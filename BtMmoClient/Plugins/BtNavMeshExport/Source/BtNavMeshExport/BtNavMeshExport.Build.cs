using UnrealBuildTool;

public class BtNavMeshExport : ModuleRules
{
    public BtNavMeshExport(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "UnrealEd", "NavigationSystem", "Navmesh", "Json"
        });
    }
}
