#include "StockingsInteractionActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "HAL/PlatformTime.h"
#include "Async/ParallelFor.h"
static uint64 EdgeKey(int32 A,int32 B){return (uint64(FMath::Min(A,B))<<32)|uint32(FMath::Max(A,B));}
static FVector Barycentric(FVector P,FVector A,FVector B,FVector C){
 FVector V0=B-A,V1=C-A,V2=P-A;double D00=V0|V0,D01=V0|V1,D11=V1|V1,D20=V2|V0,D21=V2|V1,Den=D00*D11-D01*D01;
 if(FMath::Abs(Den)<1.e-12)return FVector(1,0,0);double V=(D11*D20-D01*D21)/Den,W=(D00*D21-D01*D20)/Den;return FVector(1-V-W,V,W);
}
void AStockingsInteractionActor::InitializeSimulation(){
 SimPositions.Reset();SimRest.Reset();RenderParticle.Init(-1,RestVertices.Num());TMap<FIntVector,int32> Weld;
 TArray<int32> OriginalWeld,Representatives;OriginalWeld.SetNum(SourceVertexCount);
 for(int32 I=0;I<SourceVertexCount;++I){FVector R=RestVertices[I]*500;FIntVector K(FMath::RoundToInt(R.X),FMath::RoundToInt(R.Y),FMath::RoundToInt(R.Z));int32*Found=Weld.Find(K);
  if(!Found){int32 N=Representatives.Num();Weld.Add(K,N);Representatives.Add(I);Found=Weld.Find(K);}OriginalWeld[I]=*Found;
 }
 TArray<int32> Proxy;Proxy.Init(-1,Representatives.Num());
 for(int32 Seed=0;Seed<Representatives.Num();++Seed){Proxy[Seed]=SimPositions.Num();SimPositions.Add(BaseVertices[Representatives[Seed]]);SimRest.Add(RestVertices[Representatives[Seed]]);}
 for(int32 I=0;I<SourceVertexCount;++I)RenderParticle[I]=Proxy[OriginalWeld[I]];
 TSet<FIntVector>UniqueFaces;
 SimTriangles.Reset();SimNeighbors.SetNum(SimPositions.Num());for(auto&N:SimNeighbors)N.Reset();TSet<uint64> Edges;StretchConstraints.Reset();
 // Four render triangles are generated per original triangle by BuildClothGeometry.
 for(int32 I=0;I<ClothTriangles.Num();I+=12){int32 A=RenderParticle[ClothTriangles[I]],B=RenderParticle[ClothTriangles[I+4]],C=RenderParticle[ClothTriangles[I+8]];if(A==B||B==C||A==C)continue;int32 K0=A,K1=B,K2=C;if(K0>K1)Swap(K0,K1);if(K1>K2)Swap(K1,K2);if(K0>K1)Swap(K0,K1);FIntVector Key(K0,K1,K2);if(UniqueFaces.Contains(Key))continue;UniqueFaces.Add(Key);
  SimTriangles.Add(FIntVector(A,B,C));int32 V[3]={A,B,C};
  for(int32 E=0;E<3;++E){int32 X=V[E],Y=V[(E+1)%3];uint64 K=EdgeKey(X,Y);
   if(!Edges.Contains(K)){Edges.Add(K);SimNeighbors[X].AddUnique(Y);SimNeighbors[Y].AddUnique(X);FClothDistanceConstraint Q;Q.A=X;Q.B=Y;Q.FittedLength=FVector::Distance(SimPositions[X],SimPositions[Y]);StretchConstraints.Add(Q);
   }
  }
 }
 SimPrevious=SimPositions;GrabWeights.Init(0,SimPositions.Num());GrabParticle=-1;ManualSimulationGrip=false;
 // Exact posed body triangles, compacted to vertices used by the body material section.
 BodySource.Reset();BodyTriangles.Reset();BodyTree.Reset();BodyOrder.Reset();BodyQuerySeeds.Init(-1,SimPositions.Num()+SimTriangles.Num()+SourceVertexCount);
 if(Body&&Body->GetSkeletalMeshAsset()&&Body->GetSkeletalMeshAsset()->GetResourceForRendering()){
  const auto&L=Body->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];const auto*IB=L.MultiSizeIndexContainer.GetIndexBuffer();TMap<int32,int32> Compact;
  for(const auto&S:L.RenderSections)if(S.MaterialIndex==BodyMaterialSection){for(uint32 I=S.BaseIndex;I<S.BaseIndex+S.NumTriangles*3;I+=3){int32 V[3];for(int32 K=0;K<3;++K){int32 Original=IB->Get(I+K);int32*Found=Compact.Find(Original);if(!Found){int32 N=BodySource.Num();BodySource.Add(Original);Compact.Add(Original,N);Found=Compact.Find(Original);}V[K]=*Found;}
    FVector A(L.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(BodySource[V[0]])),B(L.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(BodySource[V[1]])),C(L.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(BodySource[V[2]]));auto N=L.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(BodySource[V[0]]);
    if(FVector::DotProduct(FVector::CrossProduct(B-A,C-A),FVector(N.X,N.Y,N.Z))<0)Swap(V[1],V[2]);for(int32 K:V)BodyTriangles.Add(K);
  }}
  BodyPositions.SetNum(BodySource.Num());BodyNormals.SetNum(BodySource.Num());RefreshBodyCollision();for(int32 I=0;I<BodyTriangles.Num()/3;++I)BodyOrder.Add(I);if(!BodyOrder.IsEmpty())BuildBodyNode(0,BodyOrder.Num());
 }
 SlideCm=0;MaxStretch=1;MinBodyClearance=.14f;SimTimeMs=0;BodyContactCount=SelfContactCount=0;
 InitializeAcceptedInteraction();BindAcceptedRender();
}
void AStockingsInteractionActor::RefreshBodyCollision(){
 if(!Body||BodySource.IsEmpty())return;const auto&L=Body->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];auto*W=Body->GetSkinWeightBuffer(0);if(!W)return;TArray<FMatrix44f>M;Body->GetCurrentRefToLocalMatrices(M,0);FTransform ToLocal=Body->GetComponentTransform().GetRelativeTransform(ClothGeometry->GetComponentTransform());
 for(int32 I=0;I<BodySource.Num();++I)BodyPositions[I]=ToLocal.TransformPosition(FVector(USkinnedMeshComponent::GetSkinnedVertexPosition(Body,BodySource[I],L,*W,M)));
 BodyNormals.Init(FVector::ZeroVector,BodySource.Num());for(int32 I=0;I<BodyTriangles.Num();I+=3){int32 A=BodyTriangles[I],B=BodyTriangles[I+1],C=BodyTriangles[I+2];FVector N=FVector::CrossProduct(BodyPositions[B]-BodyPositions[A],BodyPositions[C]-BodyPositions[A]);for(int32 V:{A,B,C})BodyNormals[V]+=N;}
 for(auto&N:BodyNormals)N=N.GetSafeNormal();if(!BodyTree.IsEmpty())RefitBodyNode(0);
}
int32 AStockingsInteractionActor::BuildBodyNode(int32 Start,int32 Count){
 int32 Index=BodyTree.AddDefaulted();BodyTree[Index].Start=Start;BodyTree[Index].Count=Count;FBox Box(ForceInit);for(int32 I=Start;I<Start+Count;++I)for(int32 K=0;K<3;++K)Box+=BodyPositions[BodyTriangles[BodyOrder[I]*3+K]];BodyTree[Index].Bounds=Box;
 if(Count>12){FVector Size=Box.GetSize();int32 Axis=Size.X>Size.Y?(Size.X>Size.Z?0:2):(Size.Y>Size.Z?1:2);TArray<int32> Part;for(int32 I=Start;I<Start+Count;++I)Part.Add(BodyOrder[I]);Part.Sort([&](int32 A,int32 B){double CA=0,CB=0;for(int32 K=0;K<3;++K){CA+=BodyPositions[BodyTriangles[A*3+K]][Axis];CB+=BodyPositions[BodyTriangles[B*3+K]][Axis];}return CA<CB;});for(int32 I=0;I<Count;++I)BodyOrder[Start+I]=Part[I];int32 Half=Count/2,L=BuildBodyNode(Start,Half),R=BuildBodyNode(Start+Half,Count-Half);BodyTree[Index].Left=L;BodyTree[Index].Right=R;}
 return Index;
}
void AStockingsInteractionActor::RefitBodyNode(int32 I){
 auto&N=BodyTree[I];if(N.Left>=0){RefitBodyNode(N.Left);RefitBodyNode(N.Right);N.Bounds=BodyTree[N.Left].Bounds+BodyTree[N.Right].Bounds;}else{N.Bounds=FBox(ForceInit);for(int32 J=N.Start;J<N.Start+N.Count;++J)for(int32 K=0;K<3;++K)N.Bounds+=BodyPositions[BodyTriangles[BodyOrder[J]*3+K]];}
}
bool AStockingsInteractionActor::NearestBody(FVector P,FVector&Q,FVector&N,float&Distance,int32 Query)const{
 if(BodyTree.IsEmpty())return false;double Best=MAX_dbl;int32 Face=-1;
 if(BodyQuerySeeds.IsValidIndex(Query)&&BodyQuerySeeds[Query]>=0){Face=BodyQuerySeeds[Query];Q=FMath::ClosestPointOnTriangleToPoint(P,BodyPositions[BodyTriangles[Face]],BodyPositions[BodyTriangles[Face+1]],BodyPositions[BodyTriangles[Face+2]]);Best=FVector::DistSquared(P,Q);}
 TArray<int32,TInlineAllocator<64>> Stack;Stack.Add(0);
 while(!Stack.IsEmpty()){int32 I=Stack.Pop(EAllowShrinking::No);const auto&B=BodyTree[I];if(B.Bounds.ComputeSquaredDistanceToPoint(P)>Best)continue;
  if(B.Left>=0){double L=BodyTree[B.Left].Bounds.ComputeSquaredDistanceToPoint(P),R=BodyTree[B.Right].Bounds.ComputeSquaredDistanceToPoint(P);if(L<R){Stack.Add(B.Right);Stack.Add(B.Left);}else{Stack.Add(B.Left);Stack.Add(B.Right);}}
  else for(int32 J=B.Start;J<B.Start+B.Count;++J){int32 F=BodyOrder[J]*3;FVector C=FMath::ClosestPointOnTriangleToPoint(P,BodyPositions[BodyTriangles[F]],BodyPositions[BodyTriangles[F+1]],BodyPositions[BodyTriangles[F+2]]);double D=FVector::DistSquared(P,C);if(D<Best){Best=D;Face=F;Q=C;}}
 }
 if(Face<0)return false;if(BodyQuerySeeds.IsValidIndex(Query))BodyQuerySeeds[Query]=Face;int32 A=BodyTriangles[Face],B=BodyTriangles[Face+1],C=BodyTriangles[Face+2];FVector W=Barycentric(Q,BodyPositions[A],BodyPositions[B],BodyPositions[C]);FVector Pseudo=(BodyNormals[A]*W.X+BodyNormals[B]*W.Y+BodyNormals[C]*W.Z).GetSafeNormal();FVector Delta=P-Q;float Len=Delta.Size(),Sign=(Delta|Pseudo)>=0?1.f:-1.f;
 // Closest-feature pseudonormal supplies a local sign, not a certified global inside/outside test.
 N=Len>1.e-5f?Delta*(Sign/Len):Pseudo;Distance=Sign*Len;return true;
}
void AStockingsInteractionActor::BeginSimulationGrab(FVector WorldPoint){
 FVector P=ClothGeometry->GetComponentTransform().InverseTransformPosition(WorldPoint);GrabParticle=-1;double Best=MAX_dbl;for(int32 I=0;I<SourceVertexCount;++I)if(RestVertices[I].Z>TopZ-1.5f){int32 J=RenderParticle[I];double D=FVector::DistSquared(SimPositions[J],P);if(D<Best){Best=D;GrabParticle=J;}}
 if(GrabParticle<0)return;SimGrabStart=SimPositions[GrabParticle];SimGrabTarget=SimGrabStart;AcceptedGrabDrop=AcceptedDrop;AcceptedDownGrab=false;AcceptedGeometryChanged=true;ManualSimulationGrip=false;GrabWeights.Init(0,SimPositions.Num());
 // Geodesic patch weights, across welded UV seams, not a sphere that grabs the opposite leg.
 TArray<float> Dist;Dist.Init(MAX_flt,SimPositions.Num());Dist[GrabParticle]=0;TArray<int32> Queue;Queue.Add(GrabParticle);
 for(int32 K=0;K<Queue.Num();++K){int32 I=Queue[K];for(int32 J:SimNeighbors[I]){float D=Dist[I]+FVector::Distance(SimRest[I],SimRest[J]);if(D<10.f&&D<Dist[J]){Dist[J]=D;Queue.Add(J);}}}
 for(int32 I=0;I<Dist.Num();++I)GrabWeights[I]=FMath::Exp(-FMath::Square(Dist[I]/5.f));AcceptedPatchWeights=GrabWeights;
}
void AStockingsInteractionActor::EndSimulationGrab(){AcceptedTargetDrop=AcceptedDrop;AcceptedDownGrab=false;GrabParticle=-1;ManualSimulationGrip=false;GrabWeights.Init(0,SimPositions.Num());}
void AStockingsInteractionActor::SetSimulationGripTarget(FVector P){if(!InteractionActive||!Dragging||GrabParticle<0)return;ManualSimulationGrip=true;SimGrabTarget=P;}
FString AStockingsInteractionActor::GetSimulationStats()const{return GetAcceptedStats();}
