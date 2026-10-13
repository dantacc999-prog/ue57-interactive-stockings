#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StockingsViewerWidget.generated.h"
class USkeletalMeshComponent;class UMaterialInterface;class AActor;

// Minimal host adapter. Projects may use their own Widget BP with this contract.
UCLASS(Blueprintable)
class STOCKINGSINTERACTION_API UStockingsViewerWidget : public UUserWidget {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable) void InitializeViewer(USkeletalMeshComponent* Body,USkeletalMeshComponent* Stockings,AActor* Camera);
 UFUNCTION(BlueprintCallable) void CycleStockings();
 UFUNCTION(BlueprintCallable) void ApplyWardrobeVisibility();
 UFUNCTION(BlueprintCallable) void CameraReset();
 UFUNCTION(BlueprintCallable) void CameraZoom(float WheelDelta);
 UFUNCTION(BlueprintCallable) void ToggleShoes();
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<USkeletalMeshComponent> SpeechMouthMesh;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<USkeletalMeshComponent> StockingsMesh;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<USkeletalMeshComponent> WardrobeOutlineMesh;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UMaterialInterface> StockingsOriginalBody;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool ChatUIActive=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool DuckSitSelected=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 StockingsState=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool WardrobeShoesWorn=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool WardrobeTopWorn=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool WardrobeSkirtWorn=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool WardrobeSocksWorn=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 BodySection=2;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 TopSection=5;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 SkirtSection=6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 SocksSection=7;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 ShoesSection=8;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<AActor> CameraViewActor;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool CameraReady=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool CameraDragging=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector CameraModeLocation;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector CameraForward;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector CameraRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector CameraUp;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector CameraPanOffset;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraBaseDistance=147;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraZoomFactor=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraOrbitYaw=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraOrbitPitch=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraOrbitCurrentYaw=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraOrbitCurrentPitch=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraPanX=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraPanY=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 CameraDragButton=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraMouseX=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) float CameraMouseY=0;
protected:
 virtual void NativeOnInitialized() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
 virtual FReply NativeOnMouseWheel(const FGeometry& Geometry,const FPointerEvent& Event) override;
};
