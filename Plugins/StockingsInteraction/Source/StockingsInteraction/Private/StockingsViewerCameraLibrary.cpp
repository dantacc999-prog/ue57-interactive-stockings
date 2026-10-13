#include "StockingsViewerCameraLibrary.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Math/RotationMatrix.h"
#include "UObject/UnrealType.h"
#include "InputCoreTypes.h"

namespace ViewerCamera
{
 bool Flag(UObject* O, const TCHAR* N)
 {
  const auto* P=O?FindFProperty<FBoolProperty>(O->GetClass(),N):nullptr;
  return P && P->GetPropertyValue_InContainer(O);
 }
 void SetFlag(UObject* O, const TCHAR* N, bool V)
 {
  if(auto* P=O?FindFProperty<FBoolProperty>(O->GetClass(),N):nullptr)P->SetPropertyValue_InContainer(O,V);
 }
 double Number(UObject* O,const TCHAR* N)
 {
  auto* P=O?FindFProperty<FNumericProperty>(O->GetClass(),N):nullptr;
  if(!P)return 0;
  const void* Value=P->ContainerPtrToValuePtr<void>(O);
  return P->IsFloatingPoint()?P->GetFloatingPointPropertyValue(Value):P->GetSignedIntPropertyValue(Value);
 }
 void SetNumber(UObject* O,const TCHAR* N,double V)
 {
  if(auto* P=O?FindFProperty<FNumericProperty>(O->GetClass(),N):nullptr)
  {
   void* Value=P->ContainerPtrToValuePtr<void>(O);
   if(P->IsFloatingPoint())P->SetFloatingPointPropertyValue(Value,V);
   else P->SetIntPropertyValue(Value,static_cast<int64>(V));
  }
 }
 FVector Vector(UObject* O,const TCHAR* N)
 {
  auto* P=O?FindFProperty<FStructProperty>(O->GetClass(),N):nullptr;
  return P && P->Struct==TBaseStructure<FVector>::Get()?*P->ContainerPtrToValuePtr<FVector>(O):FVector::ZeroVector;
 }
 void SetVector(UObject* O,const TCHAR* N,FVector V)
 {
  if(auto* P=O?FindFProperty<FStructProperty>(O->GetClass(),N):nullptr)
   if(P->Struct==TBaseStructure<FVector>::Get())*P->ContainerPtrToValuePtr<FVector>(O)=V;
 }
 AActor* View(UObject* O)
 {
  auto* P=O?FindFProperty<FObjectPropertyBase>(O->GetClass(),TEXT("CameraViewActor")):nullptr;
  return P?Cast<AActor>(P->GetObjectPropertyValue_InContainer(O)):nullptr;
 }
 bool Ready(UUserWidget* W)
 {
  return IsValid(W) && ViewerCamera::Flag(W,TEXT("CameraReady")) && ViewerCamera::Flag(W,TEXT("ChatUIActive")) && IsValid(View(W));
 }
 void BasisAngles(UUserWidget* W,double Yaw,double Pitch,FVector& Forward,FVector& Right,FVector& Up)
 {
  const FVector BaseRight=Vector(W,TEXT("CameraRight")).GetSafeNormal();
  Forward=Vector(W,TEXT("CameraForward")).GetSafeNormal().RotateAngleAxis(-Pitch,BaseRight).RotateAngleAxis(Yaw,FVector::UpVector).GetSafeNormal();
  Right=BaseRight.RotateAngleAxis(Yaw,FVector::UpVector).GetSafeNormal();
  Up=FVector::CrossProduct(Forward,Right).GetSafeNormal();
 }
 void Basis(UUserWidget* W,FVector& Forward,FVector& Right,FVector& Up)
 {
  BasisAngles(W,ViewerCamera::Number(W,TEXT("CameraOrbitCurrentYaw")),ViewerCamera::Number(W,TEXT("CameraOrbitCurrentPitch")),Forward,Right,Up);
 }
 double Distance(UUserWidget* W)
 {
  return FMath::Max(ViewerCamera::Number(W,TEXT("CameraBaseDistance"))*ViewerCamera::Number(W,TEXT("CameraZoomFactor")),20.);
 }
}

void UStockingsViewerCameraLibrary::ResetViewerCamera(UUserWidget* W)
{
 if(!W)return;
 ViewerCamera::SetNumber(W,TEXT("CameraOrbitYaw"),0);
 ViewerCamera::SetNumber(W,TEXT("CameraOrbitPitch"),0);
 ViewerCamera::SetNumber(W,TEXT("CameraOrbitCurrentYaw"),0);
 ViewerCamera::SetNumber(W,TEXT("CameraOrbitCurrentPitch"),0);
 ViewerCamera::SetVector(W,TEXT("CameraPanOffset"),FVector::ZeroVector);
 ViewerCamera::SetNumber(W,TEXT("CameraPanX"),0);
 ViewerCamera::SetNumber(W,TEXT("CameraPanY"),0);
 ViewerCamera::SetNumber(W,TEXT("CameraDragButton"),0);
 ViewerCamera::SetFlag(W,TEXT("CameraDragging"),false);
}

void UStockingsViewerCameraLibrary::OrbitViewerCamera(UUserWidget* W,double DX,double DY)
{
 if(!ViewerCamera::Ready(W) || !FMath::IsFinite(DX) || !FMath::IsFinite(DY))return;
 ViewerCamera::SetNumber(W,TEXT("CameraOrbitYaw"),FMath::UnwindDegrees(ViewerCamera::Number(W,TEXT("CameraOrbitYaw"))+DX*.25));
 ViewerCamera::SetNumber(W,TEXT("CameraOrbitPitch"),FMath::Clamp(ViewerCamera::Number(W,TEXT("CameraOrbitPitch"))-DY*.25,-80.,80.));
}

