#include "StockingsInteractionActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Async/ParallelFor.h"

namespace {
bool AcceptedTrianglesCross(const FVector*P,const FVector*Q){
 FVector E[3],F[3];for(int32 K=0;K<3;++K){E[K]=P[(K+1)%3]-P[K];F[K]=Q[(K+1)%3]-Q[K];}
 FVector N=FVector::CrossProduct(E[0],E[1]),M=FVector::CrossProduct(F[0],F[1]);
 auto Separated=[&](FVector Axis){double L=Axis.SizeSquared();if(L<1.e-18)return false;double AMin=MAX_dbl,AMax=-MAX_dbl,BMin=MAX_dbl,BMax=-MAX_dbl;for(int32 K=0;K<3;++K){double A=P[K]|Axis,B=Q[K]|Axis;AMin=FMath::Min(AMin,A);AMax=FMath::Max(AMax,A);BMin=FMath::Min(BMin,B);BMax=FMath::Max(BMax,B);}return FMath::Max(BMin-AMax,AMin-BMax)>=-1.e-5*FMath::Sqrt(L);};
 if(Separated(N)||Separated(M))return false;
 for(int32 K=0;K<3;++K)if(Separated(FVector::CrossProduct(N,E[K]))||Separated(FVector::CrossProduct(M,F[K])))return false;
 for(int32 I=0;I<3;++I)for(int32 J=0;J<3;++J)if(Separated(FVector::CrossProduct(E[I],F[J])))return false;
 return true;
}
}

// Whole-garment response controller. This transfers the accepted drag/release
// behaviour to the original mesh; it is not the cylinder's implicit solver.
void AStockingsInteractionActor::InitializeAcceptedInteraction(){
 AcceptedDrop=AcceptedTargetDrop=AcceptedGrabDrop=0;AcceptedDownGrab=false;AcceptedPeel=FVector::ZeroVector;AcceptedGeometryChanged=true;AcceptedBaselineValidated=false;
 AcceptedPatchWeights.Init(0,SimPositions.Num());AcceptedAnkleRef=BottomZ+8;
 if(Stock&&Stock->GetSkeletalMeshAsset()){
  const auto&Ref=Stock->GetSkeletalMeshAsset()->GetRefSkeleton();TArray<FTransform>CS;CS.SetNum(Ref.GetNum());
  for(int32 I=0;I<Ref.GetNum();++I){int32 P=Ref.GetParentIndex(I);CS[I]=P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*CS[P];}
  float Ankle=-MAX_flt;for(FName Name:{FName(TEXT("Foot_L")),FName(TEXT("Foot_R"))}){int32 I=Ref.FindBoneIndex(Name);if(I>=0)Ankle=FMath::Max(Ankle,float(CS[I].GetLocation().Z)+3.f);}
  if(Ankle>-MAX_flt)AcceptedAnkleRef=Ankle;
 }
 AcceptedAnkleRef=FMath::Clamp(AcceptedAnkleRef,BottomZ+5,TopZ-15);AcceptedMaxDrop=TopZ-AcceptedAnkleRef;
 AcceptedRenderRepresentatives.Init(-1,NormalGroupCount);for(int32 I=0;I<NormalGroups.Num();++I)if(AcceptedRenderRepresentatives[NormalGroups[I]]<0)AcceptedRenderRepresentatives[NormalGroups[I]]=I;
 AcceptedBaseRenderNormals.Init(FVector::ZeroVector,NormalGroupCount);for(int32 F=0;F<ClothTriangles.Num();F+=3){int32 A=ClothTriangles[F],B=ClothTriangles[F+1],C=ClothTriangles[F+2];FVector N=FVector::CrossProduct(BaseVertices[B]-BaseVertices[A],BaseVertices[C]-BaseVertices[A])*NormalSign;for(int32 V:{A,B,C})AcceptedBaseRenderNormals[NormalGroups[V]]+=N;}for(auto&N:AcceptedBaseRenderNormals)N=N.GetSafeNormal();
 BodyQuerySeeds.Init(-1,SimPositions.Num()+SimTriangles.Num()+ClothVertices.Num());
 BuildSwimBottomContact();BuildAcceptedVisualAtlas();AcceptedVisualCompression.Init(0,NormalGroupCount);AcceptedVisualHem=TopZ;
 for(int32 I=0;I<ClothVertices.Num();++I)ClothVertices[I]=AcceptedBaseline[NormalGroups[I]];
 FitReady=true;SimulationSeconds=0;
}

