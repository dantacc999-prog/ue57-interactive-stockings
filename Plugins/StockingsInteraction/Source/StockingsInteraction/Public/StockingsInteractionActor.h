#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "StockingsInteractionActor.generated.h"

class UUserWidget;class UButton;class UTextBlock;class USkeletalMeshComponent;class UMaterialInterface;class UMaterialInstanceDynamic;class UWidget;class UPointLightComponent;
// Edges are retained for geometry diagnostics, not an elastic solver.
struct FClothDistanceConstraint {int32 A=0,B=0;float FittedLength=0;};
struct FClothBodyNode {FBox Bounds=FBox(ForceInit);int32 Start=0,Count=0,Left=-1,Right=-1;};
struct FStockingsVisualSlice {FVector Center=FVector::ZeroVector;float Radius[64]={};};

UCLASS()
class STOCKINGSINTERACTION_API AStockingsInteractionActor : public AActor {
 GENERATED_BODY()
public:
 AStockingsInteractionActor();
 virtual void Tick(float Dt) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UFUNCTION(BlueprintCallable) void ToggleInteraction();
 UFUNCTION(BlueprintCallable) void StopInteraction();
 UFUNCTION(BlueprintCallable) bool TryGrab(FVector Origin,FVector Direction);
 UFUNCTION(BlueprintCallable) void MoveGrab(FVector Origin,FVector Direction);
 UFUNCTION(BlueprintCallable) void ReleaseGrab();
 UFUNCTION(BlueprintCallable) void SetRemovalProgress(float Value);
 UFUNCTION(BlueprintCallable) void SetSimulationGripTarget(FVector LocalTarget);
 UFUNCTION(BlueprintPure) FVector GetGripPoint(int32 Side) const;
 UFUNCTION(BlueprintPure) bool CanInteract() const;
 UFUNCTION(BlueprintPure) FString GetSimulationStats() const;
 UFUNCTION(BlueprintPure) FString GetAcceptedValidationMesh() const;
 UFUNCTION(BlueprintPure) float GetCameraNearClipCm() const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Integration") TSubclassOf<UUserWidget> ViewerWidgetClass;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Integration") int32 BodyMaterialSection=2;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Integration") int32 InnerGarmentMaterialSection=4;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Integration") int32 SkirtMaterialSection=6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Integration") int32 SocksMaterialSection=7;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Integration") int32 ShoesMaterialSection=8;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Integration") FVector InnerGarmentBoundsMin=FVector(-11,-10,55);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Integration") FVector InnerGarmentBoundsMax=FVector(11,5,70);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Materials") TObjectPtr<UMaterialInterface> WhiteDragMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Materials") TObjectPtr<UMaterialInterface> BlackDragMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Materials") TObjectPtr<UMaterialInterface> InteractionSkinMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Materials") TObjectPtr<UMaterialInterface> WhiteSkinMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Materials") TObjectPtr<UMaterialInterface> OrdinaryBlackMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Materials") TObjectPtr<UMaterialInterface> OrdinaryWhiteMaterial;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Response") float SnapReturnRate=22.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Response") float AnkleGatherAmplitudeCm=.12f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Response") float AcceptedRenderClearanceCm=.18f;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool InteractionActive=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool Dragging=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float SlideCm=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float RemovalProgress=0;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool FullyRemoved=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool FitReady=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool StandingPoseApplied=false;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector GrabCenter=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UProceduralMeshComponent> ClothGeometry;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> ClothKeyLight;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> ClothFillLight;
