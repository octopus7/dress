#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "Dataflow/DataflowInstance.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Level.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftClothProbeAssetTest,
    "LaceLoft.Cloth.ProbeAssetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaceLoftClothProbeAssetTest::RunTest(const FString& Parameters)
{
    const UChaosClothAsset* Asset = LoadObject<UChaosClothAsset>(nullptr,
        TEXT("/Game/Development/ClothProbe/CA_TShirtProbe.CA_TShirtProbe"));
    if (!TestNotNull(TEXT("Authored Chaos Cloth probe exists"), Asset)) return false;
    TestNotNull(TEXT("Cloth shares the dummy skeleton"), Asset->GetSkeleton());
    TestNotNull(TEXT("Cloth has body collision"), Asset->GetPhysicsAsset());
    TestTrue(TEXT("Cloth imports the whole skeleton, not root-only fallback"),Asset->GetRefSkeleton().GetNum() >= 20);
    TestNotNull(TEXT("Dataflow authoring source is retained"),Asset->GetDataflowInstance().GetDataflowAsset());
    const auto& Collections = Asset->GetClothCollections();
    if (!TestEqual(TEXT("One authored probe LOD"),Collections.Num(),1)) return false;
    UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Collections[0]);
    TestTrue(TEXT("Valid panel schema"),Cloth.IsValid());
    TestTrue(TEXT("Front/back panels exist"),Cloth.GetNumSimPatterns() >= 2);
    TestTrue(TEXT("Panels have sewn connections"),Cloth.GetNumSeams() > 0);
    TestTrue(TEXT("Actual simulation mesh exists"),Cloth.GetNumSimVertices3D() > 100);
    TestTrue(TEXT("Actual render mesh exists"),Cloth.GetNumRenderVertices() > 100);
    const auto* Simulation = Asset->GetClothSimulationModel().Get();
    if (TestNotNull(TEXT("Built simulation model exists"),Simulation))
        TestEqual(TEXT("Built model keeps all simulation particles"),Simulation->GetNumVertices(0),Cloth.GetNumSimVertices3D());
    int32 Fixed=0,Free=0;
    for (float Distance:Cloth.GetWeightMap(TEXT("MaxDistance")))
    {
        Fixed += Distance == 0; Free += Distance > 0;
        TestTrue(TEXT("Max distance has valid cm values"), FMath::IsFinite(Distance) && Distance>=0 && Distance<=5);
    }
    TestTrue(TEXT("Shoulder anchors exist"),Fixed>0);
    TestTrue(TEXT("Moving fabric exists"),Free>100);
    for (const FVector3f& Position:Cloth.GetSimPosition3D())
        TestTrue(TEXT("Cloth authored at 15cm doll scale"),Position.Z>5 && Position.Z<11 && Position.Length()<16);
    int32 InvalidWeights=0,RootOnly=0;
    const auto Bones=Cloth.GetRenderBoneIndices();
    const auto Weights=Cloth.GetRenderBoneWeights();
    for (int32 I=0;I<Bones.Num();++I)
    {
        RootOnly += Bones[I].Num()==1 && Bones[I][0]==0;
        float Sum=0;
        for(float Weight:Weights[I]) Sum+=Weight;
        InvalidWeights += !FMath::IsNearlyEqual(Sum,1.f,.0001f);
    }
    TestEqual(TEXT("Render weights are normalized"),InvalidWeights,0);
    TestEqual(TEXT("Render conversion did not silently rebind to root"),RootOnly,0);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaceLoftClothProbeSceneTest,
    "LaceLoft.Cloth.ProbeSceneContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaceLoftClothProbeSceneTest::RunTest(const FString& Parameters)
{
    UWorld* World=LoadObject<UWorld>(nullptr,TEXT("/Game/Development/ClothProbe/L_ClothProbe.L_ClothProbe"));
    if(!TestNotNull(TEXT("Saved probe map exists for cooking"),World))return false;
    int32 ClothCount=0;
    for(AActor* Actor:World->PersistentLevel->Actors)
    {
        if(!Actor)continue;
        TArray<UChaosClothComponent*> Components; Actor->GetComponents(Components);
        for(UChaosClothComponent* Cloth:Components)
        {
            if(Cloth->GetFName()==TEXT("OtherCloth"))
            {TestNull(TEXT("Optional simultaneous probe slot starts empty"),Cloth->GetAsset());continue;}
            ++ClothCount;
            TestNotNull(TEXT("Saved cloth component references the authored asset"),Cloth->GetAsset());
            USkeletalMeshComponent* Body=Cast<USkeletalMeshComponent>(Cloth->GetAttachParent());
            if(TestNotNull(TEXT("Cloth attaches directly to skeletal pose source"),Body))
                TestNotNull(TEXT("Body mesh reference survives save"),Body->GetSkeletalMeshAsset());
            TestTrue(TEXT("Probe actor is not scaled to hide tiny cloth issues"),Actor->GetActorScale3D().Equals(FVector::OneVector));
        }
    }
    TestEqual(TEXT("Exactly one probe cloth component"),ClothCount,1);
    return !HasAnyErrors();
}
#endif
