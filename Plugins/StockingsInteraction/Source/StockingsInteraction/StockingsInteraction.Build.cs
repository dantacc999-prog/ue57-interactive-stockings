using UnrealBuildTool;
public class StockingsInteraction : ModuleRules {
 public StockingsInteraction(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","InputCore","UMG","ProceduralMeshComponent"});
  PrivateDependencyModuleNames.AddRange(new[]{"Slate","SlateCore","RenderCore","RHI"});
 }
}