private:
 void InitializeAcceptedInteraction();void StepAcceptedInteraction(float Dt);void BindAcceptedRender();
 FString GetAcceptedStats() const;
 void BuildAcceptedVisualAtlas();void BuildSwimBottomContact();bool ProjectSwimBottom(FVector& Position) const;
 FStockingsVisualSlice SampleAcceptedVisualSlice(int32 Side,float Height) const;
 FVector MapAcceptedVisualVertex(int32 Group,float Height,float Derivative,float Compression) const;
 void FindUI();void CreateControls();void CacheGripPoints();void UpdateCloth(float Dt);void MouseDown();void RestoreCloth();void UpdateOrdinaryPresentation();
 bool BuildClothGeometry();void DeformCloth();void InitializeSimulation();void RefreshBodyCollision();
 void BeginSimulationGrab(FVector WorldPoint);void EndSimulationGrab();bool ApplyStandingPose();void RestoreStandingPose();
 int32 BuildBodyNode(int32 Start,int32 Count);void RefitBodyNode(int32 Node);
 bool NearestBody(FVector P,FVector& Point,FVector& Normal,float& Distance,int32 Query=-1) const;
 UFUNCTION() void ReleaseFromMouse();UFUNCTION() void GrabLeft();UFUNCTION() void GrabRight();void GrabSide(int32 Side);
 UPROPERTY() TObjectPtr<UUserWidget> UI;
 UPROPERTY() TObjectPtr<UButton> ModeButton;
 UPROPERTY() TObjectPtr<UTextBlock> ModeLabel;
 UPROPERTY() TObjectPtr<UTextBlock> Hint;
 UPROPERTY() TArray<TObjectPtr<UButton>> Grips;
 UPROPERTY() TObjectPtr<USkeletalMeshComponent> Body;
 UPROPERTY() TObjectPtr<USkeletalMeshComponent> Stock;
 UPROPERTY() TObjectPtr<UMaterialInterface> OriginalCloth;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ClothMID;
 UPROPERTY() TObjectPtr<UMaterialInterface> BodyMaterialBeforeInteraction;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> InteractionSkinMID;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> OrdinarySheerMID;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> OrdinarySkinMID;
 UPROPERTY() TObjectPtr<UMaterialInterface> OrdinaryBodyBeforeSheer;
 UPROPERTY() TObjectPtr<UWidget> CameraArea;
 UPROPERTY() TObjectPtr<UClass> SavedPostProcessClass;
 UPROPERTY() TObjectPtr<UObject> SavedPrimaryInstance;
 bool SavedDisablePostProcess=false,CameraAreaWasEnabled=true,InputInstalled=false;
 int32 OrdinaryVariant=0,ActiveVariant=0;float FindDelay=0,SavedInteractionNearClipCm=0;
 FVector PlanePoint,PlaneNormal,StartCursor;
 TArray<FVector> GripCandidates;FVector GripPoints[2];
 float TopZ=70,BottomZ=0,BaseThickness=1;int32 SourceVertexCount=0;
 TArray<FVector> RestVertices,BaseVertices,ClothVertices,ClothNormals;
 TArray<FVector2D> ClothUV;TArray<int32> ClothTriangles,SourceA,SourceB,HemVertices,NormalGroups;
 float NormalSign=1;int32 NormalGroupCount=0;
 TArray<FProcMeshTangent> ClothTangents;TArray<FLinearColor> ClothColors;
 float AcceptedDrop=0,AcceptedTargetDrop=0,AcceptedGrabDrop=0,AcceptedAnkleRef=0,AcceptedMaxDrop=0;
 bool AcceptedDownGrab=false,AcceptedGeometryChanged=true,AcceptedBaselineValidated=false;
 FVector AcceptedPeel=FVector::ZeroVector;
 TArray<int32> AcceptedRenderRepresentatives,AcceptedParticleRender;
 TArray<FVector> AcceptedBaseRenderNormals,AcceptedBaseline;
 TArray<float> AcceptedPatchWeights,AcceptedBaselineArea,AcceptedVisualCompression;
 TArray<FStockingsVisualSlice> AcceptedVisualSlices[3];TArray<TArray<int32>> AcceptedVisualNeighbors;
 float AcceptedHipRef=60,AcceptedVisualHem=74;
 TArray<FVector> SwimBottomPositions,SwimBottomFaceOutward;TArray<int32> SwimBottomTriangles;
 TArray<FBox> SwimBottomFaceBounds;FBox SwimBottomBounds=FBox(ForceInit);int32 SwimBottomInitialCorrections=0;
 TArray<FVector> SimPositions,SimPrevious,SimRest,BodyPositions,BodyNormals;
 TArray<float> GrabWeights;
 TArray<int32> RenderParticle,BodySource,BodyTriangles,BodyOrder;
 TArray<FIntVector> SimTriangles;TArray<TArray<int32>> SimNeighbors;
 TArray<FClothDistanceConstraint> StretchConstraints;
 TArray<FClothBodyNode> BodyTree;mutable TArray<int32> BodyQuerySeeds;
 FVector SimGrabTarget=FVector::ZeroVector,SimGrabStart=FVector::ZeroVector;
 int32 GrabParticle=-1,BodyContactCount=0,SelfContactCount=0,PendingSimulationInit=0,ClearanceFailures=0;
 bool ManualSimulationGrip=false;
 float SimulationSeconds=0,SimTimeMs=0,MaxStretch=1,MinBodyClearance=0,MaxStepDisplacement=0;
};
