#include "StockingsInteractionActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"

void AStockingsInteractionActor::BuildSwimBottomContact()
{
 SwimBottomPositions.Reset();SwimBottomTriangles.Reset();SwimBottomFaceBounds.Reset();SwimBottomFaceOutward.Reset();SwimBottomBounds=FBox(ForceInit);SwimBottomInitialCorrections=0;
 if(!Body || !Body->GetSkeletalMeshAsset())return;
 const auto* Data=Body->GetSkeletalMeshAsset()->GetResourceForRendering();if(!Data || Data->LODRenderData.IsEmpty())return;
 const auto& L=Data->LODRenderData[0];auto* Weights=Body->GetSkinWeightBuffer(0);if(!Weights)return;
 const auto* IB=L.MultiSizeIndexContainer.GetIndexBuffer();TArray<FMatrix44f> Matrices;Body->GetCurrentRefToLocalMatrices(Matrices,0);
 const FTransform ToLocal=Body->GetComponentTransform().GetRelativeTransform(ClothGeometry->GetComponentTransform());
 TMap<int32,int32> Compact;
 for(const auto& Section:L.RenderSections)
 {
  if(Section.MaterialIndex!=InnerGarmentMaterialSection)continue; // Inner costume, including lower straps and fasteners.
  for(uint32 F=Section.BaseIndex;F<Section.BaseIndex+Section.NumTriangles*3;F+=3)
  {
   int32 Original[3]={int32(IB->Get(F)),int32(IB->Get(F+1)),int32(IB->Get(F+2))};
   FVector Rest[3];for(int32 K=0;K<3;++K)Rest[K]=FVector(L.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Original[K]));
   const FVector M=(Rest[0]+Rest[1]+Rest[2])/3;
   // Reference-pose bounds exclude swimsuit cups, tail/wing accessories and all outer clothing.
   // The lower swimsuit's nine connected parts fit within these reference bounds.
   // The lateral straps extend to X=10.44; the old X=8 cutoff omitted them.
   if(M.X<InnerGarmentBoundsMin.X || M.X>InnerGarmentBoundsMax.X || M.Y<InnerGarmentBoundsMin.Y || M.Y>InnerGarmentBoundsMax.Y || M.Z<InnerGarmentBoundsMin.Z || M.Z>InnerGarmentBoundsMax.Z)continue;
   int32 V[3];for(int32 K=0;K<3;++K)
   {
    if(const int32* Found=Compact.Find(Original[K]))V[K]=*Found;
    else
    {
     V[K]=SwimBottomPositions.Num();Compact.Add(Original[K],V[K]);
     SwimBottomPositions.Add(ToLocal.TransformPosition(FVector(USkinnedMeshComponent::GetSkinnedVertexPosition(Body,Original[K],L,*Weights,Matrices))));
    }
   }
   const auto RN=L.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Original[0]);
   if(FVector::DotProduct(FVector::CrossProduct(Rest[1]-Rest[0],Rest[2]-Rest[0]),FVector(RN.X,RN.Y,RN.Z))<0)Swap(V[1],V[2]);
   FBox Bounds(ForceInit);for(int32 K:V){SwimBottomTriangles.Add(K);Bounds+=SwimBottomPositions[K];SwimBottomBounds+=SwimBottomPositions[K];}
   SwimBottomFaceBounds.Add(Bounds);
  }
 }
 // Retain the independent rings and straps too. Contact directions follow the
 // skin underneath: closed ring undersides must never push cloth into the body.
 for(int32 F=0;F<SwimBottomTriangles.Num();F+=3)
 {
  const FVector A=SwimBottomPositions[SwimBottomTriangles[F]],B=SwimBottomPositions[SwimBottomTriangles[F+1]],C=SwimBottomPositions[SwimBottomTriangles[F+2]];
  FVector Q,N;float D;
  if(!NearestBody((A+B+C)/3,Q,N,D))N=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
  SwimBottomFaceOutward.Add(N);
 }
 UE_LOG(LogTemp,Display,TEXT("Swimsuit bottom contact: %d triangles, %d posed vertices"),SwimBottomTriangles.Num()/3,SwimBottomPositions.Num());
}

bool AStockingsInteractionActor::ProjectSwimBottom(FVector& P) const
{
 constexpr double Search=1.0,Gap=.14;
 if(SwimBottomTriangles.IsEmpty() || !SwimBottomBounds.ExpandBy(Search).IsInsideOrOn(P))return false;
 bool Changed=false;
 for(int32 Pass=0;Pass<4;++Pass)
 {
  double Best=Search*Search;FVector Point=FVector::ZeroVector,Normal=FVector::ZeroVector;
  for(int32 F=0;F<SwimBottomFaceBounds.Num();++F)
  {
   if(SwimBottomFaceBounds[F].ComputeSquaredDistanceToPoint(P)>Best)continue;
   const FVector A=SwimBottomPositions[SwimBottomTriangles[F*3]],B=SwimBottomPositions[SwimBottomTriangles[F*3+1]],C=SwimBottomPositions[SwimBottomTriangles[F*3+2]];
   const FVector Q=FMath::ClosestPointOnTriangleToPoint(P,A,B,C);const double D=FVector::DistSquared(P,Q);
   if(D<Best){Best=D;Point=Q;Normal=SwimBottomFaceOutward[F];}
  }
  if(Normal.IsNearlyZero())break;
  const double Signed=FVector::DotProduct(P-Point,Normal);
  if(Signed>=Gap-.0001)break;
  // A local surface projection preserves the swimsuit and keeps the outer stocking outside it.
  P+=Normal*FMath::Min(Gap-Signed,.6);Changed=true;
 }
 return Changed;
}