void AStockingsInteractionActor::StepAcceptedInteraction(float Dt){
 if(SimPositions.IsEmpty())return;double Start=FPlatformTime::Seconds();float H=FMath::Clamp(Dt,0.f,.1f);SimulationSeconds+=H;SimPrevious=SimPositions;
 FVector PeelTarget=FVector::ZeroVector;
 if(Dragging&&GrabParticle>=0){
  FVector D=SimGrabTarget-SimGrabStart;
  if(D.Z<-.5&&-D.Z>FVector2D(D.X,D.Y).Size()*1.3)AcceptedDownGrab=true;
  if(AcceptedDownGrab)AcceptedTargetDrop=FMath::Clamp(AcceptedGrabDrop-float(D.Z),0.f,AcceptedMaxDrop);
  else PeelTarget=(D*.6).GetClampedToMaxSize(5);
 }
 const float PreviousDrop=AcceptedDrop;const FVector PreviousPeel=AcceptedPeel;
 AcceptedDrop=FMath::FInterpConstantTo(AcceptedDrop,AcceptedTargetDrop,H,32);
 AcceptedPeel=FMath::Lerp(AcceptedPeel,PeelTarget,1-FMath::Exp(-FMath::Max(SnapReturnRate,1.f)*H));
 if(!Dragging&&AcceptedPeel.SizeSquared()<1.e-10)AcceptedPeel=FVector::ZeroVector;
 if(!AcceptedGeometryChanged&&FMath::Abs(AcceptedDrop-PreviousDrop)<1.e-6&&(AcceptedPeel-PreviousPeel).SizeSquared()<1.e-12){SimTimeMs=0;return;}
 AcceptedGeometryChanged=true;
 const float Progress=AcceptedDrop/FMath::Max(AcceptedMaxDrop,1.f);
 // A travelling gather front leaves the cloth below it fitted and untouched.
 // A finite ankle band accommodates the retained fabric without crushing all
 // axial rows into a single plane. The height map is monotonic and C1.
 AcceptedVisualHem=TopZ-AcceptedDrop+6.f*Progress*Progress*Progress;
 float Band=FMath::Lerp(10.f,6.f,Progress*Progress),Base=FMath::Max(AcceptedAnkleRef,AcceptedVisualHem-Band);
 float L=FMath::Max(TopZ-Base,.01f),B=FMath::Max(AcceptedVisualHem-Base,.01f),Transition=FMath::Min(3.f,L*.25f),R=FMath::Clamp((B-Transition)/(L-Transition),.025f,1.f);
 auto IntegratedRamp=[](float T){T=FMath::Clamp(T,0.f,1.f);return T-T*T*T+.5f*T*T*T*T;};
 TArray<FVector> Displacement;Displacement.SetNum(NormalGroupCount);
 ParallelFor(NormalGroupCount,[&](int32 G){int32 I=AcceptedRenderRepresentatives[G];float Z=RestVertices[I].Z,Height=Z,Derivative=1;
  if(Progress>1.e-6&&Z>Base){float X=Z-Base,U=FMath::Clamp(X/Transition,0.f,1.f),V=FMath::Clamp((X-(L-Transition))/Transition,0.f,1.f);
   Height=Base+R*X+(1-R)*Transition*(IntegratedRamp(U)+V-IntegratedRamp(V));
   Derivative=R+(1-R)*(1-3*U*U+2*U*U*U+3*V*V-2*V*V*V);
  }
  float Compression=1-Derivative;AcceptedVisualCompression[G]=Compression;
  Displacement[G]=MapAcceptedVisualVertex(G,Height,Derivative,Compression)-AcceptedBaseline[G];
 });
 // Relax the displacement field, retaining original surface detail rather
 // than smoothing the mesh itself or repelling individual fold vertices.
 for(int32 K=0;K<3;++K){auto Old=Displacement;ParallelFor(NormalGroupCount,[&](int32 G){int32 I=AcceptedRenderRepresentatives[G];if(RestVertices[I].Z<=AcceptedAnkleRef||Old[G].SizeSquared()<1.e-10)return;FVector Mean=FVector::ZeroVector;for(int32 N:AcceptedVisualNeighbors[G])Mean+=Old[N];if(!AcceptedVisualNeighbors[G].IsEmpty())Displacement[G]=FMath::Lerp(Old[G],Mean/AcceptedVisualNeighbors[G].Num(),.22);});}
 TArray<FVector> Positions;Positions.SetNum(NormalGroupCount);ParallelFor(NormalGroupCount,[&](int32 G){int32 I=AcceptedRenderRepresentatives[G];float Weight=0;if(I<SourceVertexCount)Weight=AcceptedPatchWeights[RenderParticle[I]];else Weight=(AcceptedPatchWeights[RenderParticle[SourceA[I]]]+AcceptedPatchWeights[RenderParticle[SourceB[I]]])*.5;Positions[G]=AcceptedBaseline[G]+Displacement[G]+AcceptedPeel*Weight;});
 for(int32 I=0;I<ClothVertices.Num();++I)ClothVertices[I]=Positions[NormalGroups[I]];
 SlideCm=AcceptedDrop;RemovalProgress=Progress;FullyRemoved=false;FitReady=true;BodyContactCount=SelfContactCount=0;
 BindAcceptedRender();MaxStepDisplacement=0;for(int32 I=0;I<SimPositions.Num();++I)MaxStepDisplacement=FMath::Max(MaxStepDisplacement,float(FVector::Distance(SimPositions[I],SimPrevious[I])));
 MaxStretch=1;for(const auto&E:StretchConstraints)if(E.FittedLength>.03f)MaxStretch=FMath::Max(MaxStretch,float(FVector::Distance(SimPositions[E.A],SimPositions[E.B])/E.FittedLength));
 SimTimeMs=(FPlatformTime::Seconds()-Start)*1000;
}

