#include "StockingsInteractionActor.h"
#include "Modules/ModuleManager.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "UObject/UnrealType.h"
#include "CoreGlobals.h"
#include "RenderCore.h"
IMPLEMENT_MODULE(FDefaultModuleImpl,StockingsInteraction)
template<class T>static T* Obj(UObject* O,const TCHAR* Name){auto*P=O?FindFProperty<FObjectPropertyBase>(O->GetClass(),Name):nullptr;return P?Cast<T>(P->GetObjectPropertyValue_InContainer(O)):nullptr;}
static bool Flag(UObject*O,const TCHAR*N){auto*P=O?FindFProperty<FBoolProperty>(O->GetClass(),N):nullptr;return P&&P->GetPropertyValue_InContainer(O);}
static int32 Number(UObject*O,const TCHAR*N){auto*P=O?FindFProperty<FIntProperty>(O->GetClass(),N):nullptr;return P?P->GetPropertyValue_InContainer(O):0;}
static void Call(UObject*O,const TCHAR*N){if(O)if(auto*F=O->FindFunction(N))O->ProcessEvent(F,nullptr);}
static void SetFlag(UObject*O,const TCHAR*N,bool V){if(O)if(auto*P=FindFProperty<FBoolProperty>(O->GetClass(),N))P->SetPropertyValue_InContainer(O,V);}
static void SetNumber(UObject*O,const TCHAR*N,int32 V){if(O)if(auto*P=FindFProperty<FIntProperty>(O->GetClass(),N))P->SetPropertyValue_InContainer(O,V);}
static bool Plane(FVector O,FVector D,FVector P,FVector N,FVector&Hit){float Den=FVector::DotProduct(D,N);if(FMath::Abs(Den)<.0001f)return false;float T=FVector::DotProduct(P-O,N)/Den;if(T<=0)return false;Hit=O+D*T;return true;}
AStockingsInteractionActor::AStockingsInteractionActor(){
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("InteractionRoot")));
 ClothGeometry=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SlidingStockings"));ClothGeometry->SetupAttachment(RootComponent);ClothGeometry->SetCollisionEnabled(ECollisionEnabled::NoCollision);ClothGeometry->SetVisibility(false);ClothGeometry->SetLightingChannels(true,true,false);
 ClothKeyLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("ClothKeyLight"));ClothKeyLight->SetupAttachment(ClothGeometry);ClothKeyLight->SetRelativeLocation(FVector(-35,65,72));
 ClothFillLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("ClothFillLight"));ClothFillLight->SetupAttachment(ClothGeometry);ClothFillLight->SetRelativeLocation(FVector(40,45,45));
 for(auto*L:{ClothKeyLight.Get(),ClothFillLight.Get()}){L->SetMobility(EComponentMobility::Movable);L->SetIntensityUnits(ELightUnits::Lumens);L->SetAttenuationRadius(220);L->SetLightingChannels(false,true,false);L->SetCastShadows(false);L->SetVisibility(false);L->SourceRadius=12;}
 ClothKeyLight->SetIntensity(.12f);ClothFillLight->SetIntensity(.06f);ClothKeyLight->SetIndirectLightingIntensity(0);ClothFillLight->SetIndirectLightingIntensity(0);
}
void AStockingsInteractionActor::FindUI(){
 auto*Class=ViewerWidgetClass.Get();if(!Class)return;
 TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,Class,true);if(Widgets.IsEmpty())return;
 UI=Widgets[0];CreateControls();
 if(auto*PC=UI->GetOwningPlayer()){EnableInput(PC);InputComponent->bBlockInput=false;
  auto&Press=InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AStockingsInteractionActor::MouseDown);Press.bConsumeInput=false;
  auto&Release=InputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&AStockingsInteractionActor::ReleaseFromMouse);Release.bConsumeInput=false;InputInstalled=true;
 }
}
void AStockingsInteractionActor::CreateControls(){
 auto*Root=Cast<UCanvasPanel>(UI->WidgetTree->RootWidget);if(!Root)return;
 auto*Source=Cast<UButton>(UI->GetWidgetFromName(TEXT("WardrobeModeButton")));
 ModeButton=UI->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("StockingsInteractionButton"));SetFlag(ModeButton,TEXT("IsFocusable"),false);if(Source)ModeButton->SetStyle(Source->GetStyle());
 ModeLabel=UI->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("StockingsInteractionLabel"));
 if(auto*T=Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("WardrobeModeLabel")))){ModeLabel->SetFont(T->GetFont());ModeLabel->SetColorAndOpacity(T->GetColorAndOpacity());}
 ModeLabel->SetText(FText::FromString(TEXT("互动模式")));ModeButton->AddChild(ModeLabel);
 auto*S=Root->AddChildToCanvas(ModeButton);S->SetAnchors(FAnchors(.025f,.10f));S->SetPosition(FVector2D(238,0));S->SetSize(FVector2D(170,44));S->SetZOrder(5);
 ModeButton->OnClicked.AddDynamic(this,&AStockingsInteractionActor::ToggleInteraction);
 Hint=UI->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("StockingsInteractionHint"));Hint->SetFont(ModeLabel->GetFont());Hint->SetColorAndOpacity(FLinearColor(.8,1,.96,1));Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
 S=Root->AddChildToCanvas(Hint);S->SetAnchors(FAnchors(.025f,.10f));S->SetPosition(FVector2D(238,50));S->SetSize(FVector2D(600,45));S->SetZOrder(5);
 for(int32 I=0;I<2;++I){auto*B=UI->WidgetTree->ConstructWidget<UButton>();SetFlag(B,TEXT("IsFocusable"),false);if(Source)B->SetStyle(Source->GetStyle());B->SetBackgroundColor(FLinearColor(.15,1,.9,.75));auto*T=UI->WidgetTree->ConstructWidget<UTextBlock>();T->SetText(FText::FromString(TEXT("●")));T->SetFont(ModeLabel->GetFont());T->SetColorAndOpacity(FLinearColor(.4,1,1,1));B->AddChild(T);S=Root->AddChildToCanvas(B);S->SetSize(FVector2D(22,22));S->SetZOrder(6);B->SetVisibility(ESlateVisibility::Collapsed);Grips.Add(B);B->OnReleased.AddDynamic(this,&AStockingsInteractionActor::ReleaseFromMouse);}
 Grips[0]->OnPressed.AddDynamic(this,&AStockingsInteractionActor::GrabLeft);Grips[1]->OnPressed.AddDynamic(this,&AStockingsInteractionActor::GrabRight);
 CameraArea=UI->GetWidgetFromName(TEXT("CameraDragArea"));
}
bool AStockingsInteractionActor::CanInteract()const{
 if(!IsValid(UI)||!IsValid(Body)||!Flag(UI,TEXT("ChatUIActive"))||Flag(UI,TEXT("DuckSitSelected")))return false;
 // Check the actual leg pose as well as the selected idle, so sit/stand transitions cannot enter.
 for(const TCHAR*Side:{TEXT("L"),TEXT("R")}){FVector H=Body->GetSocketLocation(FName(*(FString(TEXT("UpperLeg_"))+Side))),K=Body->GetSocketLocation(FName(*(FString(TEXT("LowerLeg_"))+Side))),F=Body->GetSocketLocation(FName(*(FString(TEXT("Foot_"))+Side)));if(FVector::DotProduct((K-H).GetSafeNormal(),(F-K).GetSafeNormal())<.15f)return false;}
 return true;
}
void AStockingsInteractionActor::ToggleInteraction(){
 if(InteractionActive){StopInteraction();return;}Body=Obj<USkeletalMeshComponent>(UI,TEXT("SpeechMouthMesh"));if(!CanInteract())return;
 if(Number(UI,TEXT("StockingsState"))==0)Call(UI,TEXT("CycleStockings"));
 Stock=Obj<USkeletalMeshComponent>(UI,TEXT("StockingsMesh"));ActiveVariant=Number(UI,TEXT("StockingsState"));if(!Stock||!ActiveVariant)return;
 auto*Mat=ActiveVariant==1?WhiteDragMaterial.Get():BlackDragMaterial.Get();if(!Mat)return;
 OriginalCloth=Stock->GetMaterial(0);BodyMaterialBeforeInteraction=Body->GetMaterial(BodyMaterialSection);ClothMID=UMaterialInstanceDynamic::Create(Mat,this);ClothMID->SetScalarParameterValue(TEXT("GrabActive"),0);OriginalCloth->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Thickness")),BaseThickness);
 if(auto*M=Obj<UMaterialInterface>(UI,TEXT("StockingsOriginalBody")))Body->SetMaterial(BodyMaterialSection,M);
 SlideCm=0;RemovalProgress=0;FullyRemoved=false;InteractionActive=true;Dragging=false;FitReady=false;SimulationSeconds=0;
 if(!ApplyStandingPose()){InteractionActive=false;Call(UI,TEXT("ApplyWardrobeVisibility"));return;}
 UMaterialInterface* SkinMaterial=ActiveVariant==1&&WhiteSkinMaterial?WhiteSkinMaterial.Get():InteractionSkinMaterial.Get();
 if(SkinMaterial){InteractionSkinMID=UMaterialInstanceDynamic::Create(SkinMaterial,this);InteractionSkinMID->SetScalarParameterValue(TEXT("SkinCoverageTopZWS"),-100000.f);Body->SetMaterial(BodyMaterialSection,InteractionSkinMID);}
 SavedInteractionNearClipCm=GNearClippingPlane;SetNearClipPlaneGlobals(FMath::Min(GNearClippingPlane,1.f));
 // Let the post-process pose and leader-following mesh evaluate before reading skinned vertices.
 PendingSimulationInit=2;ClothGeometry->SetVisibility(false);ClothKeyLight->SetVisibility(false);ClothFillLight->SetVisibility(false);
 // Viewer camera controls remain available during garment interaction.
 if(CameraArea)CameraAreaWasEnabled=CameraArea->GetIsEnabled();SetFlag(UI,TEXT("CameraDragging"),false);
 GripCandidates.Reset();UE_LOG(LogTemp,Display,TEXT("Stockings interaction ON variant=%d; awaiting fitting pose"),ActiveVariant);
}
void AStockingsInteractionActor::RestoreCloth(){
 Call(UI,TEXT("ApplyWardrobeVisibility"));
}
void AStockingsInteractionActor::StopInteraction(){
 if(!InteractionActive)return;InteractionActive=false;PendingSimulationInit=0;Dragging=false;SlideCm=0;
 if(SavedInteractionNearClipCm>0){SetNearClipPlaneGlobals(SavedInteractionNearClipCm);SavedInteractionNearClipCm=0;}
 RestoreStandingPose();ClothGeometry->SetVisibility(false);ClothKeyLight->SetVisibility(false);ClothFillLight->SetVisibility(false);if(Body&&BodyMaterialBeforeInteraction)Body->SetMaterial(BodyMaterialSection,BodyMaterialBeforeInteraction);InteractionSkinMID=nullptr;BodyMaterialBeforeInteraction=nullptr;RestoreCloth();
 if(CameraArea)CameraArea->SetIsEnabled(CameraAreaWasEnabled);SetFlag(UI,TEXT("CameraDragging"),false);for(auto B:Grips)B->SetVisibility(ESlateVisibility::Collapsed);ClothMID=nullptr;OriginalCloth=nullptr;
 UE_LOG(LogTemp,Display,TEXT("Stockings interaction OFF"));
}
void AStockingsInteractionActor::CacheGripPoints(){
 GripCandidates.Reset();if(ClothVertices.IsEmpty())return;
 for(int32 Side=0;Side<2;++Side){float Best=-MAX_flt;for(int32 I:HemVertices){if((RestVertices[I].X<0?0:1)!=Side)continue;FVector P=ClothVertices[I];GripCandidates.Add(ClothGeometry->GetComponentTransform().TransformPosition(P));float Score=P.Y-.1f*FMath::Abs(P.Z);if(Score>Best){Best=Score;GripPoints[Side]=ClothGeometry->GetComponentTransform().TransformPosition(P);}}}
}
FVector AStockingsInteractionActor::GetGripPoint(int32 Side)const{return GripCandidates.IsEmpty()?FVector::ZeroVector:GripPoints[FMath::Clamp(Side,0,1)];}
float AStockingsInteractionActor::GetCameraNearClipCm()const{return GNearClippingPlane;}
bool AStockingsInteractionActor::TryGrab(FVector O,FVector D){
 if(!InteractionActive||Dragging||!CanInteract()||!FitReady)return false;D.Normalize();float Best=MAX_flt;FVector Pick=FVector::ZeroVector;
 for(FVector C:GripCandidates){float T=FVector::DotProduct(C-O,D);float Dist=FVector::Distance(O+D*T,C);if(T>0&&Dist<2.5f&&Dist+.001f*T<Best){Best=Dist+.001f*T;Pick=C;}}
 if(Best==MAX_flt)return false;GrabCenter=Pick;PlanePoint=Pick;auto*PC=UI->GetOwningPlayer();PlaneNormal=PC->PlayerCameraManager->GetCameraRotation().Vector();
 if(!Plane(O,D,PlanePoint,PlaneNormal,StartCursor))return false;Dragging=true;BeginSimulationGrab(Pick);UE_LOG(LogTemp,Display,TEXT("Stockings grip HIT progress=%.2f"),RemovalProgress);return true;
}
void AStockingsInteractionActor::MoveGrab(FVector O,FVector D){
 if(!Dragging||!Stock)return;FVector P;if(!Plane(O,D.GetSafeNormal(),PlanePoint,PlaneNormal,P))return;
 const FTransform Tr=ClothGeometry->GetComponentTransform();FVector Local=Tr.InverseTransformVector(P-StartCursor);
 SimGrabTarget=SimGrabStart+Local.GetClampedToMaxSize(100);
}
void AStockingsInteractionActor::ReleaseGrab(){
 if(!Dragging)return;Dragging=false;EndSimulationGrab();UE_LOG(LogTemp,Display,TEXT("Stockings grip RELEASE progress=%.2f"),RemovalProgress);
}
void AStockingsInteractionActor::ReleaseFromMouse(){if(ManualSimulationGrip)return;if(Dragging&&UI&&!ManualSimulationGrip){FVector O,D;if(auto*PC=UI->GetOwningPlayer())if(PC->DeprojectMousePositionToWorld(O,D))MoveGrab(O,D);}ReleaseGrab();}
void AStockingsInteractionActor::SetRemovalProgress(float V){if(!InteractionActive)return;if(V>.001f){AcceptedTargetDrop=AcceptedMaxDrop*FMath::Clamp(V,0.f,1.f);return;}Dragging=false;EndSimulationGrab();BuildClothGeometry();}
void AStockingsInteractionActor::MouseDown(){if(!InteractionActive||!UI||!UI->GetOwningPlayer())return;auto*PC=UI->GetOwningPlayer();if(PC->IsInputKeyDown(EKeys::MiddleMouseButton)||PC->IsInputKeyDown(EKeys::RightMouseButton))return;FVector O,D;if(PC->DeprojectMousePositionToWorld(O,D))TryGrab(O,D);}
void AStockingsInteractionActor::GrabSide(int32 Side){
 if(!InteractionActive||!UI||!FitReady)return;auto*PC=UI->GetOwningPlayer();FVector O,D;if(!PC||!PC->DeprojectMousePositionToWorld(O,D))return;
 PlanePoint=GetGripPoint(Side);GrabCenter=PlanePoint;PlaneNormal=PC->PlayerCameraManager->GetCameraRotation().Vector();if(Plane(O,D,PlanePoint,PlaneNormal,StartCursor)){Dragging=true;BeginSimulationGrab(PlanePoint);}
}
void AStockingsInteractionActor::GrabLeft(){GrabSide(0);}void AStockingsInteractionActor::GrabRight(){GrabSide(1);}
void AStockingsInteractionActor::UpdateCloth(float Dt){
 StepAcceptedInteraction(Dt);if(!AcceptedGeometryChanged)return;ClothMID->SetScalarParameterValue(TEXT("Thickness"),BaseThickness);DeformCloth();CacheGripPoints();AcceptedGeometryChanged=false;
}
void AStockingsInteractionActor::Tick(float Dt){
 Super::Tick(Dt);if(!UI){FindDelay-=Dt;if(FindDelay<=0){FindDelay=.5f;FindUI();}return;}
 Body=Obj<USkeletalMeshComponent>(UI,TEXT("SpeechMouthMesh"));bool Can=CanInteract();
 if(InteractionActive&&(!Can||Number(UI,TEXT("StockingsState"))!=ActiveVariant||!Stock))StopInteraction();
 if(ModeButton){ModeButton->SetIsEnabled(Can||InteractionActive);ModeLabel->SetText(FText::FromString(InteractionActive?TEXT("互动模式：开启"):TEXT("互动模式")));}
 if(Hint)Hint->SetText(FText::FromString(InteractionActive?(FitReady?(TEXT("外拉松手收紧；下拉保持，最多到脚踝")):(SimulationSeconds>6?TEXT("贴合尚未稳定，请退出互动"):TEXT("正在贴合，请稍候"))):(!Can?TEXT("互动模式仅站姿可用"):TEXT(""))));
 if(!InteractionActive){UpdateOrdinaryPresentation();return;}
 if(PendingSimulationInit>0){if(--PendingSimulationInit==0){if(!BuildClothGeometry()){StopInteraction();return;}Stock->SetVisibility(false);ClothGeometry->SetVisibility(true);ClothKeyLight->SetVisibility(true);ClothFillLight->SetVisibility(true);CacheGripPoints();}return;}
 // Keep the user's wardrobe state; expose only the covered waist during this temporary mode.
 Body->ShowMaterialSection(SkirtMaterialSection,-1,false,0);if(auto*O=Obj<USkeletalMeshComponent>(UI,TEXT("WardrobeOutlineMesh")))O->ShowMaterialSection(SkirtMaterialSection,-1,false,0);
 Body->ShowMaterialSection(SocksMaterialSection,-1,false,0);Body->ShowMaterialSection(ShoesMaterialSection,-1,Flag(UI,TEXT("WardrobeShoesWorn")),0);
 if(auto*O=Obj<USkeletalMeshComponent>(UI,TEXT("WardrobeOutlineMesh"))){O->ShowMaterialSection(SocksMaterialSection,-1,false,0);O->ShowMaterialSection(ShoesMaterialSection,-1,Flag(UI,TEXT("WardrobeShoesWorn")),0);}
 Stock->SetVisibility(false);
 if(Dragging&&!ManualSimulationGrip){FVector O,D;auto*PC=UI->GetOwningPlayer();if(PC&&PC->DeprojectMousePositionToWorld(O,D))MoveGrab(O,D);}
 UpdateCloth(Dt);
 if(ClothMID)ClothMID->SetScalarParameterValue(TEXT("ShoesWorn"),Flag(UI,TEXT("WardrobeShoesWorn"))?1.f:0.f);
 if(InteractionSkinMID&&Body){
  // Correct skin radiance only beneath the moving cuff; exposed skin uses its original toon shader.
  float HemZ=-MAX_flt;const FTransform Tr=ClothGeometry->GetComponentTransform();
  for(int32 I:HemVertices)if(ClothVertices.IsValidIndex(I))HemZ=FMath::Max(HemZ,float(Tr.TransformPosition(ClothVertices[I]).Z));
  InteractionSkinMID->SetScalarParameterValue(TEXT("SkinCoverageTopZWS"),HemZ);
  InteractionSkinMID->SetScalarParameterValue(TEXT("ShoesWorn"),Flag(UI,TEXT("WardrobeShoesWorn"))?1.f:0.f);
  if(Body->GetMaterial(BodyMaterialSection)!=InteractionSkinMID)Body->SetMaterial(BodyMaterialSection,InteractionSkinMID);
 }
 for(int32 I=0;I<Grips.Num();++I){FVector2D Screen;bool Visible=UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(UI->GetOwningPlayer(),GetGripPoint(I),Screen,false);Grips[I]->SetIsEnabled(FitReady);Grips[I]->SetVisibility(Visible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);if(Visible)Cast<UCanvasPanelSlot>(Grips[I]->Slot)->SetPosition(Screen-FVector2D(11,11));}
}
void AStockingsInteractionActor::UpdateOrdinaryPresentation(){
 if(!UI||!Body)return;
 const int32 Variant=Number(UI,TEXT("StockingsState"));
 UMaterialInterface* Material=Variant==1?OrdinaryWhiteMaterial.Get():Variant==2?OrdinaryBlackMaterial.Get():nullptr;
 if(!Flag(UI,TEXT("ChatUIActive"))||!Material||OrdinaryVariant!=Variant){
  if(OrdinarySkinMID&&Body->GetMaterial(BodyMaterialSection)==OrdinarySkinMID&&OrdinaryBodyBeforeSheer)Body->SetMaterial(BodyMaterialSection,OrdinaryBodyBeforeSheer);
  OrdinarySkinMID=nullptr;OrdinarySheerMID=nullptr;OrdinaryBodyBeforeSheer=nullptr;OrdinaryVariant=0;
  if(!Flag(UI,TEXT("ChatUIActive"))||!Material)return;
 }
 UMaterialInterface* SkinMaterial=Variant==1&&WhiteSkinMaterial?WhiteSkinMaterial.Get():InteractionSkinMaterial.Get();
 auto*Mesh=Obj<USkeletalMeshComponent>(UI,TEXT("StockingsMesh"));if(!Mesh||!SkinMaterial)return;
 if(!OrdinarySheerMID){OrdinarySheerMID=UMaterialInstanceDynamic::Create(Material,this);OrdinarySheerMID->SetScalarParameterValue(TEXT("GrabActive"),0.f);OrdinaryVariant=Variant;}
 if(!OrdinarySkinMID){OrdinaryBodyBeforeSheer=Body->GetMaterial(BodyMaterialSection);OrdinarySkinMID=UMaterialInstanceDynamic::Create(SkinMaterial,this);OrdinarySkinMID->SetScalarParameterValue(TEXT("StaticCoverage"),1.f);}
 const float Shoes=Flag(UI,TEXT("WardrobeShoesWorn"))?1.f:0.f;
 OrdinarySheerMID->SetScalarParameterValue(TEXT("ShoesWorn"),Shoes);OrdinarySkinMID->SetScalarParameterValue(TEXT("ShoesWorn"),Shoes);
 if(Mesh->GetMaterial(0)!=OrdinarySheerMID)Mesh->SetMaterial(0,OrdinarySheerMID);
 if(Body->GetMaterial(BodyMaterialSection)!=OrdinarySkinMID)Body->SetMaterial(BodyMaterialSection,OrdinarySkinMID);
}
void AStockingsInteractionActor::EndPlay(const EEndPlayReason::Type Reason){StopInteraction();if(ModeButton)ModeButton->RemoveFromParent();if(Hint)Hint->RemoveFromParent();for(auto B:Grips)if(B)B->RemoveFromParent();Super::EndPlay(Reason);}
