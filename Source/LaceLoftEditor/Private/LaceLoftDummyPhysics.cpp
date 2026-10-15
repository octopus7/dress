#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

// Fits the final dummy's measured anatomical cross sections in UE centimetres.
// Generic auto-fit has a minimum radius and greatly overextends this 15cm head.
int32 AuthorDummyPhysics()
{
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Dummy/SK_Dummy.SK_Dummy"));
    if(!Mesh || !Mesh->GetPhysicsAsset())return 1;
    auto* Physics=Mesh->GetPhysicsAsset();
    const auto& Ref=Mesh->GetRefSkeleton();
    TArray<FTransform> Component=Ref.GetRefBonePose();
    for(int32 I=1;I<Component.Num();++I)Component[I]*=Component[Ref.GetParentIndex(I)];
    auto Capsule=[&](FName Name,FVector A,FVector B,float Radius)
    {
        const int32 BodyIndex=Physics->FindBodyIndex(Name);
        if(BodyIndex==INDEX_NONE)return false;
        USkeletalBodySetup* Body=Physics->SkeletalBodySetups[BodyIndex];
        Body->AggGeom.EmptyElements();
        FKSphylElem Shape;
        Shape.Radius=Radius;Shape.Length=FVector::Distance(A,B);
        const FTransform World(FQuat::FindBetweenNormals(FVector::UpVector,(B-A).GetSafeNormal()),(A+B)*.5);
        Shape.SetTransform(World.GetRelativeTransform(Component[Ref.FindBoneIndex(Name)]));
        Body->AggGeom.SphylElems.Add(Shape);
        Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();
        return true;
    };
    bool Valid=true;
    Valid &= Capsule(TEXT("head"),FVector(-.08,0,12.15),FVector(-.08,0,12.75),2.1f);
    Valid &= Capsule(TEXT("neck"),FVector(0,0,9.5),FVector(0,0,9.95),.5f);
    Valid &= Capsule(TEXT("chest"),FVector(0,-.45,8.60),FVector(0,.45,8.60),1.04f);
    Valid &= Capsule(TEXT("spine_01"),FVector(0,-.65,7.80),FVector(0,.65,7.80),.98f);
    // Ray-parity validation localizes sitting penetration to the lateral pelvis.
    Valid &= Capsule(TEXT("pelvis"),FVector(0,-.65,6.55),FVector(0,.65,6.55),1.12f);
    for(int32 Side:{-1,1})
    {
        const FString Suffix=Side==-1?TEXT("_l"):TEXT("_r");
        Valid &= Capsule(FName(TEXT("clavicle")+Suffix),FVector(0,Side*.65,9.20),FVector(0,Side*1.3,9.16),.50f);
        Valid &= Capsule(FName(TEXT("upperarm")+Suffix),FVector(0,Side*1.70,8.90),FVector(0,Side*2.65,7.78),.67f);
        Valid &= Capsule(FName(TEXT("forearm")+Suffix),FVector(.01,Side*3.10,7.23),FVector(.03,Side*3.85,6.35),.36f);
        Valid &= Capsule(FName(TEXT("hand")+Suffix),FVector(.07,Side*4.15,6.00),FVector(.10,Side*4.40,5.68),.27f);
        Valid &= Capsule(FName(TEXT("thigh")+Suffix),FVector(0,Side*.87,5.65),FVector(.03,Side*.94,4.24),.86f);
        Valid &= Capsule(FName(TEXT("calf")+Suffix),FVector(.01,Side*.97,2.90),FVector(-.03,Side*.98,1.55),.49f);
        Valid &= Capsule(FName(TEXT("foot")+Suffix),FVector(.03,Side*.98,.43),FVector(.98,Side*.98,.43),.42f);
    }
    if(!Valid)return 2;
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    const FString Filename=FPackageName::LongPackageNameToFilename(Physics->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    if(!UPackage::SavePackage(Physics->GetOutermost(),Physics,*Filename,Args))return 3;
    UE_LOG(LogTemp,Display,TEXT("Saved 19 anatomically fitted dummy collision bodies in cm."));
    return 0;
}
