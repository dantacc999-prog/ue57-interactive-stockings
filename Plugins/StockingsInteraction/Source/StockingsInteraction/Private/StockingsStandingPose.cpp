#include "StockingsStandingPose.h"
#include "StockingsInteractionActor.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

class FStockingsStandingProxy final : public FAnimInstanceProxy {
 FAnimNode_Base Root;TArray<FTransform> LocalPose;
public:
 explicit FStockingsStandingProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance){}
 virtual FAnimNode_Base* GetCustomRootNode()override{return &Root;}
 virtual void GetCustomNodes(TArray<FAnimNode_Base*>&Nodes)override{Nodes.Add(&Root);}
 virtual void PreUpdate(UAnimInstance* Instance,float Dt)override{FAnimInstanceProxy::PreUpdate(Instance,Dt);LocalPose=CastChecked<UStockingsStandingPose>(Instance)->StandingLocalPose;}
 virtual bool Evaluate(FPoseContext&Output)override{
  Output.ResetToRefPose();for(FCompactPoseBoneIndex I:Output.Pose.ForEachBoneIndex()){int32 M=Output.Pose.GetBoneContainer().MakeMeshPoseIndex(I).GetInt();if(LocalPose.IsValidIndex(M))Output.Pose[I]=LocalPose[M];}return true;
 }
};
FAnimInstanceProxy* UStockingsStandingPose::CreateAnimInstanceProxy(){return new FStockingsStandingProxy(this);}
void UStockingsStandingPose::DestroyAnimInstanceProxy(FAnimInstanceProxy*Proxy){delete Proxy;}

bool AStockingsInteractionActor::ApplyStandingPose(){
 if(!Body||!Body->GetSkeletalMeshAsset())return false;
 const auto&Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();const auto&CS=Body->GetComponentSpaceTransforms();if(CS.Num()!=Ref.GetNum())return false;
 TArray<FTransform>Local;Local.SetNum(CS.Num());for(int32 I=0;I<CS.Num();++I){int32 P=Ref.GetParentIndex(I);Local[I]=P<0?CS[I]:CS[I].GetRelativeTransform(CS[P]);}
 TArray<FTransform>RefCS;RefCS.SetNum(Ref.GetNum());for(int32 I=0;I<Ref.GetNum();++I){int32 P=Ref.GetParentIndex(I);RefCS[I]=P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*RefCS[P];}
 TArray<uint8>Leg;Leg.Init(0,Ref.GetNum());for(int32 I=0;I<Ref.GetNum();++I){FName Name=Ref.GetBoneName(I);int32 P=Ref.GetParentIndex(I);bool Upper=Name==TEXT("UpperLeg_L")||Name==TEXT("UpperLeg_R");Leg[I]=Upper||(P>=0&&Leg[P]);if(Leg[I])Local[I]=Ref.GetRefBonePose()[I];
  if(Upper){FVector Axis=P>=0?CS[P].GetRotation().Inverse().RotateVector(FVector(0,1,0)):FVector(0,1,0);float Sign=RefCS[I].GetLocation().X<0?1.f:-1.f;Local[I].SetRotation(FQuat(Axis,FMath::DegreesToRadians(6.f*Sign))*Local[I].GetRotation());}
 }
 SavedPostProcessClass=Body->GetPostProcessAnimBPClassToBeUsed();SavedDisablePostProcess=Body->GetDisablePostProcessBlueprint();SavedPrimaryInstance=Body->GetAnimInstance();
 Body->SetOverridePostProcessAnimBP(UStockingsStandingPose::StaticClass(),true);Body->SetDisablePostProcessBlueprint(false);
 auto*Instance=Cast<UStockingsStandingPose>(Body->GetPostProcessInstance());if(!Instance)return false;Instance->StandingLocalPose=MoveTemp(Local);StandingPoseApplied=true;
 Body->TickAnimation(.001f,false);Body->RefreshBoneTransforms();Body->MarkRenderDynamicDataDirty();
 UE_LOG(LogTemp,Display,TEXT("Stockings fitting stance installed; primary animation instance preserved=%d"),SavedPrimaryInstance==Body->GetAnimInstance());return true;
}
void AStockingsInteractionActor::RestoreStandingPose(){
 if(!StandingPoseApplied||!Body)return;Body->SetOverridePostProcessAnimBP(SavedPostProcessClass,true);Body->SetDisablePostProcessBlueprint(SavedDisablePostProcess);StandingPoseApplied=false;
 Body->TickAnimation(.001f,false);Body->RefreshBoneTransforms();Body->MarkRenderDynamicDataDirty();
}
