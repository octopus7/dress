#include "Fitting/LaceLoftDummyAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "BonePose.h"
#include "BoneContainer.h"

namespace
{
struct FDummyPoseProxy final:FAnimInstanceProxy
{
    explicit FDummyPoseProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance){}
    UAnimSequence* Sequence=nullptr;
    TArray<TPair<int32,FQuat>> Offsets;
    void PreUpdate(UAnimInstance* Instance,float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,DeltaSeconds);
        const auto* Source=CastChecked<ULaceLoftDummyAnimInstance>(Instance);Sequence=Source->SelectedPose;Offsets.Reset();
        if(auto* Mesh=Source->GetSkelMeshComponent())for(const auto& Pair:Source->JointOffsets)
        {const int32 Index=Mesh->GetBoneIndex(Pair.Key);if(Index!=INDEX_NONE)Offsets.Emplace(Index,Pair.Value.Quaternion());}
    }
    bool Evaluate(FPoseContext& Output) override
    {
        Output.ResetToRefPose();
        if(Sequence){FAnimationPoseData Data(Output);Sequence->GetAnimationPose(Data,FAnimExtractContext(0.,false));}
        for(const auto& Pair:Offsets)
        {
            const FCompactPoseBoneIndex Bone=Output.Pose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(Pair.Key));
            if(Bone==INDEX_NONE)continue;
            auto& Local=Output.Pose[Bone];Local.SetRotation((Local.GetRotation()*Pair.Value).GetNormalized());
        }
        return true;
    }
};
}
FAnimInstanceProxy* ULaceLoftDummyAnimInstance::CreateAnimInstanceProxy(){return new FDummyPoseProxy(this);}
void ULaceLoftDummyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