void AStockingsInteractionActor::BindAcceptedRender(){
 ClothColors.Init(FLinearColor::White,ClothVertices.Num());
 TArray<FVector>Positions;Positions.SetNum(NormalGroupCount);TArray<float>Fit;Fit.Init(1,NormalGroupCount);
 TArray<float>Distances;Distances.Init(.18f,NormalGroupCount);
 ParallelFor(NormalGroupCount,[&](int32 G){int32 I=AcceptedRenderRepresentatives[G];FVector P=ClothVertices[I],Q,N;float D=MAX_flt;
  if(RestVertices[I].Z<=AcceptedAnkleRef){Positions[G]=AcceptedBaseline[G];return;}
  if(AcceptedBaselineValidated&&FVector::DistSquared(P,AcceptedBaseline[G])<1.e-10){Positions[G]=P;return;}
  int32 Seed=SimPositions.Num()+SimTriangles.Num()+I;
  float Gap=FMath::Max(AcceptedRenderClearanceCm,.1f);
  for(int32 K=0;K<8;++K){if(!NearestBody(P,Q,N,D,Seed)||D>=Gap-.0001f)break;P+=N*(Gap-D);}
  ProjectSwimBottom(P);
  NearestBody(P,Q,N,D,Seed);
  Positions[G]=P;Distances[G]=D;Fit[G]=1-FMath::Clamp((FMath::Abs(D)-Gap-.08f)/.8f,0.f,1.f);
 });
 // Resolve actual render-triangle crossings too. Point projection alone can
 // leave a triangle edge through a curved skin surface. Corrections operate
 // on seam groups, so every duplicate UV vertex receives the same position.
 for(int32 Pass=0;Pass<8;++Pass){
  TArray<FVector>Pushes;Pushes.Init(FVector::ZeroVector,NormalGroupCount);int32 Crossings=0;
  for(int32 F=0;F<ClothTriangles.Num();F+=3){
   if(RestVertices[ClothTriangles[F]].Z<=AcceptedAnkleRef||RestVertices[ClothTriangles[F+1]].Z<=AcceptedAnkleRef||RestVertices[ClothTriangles[F+2]].Z<=AcceptedAnkleRef)continue;
   int32 G[3]={NormalGroups[ClothTriangles[F]],NormalGroups[ClothTriangles[F+1]],NormalGroups[ClothTriangles[F+2]]};FVector P[3]={Positions[G[0]],Positions[G[1]],Positions[G[2]]};FBox Bounds(ForceInit);for(auto V:P)Bounds+=V;
   if(AcceptedBaselineValidated&&FVector::DistSquared(P[0],AcceptedBaseline[G[0]])+FVector::DistSquared(P[1],AcceptedBaseline[G[1]])+FVector::DistSquared(P[2],AcceptedBaseline[G[2]])<1.e-9)continue;
   TArray<int32,TInlineAllocator<64>>Stack;if(!BodyTree.IsEmpty())Stack.Add(0);
   while(!Stack.IsEmpty()){
    const auto&Node=BodyTree[Stack.Pop(EAllowShrinking::No)];if(!Bounds.Intersect(Node.Bounds))continue;
    if(Node.Left>=0){Stack.Add(Node.Left);Stack.Add(Node.Right);continue;}
    for(int32 J=Node.Start;J<Node.Start+Node.Count;++J){int32 B=BodyOrder[J]*3;FVector Q[3]={BodyPositions[BodyTriangles[B]],BodyPositions[BodyTriangles[B+1]],BodyPositions[BodyTriangles[B+2]]};FBox QB(ForceInit);for(auto V:Q)QB+=V;if(!Bounds.Intersect(QB))continue;
     if(!AcceptedTrianglesCross(P,Q))continue;FVector N=FVector::CrossProduct(Q[1]-Q[0],Q[2]-Q[0]).GetSafeNormal();for(int32 K=0;K<3;++K){float Depth=FMath::Clamp(.03f-float((P[K]-Q[0])|N),0.f,.2f);FVector Push=N*Depth;if(Push.SizeSquared()>Pushes[G[K]].SizeSquared())Pushes[G[K]]=Push;}++Crossings;
    }
   }
   if(Bounds.Intersect(SwimBottomBounds))for(int32 B=0;B<SwimBottomFaceBounds.Num();++B){
    if(!Bounds.Intersect(SwimBottomFaceBounds[B]))continue;
    FVector Q[3]={SwimBottomPositions[SwimBottomTriangles[B*3]],SwimBottomPositions[SwimBottomTriangles[B*3+1]],SwimBottomPositions[SwimBottomTriangles[B*3+2]]};
    if(!AcceptedTrianglesCross(P,Q))continue;
    FVector N=SwimBottomFaceOutward[B];float Minimum=MAX_flt;
    for(int32 K=0;K<3;++K)Minimum=FMath::Min(Minimum,float((P[K]-Q[0])|N));
    const FVector Push=N*FMath::Clamp(.03f-Minimum,0.f,.3f);
    for(int32 K=0;K<3;++K)if(Push.SizeSquared()>Pushes[G[K]].SizeSquared())Pushes[G[K]]=Push;
    ++Crossings;
   }
  }
  if(!Crossings)break;for(int32 G=0;G<Positions.Num();++G)Positions[G]+=Pushes[G];
 }
 MinBodyClearance=MAX_flt;ClearanceFailures=0;for(int32 G=0;G<NormalGroupCount;++G)if(RestVertices[AcceptedRenderRepresentatives[G]].Z>AcceptedAnkleRef){MinBodyClearance=FMath::Min(MinBodyClearance,Distances[G]);ClearanceFailures+=Distances[G]<.12f;}
 // Conserve the local fiber amount over the rendered surface. UV seam copies
 // share one density, so transparency cannot open a line along the seam.
 TArray<float> Area;Area.Init(0,NormalGroupCount);
 for(int32 F=0;F<ClothTriangles.Num();F+=3){int32 A=NormalGroups[ClothTriangles[F]],B=NormalGroups[ClothTriangles[F+1]],C=NormalGroups[ClothTriangles[F+2]];float W=FVector::CrossProduct(Positions[B]-Positions[A],Positions[C]-Positions[A]).Size();Area[A]+=W;Area[B]+=W;Area[C]+=W;}
 if(!AcceptedBaselineValidated){AcceptedBaseline=Positions;AcceptedBaselineArea=Area;AcceptedBaselineValidated=true;}
 TArray<float> Density;Density.SetNum(NormalGroupCount);
 for(int32 G=0;G<NormalGroupCount;++G)Density[G]=FMath::Clamp(AcceptedBaselineArea[G]/FMath::Max(Area[G],1.e-6f),.62f,2.4f);
 const auto RawDensity=Density;
 for(int32 G=0;G<NormalGroupCount;++G){float Sum=0;for(int32 N:AcceptedVisualNeighbors[G])Sum+=RawDensity[N];if(!AcceptedVisualNeighbors[G].IsEmpty())Density[G]=FMath::Lerp(RawDensity[G],Sum/AcceptedVisualNeighbors[G].Num(),.25f);}
 // Procedural-mesh vertex colors are stored as UNorm8; encode density in [0,1].
 for(int32 I=0;I<ClothVertices.Num();++I){int32 G=NormalGroups[I];ClothVertices[I]=Positions[G];float Cuff=FMath::Clamp(1-float(TopZ-RestVertices[I].Z)/2.f,0.f,1.f);ClothColors[I]=FLinearColor(FMath::Clamp(Density[G]+.25f*Cuff*RemovalProgress,.62f,2.4f)/2.4f,Fit[G],0,1);}
 for(int32 I=0;I<SimPositions.Num();++I)SimPositions[I]=Positions[AcceptedParticleRender[I]];
}

