#include "StockingsInteractionActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
bool AStockingsInteractionActor::BuildClothGeometry(){
 auto*M=Stock->GetSkeletalMeshAsset();if(!M||!M->GetResourceForRendering())return false;
 const auto&LOD=M->GetResourceForRendering()->LODRenderData[0];auto*Weights=Stock->GetSkinWeightBuffer(0);if(!Weights)return false;
 SourceVertexCount=LOD.GetNumVertices();RestVertices.Reset();BaseVertices.Reset();ClothUV.Reset();SourceA.Reset();SourceB.Reset();ClothTriangles.Reset();HemVertices.Reset();
 TArray<FMatrix44f> Matrices;Stock->GetCurrentRefToLocalMatrices(Matrices,0);TopZ=-MAX_flt;BottomZ=MAX_flt;
 for(int32 I=0;I<SourceVertexCount;++I){FVector R(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(I));RestVertices.Add(R);BaseVertices.Add(FVector(USkinnedMeshComponent::GetSkinnedVertexPosition(Stock,I,LOD,*Weights,Matrices)));ClothUV.Add(FVector2D(LOD.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(I,0)));SourceA.Add(I);SourceB.Add(I);TopZ=FMath::Max(TopZ,float(R.Z));BottomZ=FMath::Min(BottomZ,float(R.Z));}
 const auto*IB=LOD.MultiSizeIndexContainer.GetIndexBuffer();TArray<int32> Original;for(int32 I=0;I<int32(IB->Num());++I)Original.Add(IB->Get(I));
 // One subdivision supplies enough axial samples for actual gathered folds. UVs and seam pairs stay intact.
 TMap<uint64,int32> Edges;auto Mid=[&](int32 A,int32 B){uint64 Key=(uint64(FMath::Min(A,B))<<32)|uint32(FMath::Max(A,B));if(auto*Found=Edges.Find(Key))return *Found;int32 I=RestVertices.Num();Edges.Add(Key,I);RestVertices.Add((RestVertices[A]+RestVertices[B])*.5);BaseVertices.Add((BaseVertices[A]+BaseVertices[B])*.5);ClothUV.Add((ClothUV[A]+ClothUV[B])*.5);SourceA.Add(A);SourceB.Add(B);return I;};
 for(int32 I=0;I<Original.Num();I+=3){int32 A=Original[I],B=Original[I+1],C=Original[I+2],AB=Mid(A,B),BC=Mid(B,C),CA=Mid(C,A);for(int32 V:{A,AB,CA,AB,B,BC,CA,BC,C,AB,BC,CA})ClothTriangles.Add(V);}
 NormalSign=1;for(int32 I=0;I<Original.Num();I+=3){int32 A=Original[I],B=Original[I+1],C=Original[I+2];FVector N=FVector::CrossProduct(RestVertices[B]-RestVertices[A],RestVertices[C]-RestVertices[A]);if(N.SizeSquared()>.00001){auto RN=LOD.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(A);NormalSign=FVector::DotProduct(N,FVector(RN.X,RN.Y,RN.Z))<0?-1:1;break;}}
 NormalGroups.Reset();NormalGroupCount=0;TMap<FIntVector,int32> SeamGroups;
 for(int32 I=0;I<RestVertices.Num();++I){if(RestVertices[I].Z>TopZ-1.5f)HemVertices.Add(I);FVector P=RestVertices[I]*500;FIntVector Key(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.Z));int32*Group=SeamGroups.Find(Key);if(!Group){SeamGroups.Add(Key,NormalGroupCount++);Group=SeamGroups.Find(Key);}NormalGroups.Add(*Group);}
 ClothVertices=BaseVertices;ClothNormals.SetNum(ClothVertices.Num());ClothTangents.SetNum(ClothVertices.Num());ClothGeometry->SetWorldTransform(Stock->GetComponentTransform());ClothGeometry->SetMaterial(0,ClothMID);InitializeSimulation();DeformCloth();
 // Vertex colors carry density/contact data, so keep them linear for both
 // creation and updates (the two API overloads have different sRGB defaults).
 ClothGeometry->ClearAllMeshSections();ClothGeometry->CreateMeshSection_LinearColor(0,ClothVertices,ClothTriangles,ClothNormals,ClothUV,ClothColors,ClothTangents,false,false);ClothGeometry->SetCastShadow(false);
 UE_LOG(LogTemp,Display,TEXT("Whole stockings geometry: %d verts, %d triangles, hem %.2f, floor %.2f"),ClothVertices.Num(),ClothTriangles.Num()/3,TopZ,BottomZ);return !ClothVertices.IsEmpty();
}
void AStockingsInteractionActor::DeformCloth(){
 if(BaseVertices.IsEmpty())return;
 TArray<FVector> GroupNormals,TangentX,TangentY;GroupNormals.Init(FVector::ZeroVector,NormalGroupCount);TangentX.Init(FVector::ZeroVector,ClothVertices.Num());TangentY.Init(FVector::ZeroVector,ClothVertices.Num());
 for(int32 I=0;I<ClothTriangles.Num();I+=3){int32 A=ClothTriangles[I],B=ClothTriangles[I+1],C=ClothTriangles[I+2];FVector E1=ClothVertices[B]-ClothVertices[A],E2=ClothVertices[C]-ClothVertices[A],N=FVector::CrossProduct(E1,E2)*NormalSign;FVector2D U1=ClothUV[B]-ClothUV[A],U2=ClothUV[C]-ClothUV[A];float Det=U1.X*U2.Y-U1.Y*U2.X;FVector TX=FVector::ZeroVector,TY=FVector::ZeroVector;if(FMath::Abs(Det)>.00000001){TX=(E1*U2.Y-E2*U1.Y)/Det;TY=(E2*U1.X-E1*U2.X)/Det;}for(int32 V:{A,B,C}){GroupNormals[NormalGroups[V]]+=N;TangentX[V]+=TX;TangentY[V]+=TY;}}
 for(int32 I=0;I<ClothVertices.Num();++I){FVector N=GroupNormals[NormalGroups[I]].GetSafeNormal(SMALL_NUMBER,FVector::UpVector);FVector T=(TangentX[I]-N*FVector::DotProduct(N,TangentX[I])).GetSafeNormal(SMALL_NUMBER,FVector(1,0,0));ClothNormals[I]=N;ClothTangents[I]=FProcMeshTangent(T,FVector::DotProduct(FVector::CrossProduct(N,T),TangentY[I])<0);}
 if(ClothGeometry->GetNumSections()>0)ClothGeometry->UpdateMeshSection_LinearColor(0,ClothVertices,ClothNormals,ClothUV,ClothColors,ClothTangents,false);
}
