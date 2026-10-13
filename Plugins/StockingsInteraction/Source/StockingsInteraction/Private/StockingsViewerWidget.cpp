#include "StockingsViewerWidget.h"
#include "StockingsViewerCameraLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

void UStockingsViewerWidget::NativeOnInitialized(){
 Super::NativeOnInitialized();
 if(!WidgetTree->RootWidget){
  auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Root;
  auto AddButton=[&](const TCHAR* Name,const TCHAR* Label,float Y){
   auto* B=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);
   if(auto* P=FindFProperty<FBoolProperty>(B->GetClass(),TEXT("IsFocusable")))P->SetPropertyValue_InContainer(B,false);
   auto* T=WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(Label));B->AddChild(T);
   auto* S=Root->AddChildToCanvas(B);S->SetPosition(FVector2D(25,Y));S->SetSize(FVector2D(190,42));return B;
  };
  AddButton(TEXT("WardrobeModeButton"),TEXT("White / Black / None"),25)->OnClicked.AddDynamic(this,&UStockingsViewerWidget::CycleStockings);
  AddButton(TEXT("ShoesButton"),TEXT("Shoes on / off"),75)->OnClicked.AddDynamic(this,&UStockingsViewerWidget::ToggleShoes);
  AddButton(TEXT("ResetCameraButton"),TEXT("Reset camera"),125)->OnClicked.AddDynamic(this,&UStockingsViewerWidget::CameraReset);
 }
}
void UStockingsViewerWidget::InitializeViewer(USkeletalMeshComponent* Body,USkeletalMeshComponent* Stockings,AActor* Camera){
 SpeechMouthMesh=Body;StockingsMesh=Stockings;CameraViewActor=Camera;
 if(Body){StockingsOriginalBody=Body->GetMaterial(BodySection);if(Stockings)Stockings->SetLeaderPoseComponent(Body);}
 if(Camera){CameraModeLocation=Camera->GetActorLocation();CameraForward=Camera->GetActorForwardVector();CameraRight=Camera->GetActorRightVector();CameraUp=Camera->GetActorUpVector();CameraReady=true;}
 if(auto* PC=GetOwningPlayer()){if(Camera)PC->SetViewTarget(Camera);PC->bShowMouseCursor=true;FInputModeGameAndUI Input;Input.SetHideCursorDuringCapture(false);PC->SetInputMode(Input);}
 ApplyWardrobeVisibility();
}
void UStockingsViewerWidget::CycleStockings(){StockingsState=(StockingsState+1)%3;ApplyWardrobeVisibility();}
void UStockingsViewerWidget::ApplyWardrobeVisibility(){
 if(StockingsMesh)StockingsMesh->SetVisibility(StockingsState!=0);
 for(auto* M:{SpeechMouthMesh.Get(),WardrobeOutlineMesh.Get()})if(M){
  M->ShowMaterialSection(TopSection,-1,WardrobeTopWorn,0);M->ShowMaterialSection(SkirtSection,-1,WardrobeSkirtWorn,0);
  M->ShowMaterialSection(SocksSection,-1,WardrobeSocksWorn,0);M->ShowMaterialSection(ShoesSection,-1,WardrobeShoesWorn,0);
 }
}
void UStockingsViewerWidget::ToggleShoes(){WardrobeShoesWorn=!WardrobeShoesWorn;ApplyWardrobeVisibility();}
void UStockingsViewerWidget::CameraReset(){CameraZoomFactor=1;UStockingsViewerCameraLibrary::ResetViewerCamera(this);}
void UStockingsViewerWidget::CameraZoom(float WheelDelta){CameraZoomFactor=FMath::Clamp(CameraZoomFactor-WheelDelta*.1f,.08f,2.2f);}
void UStockingsViewerWidget::NativeTick(const FGeometry& G,float Dt){Super::NativeTick(G,Dt);UStockingsViewerCameraLibrary::UpdateViewerCameraInput(this);UStockingsViewerCameraLibrary::ApplyViewerCamera(this,Dt);}
FReply UStockingsViewerWidget::NativeOnMouseWheel(const FGeometry& G,const FPointerEvent& E){CameraZoom(E.GetWheelDelta());return FReply::Handled();}
