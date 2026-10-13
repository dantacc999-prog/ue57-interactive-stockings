#include "StockingsInteractionActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"

namespace {
double Cross2(FVector2D A,FVector2D B){return A.X*B.Y-A.Y*B.X;}
float Smooth01(float T){T=FMath::Clamp(T,0.f,1.f);return T*T*(3-2*T);}
TArray<FVector2D> Hull(TArray<FVector2D> P){
 P.Sort([](const FVector2D&A,const FVector2D&B){return A.X==B.X?A.Y<B.Y:A.X<B.X;});
 TArray<FVector2D> H;for(auto V:P){while(H.Num()>1&&Cross2(H.Last()-H[H.Num()-2],V-H.Last())<=0)H.Pop(EAllowShrinking::No);H.Add(V);}
 int32 Lower=H.Num();for(int32 I=P.Num()-2;I>=0;--I){auto V=P[I];while(H.Num()>Lower&&Cross2(H.Last()-H[H.Num()-2],V-H.Last())<=0)H.Pop(EAllowShrinking::No);H.Add(V);}if(H.Num()>1)H.Pop(EAllowShrinking::No);return H;
}
}

void AStockingsInteractionActor::BuildAcceptedVisualAtlas(){
 const auto&Ref=Stock->GetSkeletalMeshAsset()->GetRefSkeleton();TArray<FTransform> CS;CS.SetNum(Ref.GetNum());
 for(int32 I=0;I<Ref.GetNum();++I){int32 P=Ref.GetParentIndex(I);CS[I]=P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*CS[P];}
 auto Reference=[&](FName N){int32 I=Ref.FindBoneIndex(N);return I>=0?CS[I].GetLocation():FVector::ZeroVector;};
 const auto Transform=ClothGeometry->GetComponentTransform();auto Pose=[&](FName N){return Transform.InverseTransformPosition(Body->GetSocketLocation(N));};
 FVector R[2][3],P[2][3];for(int32 S=0;S<2;++S){const TCHAR*Suffix=S==0?TEXT("R"):TEXT("L");int32 K=0;for(const TCHAR*Part:{TEXT("Foot_"),TEXT("LowerLeg_"),TEXT("UpperLeg_")}){FName N(*(FString(Part)+Suffix));R[S][K]=Reference(N);P[S][K++]=Pose(N);}}
 const FVector RH=Reference(TEXT("Hips")),PH=Pose(TEXT("Hips"));AcceptedHipRef=(R[0][2].Z+R[1][2].Z)*.5;
 auto Center=[&](int32 S,float H){
  if(H<=R[S][1].Z)return FMath::Lerp(P[S][0],P[S][1],double((H-R[S][0].Z)/FMath::Max(R[S][1].Z-R[S][0].Z,1.)));
  if(H<=R[S][2].Z)return FMath::Lerp(P[S][1],P[S][2],double((H-R[S][1].Z)/FMath::Max(R[S][2].Z-R[S][1].Z,1.)));
  FVector Upper=P[S][2]+FVector(0,0,H-R[S][2].Z),Torso=PH+FVector(0,0,H-RH.Z);
  return FMath::Lerp(Upper,Torso,double(Smooth01((H-R[S][2].Z)/7.f)));
 };
 // Use the actual lower-body skin contour, excluding hands and upper body in
 // the reference pose. Equal-height slices avoid vertex-density-biased rings.
 TArray<FIntVector> Faces;const auto&L=Body->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
 for(int32 F=0;F<BodyTriangles.Num();F+=3){FVector A(L.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(BodySource[BodyTriangles[F]])),B(L.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(BodySource[BodyTriangles[F+1]])),C(L.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(BodySource[BodyTriangles[F+2]]));FVector M=(A+B+C)/3;if(M.Z<TopZ+3&&FMath::Abs(M.X)<16)Faces.Add(FIntVector(BodyTriangles[F],BodyTriangles[F+1],BodyTriangles[F+2]));}
 constexpr int32 Count=128;for(auto&V:AcceptedVisualSlices)V.SetNum(Count);
 for(int32 I=0;I<Count;++I){float H=FMath::Lerp(AcceptedAnkleRef,TopZ,float(I)/(Count-1));FVector Centers[3]={Center(0,H),Center(1,H),FVector::ZeroVector};Centers[2]=(Centers[0]+Centers[1])*.5;
  for(int32 S=0;S<3;++S){auto&Slice=AcceptedVisualSlices[S][I];Slice.Center=Centers[S];float PlaneZ=Centers[S].Z;TArray<TPair<FVector2D,FVector2D>>Segments;TArray<FVector2D>Points;
   for(const auto&T:Faces){FVector V[3]={BodyPositions[T.X],BodyPositions[T.Y],BodyPositions[T.Z]};TArray<FVector2D,TInlineAllocator<3>>Cross;
    for(int32 K=0;K<3;++K){auto A=V[K],B=V[(K+1)%3];if((A.Z<=PlaneZ&&B.Z>PlaneZ)||(B.Z<=PlaneZ&&A.Z>PlaneZ)){FVector Q=FMath::Lerp(A,B,(PlaneZ-A.Z)/(B.Z-A.Z));Cross.Add(FVector2D(Q.X,Q.Y));}}
    if(Cross.Num()!=2)continue;FVector2D Mid=(Cross[0]+Cross[1])*.5;float Bound=FMath::Lerp(7.f,15.f,Smooth01((H-(AcceptedHipRef-12))/15.f));double Dist=FMath::Min(FVector2D::DistSquared(Mid,FVector2D(Centers[0].X,Centers[0].Y)),FVector2D::DistSquared(Mid,FVector2D(Centers[1].X,Centers[1].Y)));if(Dist>Bound*Bound)continue;
    Segments.Emplace(Cross[0],Cross[1]);Points.Append(Cross);
   }
   if(S==2){auto Poly=Hull(Points);Segments.Reset();for(int32 K=0;K<Poly.Num();++K)Segments.Emplace(Poly[K],Poly[(K+1)%Poly.Num()]);}
   FVector2D O(Slice.Center.X,Slice.Center.Y);
   for(int32 B=0;B<64;++B){float Angle=2*PI*B/64;FVector2D D(FMath::Cos(Angle),FMath::Sin(Angle));double Best=MAX_dbl;
    for(auto Edge:Segments){FVector2D A=Edge.Key-O,E=Edge.Value-Edge.Key;double Den=Cross2(D,E);if(FMath::Abs(Den)<1.e-9)continue;double T=Cross2(A,E)/Den,U=Cross2(A,D)/Den;if(T>.05&&U>=0&&U<=1)Best=FMath::Min(Best,T);}
    Slice.Radius[B]=Best<MAX_dbl?FMath::Clamp(float(Best),.5f,22.f):(I?AcceptedVisualSlices[S][I-1].Radius[B]:3.f);
   }
   // Small angular smoothing removes tessellation-scale bumps, retaining the
   // anatomical silhouette and a smooth radius function across the UV seam.
   float Temp[64];for(int32 K=0;K<2;++K){for(int32 B=0;B<64;++B)Temp[B]=.2f*Slice.Radius[(B+63)%64]+.6f*Slice.Radius[B]+.2f*Slice.Radius[(B+1)%64];for(int32 B=0;B<64;++B)Slice.Radius[B]=Temp[B];}
  }
 }
 AcceptedVisualNeighbors.SetNum(NormalGroupCount);for(auto&N:AcceptedVisualNeighbors)N.Reset();
 for(int32 F=0;F<ClothTriangles.Num();F+=3)for(int32 K=0;K<3;++K){int32 A=NormalGroups[ClothTriangles[F+K]],B=NormalGroups[ClothTriangles[F+(K+1)%3]];if(A!=B){AcceptedVisualNeighbors[A].AddUnique(B);AcceptedVisualNeighbors[B].AddUnique(A);}}
 AcceptedParticleRender.Init(-1,SimPositions.Num());for(int32 I=0;I<SourceVertexCount;++I)AcceptedParticleRender[RenderParticle[I]]=NormalGroups[I];
 AcceptedBaseline.SetNum(NormalGroupCount);for(int32 G=0;G<NormalGroupCount;++G){int32 I=AcceptedRenderRepresentatives[G];FVector V=BaseVertices[I],Q,N;float D;
  if(RestVertices[I].Z<=AcceptedAnkleRef)V+=AcceptedBaseRenderNormals[G]*.10;
  else for(int32 K=0;K<8;++K){if(!NearestBody(V,Q,N,D)||D>=.1599f)break;V+=N*(.16f-D);}
  if(ProjectSwimBottom(V))++SwimBottomInitialCorrections;
  AcceptedBaseline[G]=V;
 }
}

