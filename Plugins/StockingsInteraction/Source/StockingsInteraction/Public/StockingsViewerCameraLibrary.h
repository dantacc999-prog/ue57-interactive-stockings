#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StockingsViewerCameraLibrary.generated.h"

class UUserWidget;

UCLASS()
class STOCKINGSINTERACTION_API UStockingsViewerCameraLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Viewer Camera", meta=(DefaultToSelf="Widget"))
 static void UpdateViewerCameraInput(UUserWidget* Widget);
 UFUNCTION(BlueprintCallable, Category="Viewer Camera", meta=(DefaultToSelf="Widget"))
 static void ApplyViewerCamera(UUserWidget* Widget, float DeltaTime);
 UFUNCTION(BlueprintCallable, Category="Viewer Camera", meta=(DefaultToSelf="Widget"))
 static void OrbitViewerCamera(UUserWidget* Widget, double DeltaX, double DeltaY);
 UFUNCTION(BlueprintCallable, Category="Viewer Camera", meta=(DefaultToSelf="Widget"))
 static void PanViewerCamera(UUserWidget* Widget, double DeltaX, double DeltaY);
 UFUNCTION(BlueprintCallable, Category="Viewer Camera", meta=(DefaultToSelf="Widget"))
 static void ResetViewerCamera(UUserWidget* Widget);
 // Shared by physical input and deterministic input-sequence verification.
 UFUNCTION(BlueprintCallable, Category="Viewer Camera", meta=(DefaultToSelf="Widget"))
 static void ProcessViewerCameraPointer(UUserWidget* Widget, double X, double Y,
  bool PositionValid, bool MiddleDown, bool RightDown, bool LeftDown);
};
