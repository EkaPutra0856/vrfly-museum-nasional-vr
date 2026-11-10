using UnrealBuildTool;

public class Museum : ModuleRules
{
	public Museum(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		// Same dependency set as the Zoo module, so its VR / hologram / AR / path-guide code ports without changes.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "UISystem", "GameplayTags", "NarrativeSystem", "ExplorationSystem", "AIModule", "Niagara", "MediaAssets" });

		PrivateDependencyModuleNames.AddRange(new string[] { "HeadMountedDisplay", "XRBase", "NavigationSystem", "Json", "JsonUtilities", "ImageWriteQueue", "ImageWrapper", "LevelSequence", "MovieScene", "CinematicCamera", "ProceduralMeshComponent" });
	}
}