FString AStockingsInteractionActor::GetAcceptedStats()const{
 return FString::Printf(TEXT("{\"mode\":\"accepted_response\",\"scope\":\"whole original pantyhose mesh, guided axial gathering to ankle; not full coupled cloth\",\"particles\":%d,\"faces\":%d,\"render_vertices\":%d,\"render_faces\":%d,\"body_faces\":%d,\"fit_ready\":%s,\"dragging\":%s,\"simulation_seconds\":%.6f,\"solver_ms\":%.4f,\"hem_drop_cm\":%.6f,\"ankle_reference_z_cm\":%.6f,\"max_drop_cm\":%.6f,\"progress\":%.6f,\"peel_cm\":[%.6f,%.6f,%.6f],\"min_body_clearance_cm\":%.6f,\"clearance_failures\":%d,\"max_stretch\":%.6f,\"self_contacts\":%d,\"fully_removed\":false}"),SimPositions.Num(),SimTriangles.Num(),ClothVertices.Num(),ClothTriangles.Num()/3,BodyTriangles.Num()/3,FitReady?TEXT("true"):TEXT("false"),Dragging?TEXT("true"):TEXT("false"),SimulationSeconds,SimTimeMs,AcceptedDrop,AcceptedAnkleRef,AcceptedMaxDrop,RemovalProgress,AcceptedPeel.X,AcceptedPeel.Y,AcceptedPeel.Z,MinBodyClearance,ClearanceFailures,MaxStretch,SelfContactCount);
}