void UStockingsViewerCameraLibrary::PanViewerCamera(UUserWidget* W,double DX,double DY)
{
 if(!ViewerCamera::Ready(W) || !FMath::IsFinite(DX) || !FMath::IsFinite(DY))return;
 auto* PC=W->GetOwningPlayer();if(!PC)return;
 int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
 const double Scale=ViewerCamera::Distance(W)*1.534654/FMath::Max(Width,1);
 FVector Forward,Right,Up;ViewerCamera::Basis(W,Forward,Right,Up);
 // Keep a world-space focus offset so rotation stays centered on the panned detail.
 const FVector Offset=(ViewerCamera::Vector(W,TEXT("CameraPanOffset"))-Right*(DX*Scale)+Up*(DY*Scale)).GetClampedToMaxSize(250.);
 ViewerCamera::SetVector(W,TEXT("CameraPanOffset"),Offset);
 ViewerCamera::SetNumber(W,TEXT("CameraPanX"),FVector::DotProduct(Offset,ViewerCamera::Vector(W,TEXT("CameraRight"))));
 ViewerCamera::SetNumber(W,TEXT("CameraPanY"),FVector::DotProduct(Offset,ViewerCamera::Vector(W,TEXT("CameraUp"))));
}

void UStockingsViewerCameraLibrary::ProcessViewerCameraPointer(UUserWidget* W,double X,double Y,bool Valid,bool Middle,bool Right,bool Left)
{
 if(!W)return;
 int32 Button=static_cast<int32>(ViewerCamera::Number(W,TEXT("CameraDragButton")));
 const bool Held=Button==1?Middle:Button==2?Right:false;
 if(!ViewerCamera::Ready(W) || !Valid || Left || (Button!=0 && !Held))
 {
  ViewerCamera::SetNumber(W,TEXT("CameraDragButton"),0);ViewerCamera::SetFlag(W,TEXT("CameraDragging"),false);return;
 }
 if(Button==0)
 {
  auto* PC=W->GetOwningPlayer();if(!PC)return;
  int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
  // Preserve the existing viewer region, excluding the chat input on the right.
  if(X<0 || Y<0 || X>=Width*.655 || Y>=Height || (!Middle && !Right))return;
  Button=Middle?1:2;
  ViewerCamera::SetNumber(W,TEXT("CameraDragButton"),Button);
  ViewerCamera::SetFlag(W,TEXT("CameraDragging"),true);
 }
 else
 {
  const double DX=X-ViewerCamera::Number(W,TEXT("CameraMouseX")),DY=Y-ViewerCamera::Number(W,TEXT("CameraMouseY"));
  if(Button==1)OrbitViewerCamera(W,DX,DY);else PanViewerCamera(W,DX,DY);
 }
 ViewerCamera::SetNumber(W,TEXT("CameraMouseX"),X);ViewerCamera::SetNumber(W,TEXT("CameraMouseY"),Y);
}

void UStockingsViewerCameraLibrary::UpdateViewerCameraInput(UUserWidget* W)
{
 if(!W)return;
 auto* PC=W->GetOwningPlayer();if(!PC)return;
 float X=0,Y=0;const bool Valid=PC->GetMousePosition(X,Y);
 ProcessViewerCameraPointer(W,X,Y,Valid,PC->IsInputKeyDown(EKeys::MiddleMouseButton),PC->IsInputKeyDown(EKeys::RightMouseButton),PC->IsInputKeyDown(EKeys::LeftMouseButton));
}

void UStockingsViewerCameraLibrary::ApplyViewerCamera(UUserWidget* W,float DT)
{
 if(!ViewerCamera::Ready(W))return;
 auto* View=ViewerCamera::View(W);
 const double Alpha=FMath::Clamp(double(DT)*9.,0.,1.);
 const double CurrentYaw=ViewerCamera::Number(W,TEXT("CameraOrbitCurrentYaw"));
 const double Yaw=FMath::UnwindDegrees(CurrentYaw+FMath::FindDeltaAngleDegrees(CurrentYaw,ViewerCamera::Number(W,TEXT("CameraOrbitYaw")))*Alpha);
 const double Pitch=FMath::Lerp(ViewerCamera::Number(W,TEXT("CameraOrbitCurrentPitch")),ViewerCamera::Number(W,TEXT("CameraOrbitPitch")),Alpha);
 ViewerCamera::SetNumber(W,TEXT("CameraOrbitCurrentYaw"),Yaw);
 ViewerCamera::SetNumber(W,TEXT("CameraOrbitCurrentPitch"),Pitch);
 FVector Forward,Right,Up;ViewerCamera::Basis(W,Forward,Right,Up);
 const FVector Focus=ViewerCamera::Vector(W,TEXT("CameraModeLocation"))
  +ViewerCamera::Vector(W,TEXT("CameraForward"))*ViewerCamera::Number(W,TEXT("CameraBaseDistance"))
  +ViewerCamera::Vector(W,TEXT("CameraPanOffset"));
 // Interpolate angle and radius, rather than cutting a chord through the model.
 const double Distance=FMath::Lerp(FVector::Distance(View->GetActorLocation(),Focus),ViewerCamera::Distance(W),Alpha);
 const FVector Location=Focus-Forward*Distance;
 const FVector Look=(Focus-Location).GetSafeNormal();
 if(!Look.IsNearlyZero())View->SetActorLocationAndRotation(Location,FRotationMatrix::MakeFromXZ(Look,Up).Rotator(),false,nullptr,ETeleportType::TeleportPhysics);
}