FStockingsVisualSlice AStockingsInteractionActor::SampleAcceptedVisualSlice(int32 S,float H)const{
 const auto&V=AcceptedVisualSlices[S];float T=FMath::Clamp((H-AcceptedAnkleRef)/(TopZ-AcceptedAnkleRef)*(V.Num()-1),0.f,float(V.Num()-1));int32 A=FMath::FloorToInt(T),B=FMath::Min(A+1,V.Num()-1);float W=T-A;FStockingsVisualSlice Out;Out.Center=FMath::Lerp(V[A].Center,V[B].Center,double(W));for(int32 K=0;K<64;++K)Out.Radius[K]=FMath::Lerp(V[A].Radius[K],V[B].Radius[K],W);return Out;
}

FVector AStockingsInteractionActor::MapAcceptedVisualVertex(int32 G,float H,float Derivative,float Compression)const{
 int32 I=AcceptedRenderRepresentatives[G];float Z=RestVertices[I].Z;FVector Initial=AcceptedBaseline[G];
 if(Z<=AcceptedAnkleRef||FMath::Abs(H-Z)<1.e-6)return Initial;
 auto Map=[&](int32 S){auto Old=SampleAcceptedVisualSlice(S,Z),Now=SampleAcceptedVisualSlice(S,H);FVector Offset=Initial-Old.Center;double A=FMath::Atan2(Offset.Y,Offset.X);if(A<0)A+=2*PI;float Bin=A/(2*PI)*64;int32 K=FMath::FloorToInt(Bin);float W=Bin-K;
  auto Radius=[&](const FStockingsVisualSlice&V){return FMath::Lerp(V.Radius[K%64],V.Radius[(K+1)%64],W);};
  float Fold=FMath::Min(AnkleGatherAmplitudeCm,.15f)*Compression*(.6f+.4f*FMath::Sin((H-AcceptedAnkleRef)*2*PI/2.4f+.28f*FMath::Cos(A*3)));
  float Cuff=FMath::Clamp(1-(TopZ-Z)/2.5f,0.f,1.f),Roll=.16f*RemovalProgress*4*Cuff*(1-Cuff);
  float Scale=(Radius(Now)+Fold+Roll+.10f)/FMath::Max(Radius(Old)+.10f,.5f);
  return Now.Center+FVector(Offset.X*Scale,Offset.Y*Scale,Offset.Z*Derivative);
 };
 float Side=Smooth01((float(RestVertices[I].X)+2.f)/4.f);FVector Leg=Side<=0?Map(0):(Side>=1?Map(1):FMath::Lerp(Map(0),Map(1),double(Side)));
 float Torso=Smooth01((Z-(AcceptedHipRef-8))/12.f);return Torso<=0?Leg:(Torso>=1?Map(2):FMath::Lerp(Leg,Map(2),double(Torso)));
}