FString AStockingsInteractionActor::GetAcceptedValidationMesh()const{
 FString S=TEXT("{\"positions_cm\":[");for(int32 I=0;I<ClothVertices.Num();++I){auto P=ClothVertices[I];S+=FString::Printf(TEXT("%s[%.9g,%.9g,%.9g]"),I?TEXT(","):TEXT(""),P.X,P.Y,P.Z);}
 S+=TEXT("],\"triangles\":[");for(int32 I=0;I<ClothTriangles.Num();++I)S+=FString::Printf(TEXT("%s%d"),I?TEXT(","):TEXT(""),ClothTriangles[I]);
 S+=TEXT("],\"seam_groups\":[");for(int32 I=0;I<NormalGroups.Num();++I)S+=FString::Printf(TEXT("%s%d"),I?TEXT(","):TEXT(""),NormalGroups[I]);
 S+=TEXT("],\"fiber_density\":[");for(int32 I=0;I<ClothColors.Num();++I)S+=FString::Printf(TEXT("%s%.9g"),I?TEXT(","):TEXT(""),ClothColors[I].R*2.4f);
 // Verify the actual 8-bit render buffer as well as simulation-side density.
 S+=TEXT("],\"render_fiber_density\":[");if(ClothGeometry)if(const auto*Section=ClothGeometry->GetProcMeshSection(0))for(int32 I=0;I<Section->ProcVertexBuffer.Num();++I)S+=FString::Printf(TEXT("%s%.9g"),I?TEXT(","):TEXT(""),float(Section->ProcVertexBuffer[I].Color.R)*2.4f/255.f);
 S+=TEXT("],\"body_positions_cm\":[");for(int32 I=0;I<BodyPositions.Num();++I){auto P=BodyPositions[I];S+=FString::Printf(TEXT("%s[%.9g,%.9g,%.9g]"),I?TEXT(","):TEXT(""),P.X,P.Y,P.Z);}
 S+=TEXT("],\"body_triangles\":[");for(int32 I=0;I<BodyTriangles.Num();++I)S+=FString::Printf(TEXT("%s%d"),I?TEXT(","):TEXT(""),BodyTriangles[I]);
 S+=TEXT("],\"swim_positions_cm\":[");for(int32 I=0;I<SwimBottomPositions.Num();++I){auto P=SwimBottomPositions[I];S+=FString::Printf(TEXT("%s[%.9g,%.9g,%.9g]"),I?TEXT(","):TEXT(""),P.X,P.Y,P.Z);}
 S+=TEXT("],\"swim_triangles\":[");for(int32 I=0;I<SwimBottomTriangles.Num();++I)S+=FString::Printf(TEXT("%s%d"),I?TEXT(","):TEXT(""),SwimBottomTriangles[I]);return S+TEXT("]}");
}
