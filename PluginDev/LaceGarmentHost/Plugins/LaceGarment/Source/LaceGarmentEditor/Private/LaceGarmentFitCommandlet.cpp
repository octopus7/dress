#include "LaceGarmentFitCommandlet.h"
#include "LaceGarmentTemplates.h"
#include "LaceGarmentSampling.h"
#include "LaceGarmentInspection.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/MeshNormals.h"
#include "Materials/MaterialInterface.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

ULaceGarmentFitCommandlet::ULaceGarmentFitCommandlet()
{
    IsClient=true; IsServer=false; IsEditor=true; LogToConsole=true;
}
int32 ULaceGarmentFitCommandlet::Main(const FString& Params)
{
    using namespace LaceGarment;
    FString Directory,Kind;
    if(!FParse::Value(*Params,TEXT("ReportDir="),Directory) || !FParse::Value(*Params,TEXT("Template="),Kind)) return 2;
    if(Kind!=TEXT("TShirt") && Kind!=TEXT("DolphinShorts")) return 2;
    IFileManager::Get().MakeDirectory(*Directory,true);
    FString Error; auto* Document=NewObject<ULaceGarmentDocument>(); Document->AddToRoot();
    FString DiagnosticDraft;
    if(FParse::Value(*Params,TEXT("SourceDraft="),DiagnosticDraft))
    {
        // Development-only replay of the exported diagnostic document. This is
        // not the versioned, hashed .lgarment archive import/export path.
        FString Json;
        if(!FFileHelper::LoadFileToString(Json,*DiagnosticDraft)
            || !FJsonObjectConverter::JsonObjectStringToUStruct(Json,Document,0,CPF_Transient)
            || !Document->ValidateStructure(Error))
        { UE_LOG(LogTemp,Error,TEXT("Invalid diagnostic source draft: %s"),*Error); Document->RemoveFromRoot(); return 3; }
    }
    else if(!ApplyTemplate(*Document,Kind==TEXT("TShirt")?ETemplate::TShirt:ETemplate::DolphinShorts,{},Error))
    { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); Document->RemoveFromRoot(); return 3; }
    FString Source;
    FJsonObjectConverter::UStructToJsonObjectString(ULaceGarmentDocument::StaticClass(),Document,Source,0,CPF_Transient);
    if(!FFileHelper::SaveStringToFile(Source,*(Directory/TEXT("source-draft.json")))) return 4;
    FSampledDocument Sampled;
    if(!SampleDocument(*Document,Sampled,Error)) { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 5; }
    auto* Body=LoadObject<USkeletalMesh>(nullptr,TEXT("/LaceGarment/Authoring/Body/SK_OB11.SK_OB11"));
    if(!ReadBodyCollision(Body,Sampled.Input.Colliders,Sampled.Input.ConvexColliders,Error)) { UE_LOG(LogTemp,Error,TEXT("%s"),*Error); return 6; }
    FParse::Value(*Params,TEXT("MaxSteps="),Sampled.Input.MaxSteps);
    FParse::Value(*Params,TEXT("GravityCm="),Sampled.Input.GravityCm);
    FParse::Value(*Params,TEXT("BendingStrength="),Sampled.Input.BendingStrength);
    FParse::Value(*Params,TEXT("Iterations="),Sampled.Input.Iterations);
    Sampled.Input.Progress=[](const FBuildResult& B)
    {UE_LOG(LogTemp,Display,TEXT("Fit progress: %d steps, seam %.5fcm, edge %.5f, penetration %.5fcm, speed %.5fcm/s"),B.Steps,B.MaxSeamGapCm,B.MaxRelativeEdgeError,B.MaxPenetrationCm,B.MaxSpeedCmPerSec);};
    FBuildResult Build;
    const bool Fitted=BuildGarment(Sampled.Input,Build,Error);
    const FString FitError=Error;
    auto Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("scope"),TEXT("Template fitting diagnostic; not a validated garment, archive or runtime acceptance"));
    Report->SetStringField(TEXT("template"),Kind);
    Report->SetBoolField(TEXT("replayedDiagnosticDraft"),!DiagnosticDraft.IsEmpty());
    Report->SetStringField(TEXT("generatorVersion"),GeneratorVersion);
    Report->SetBoolField(TEXT("fitted"),Fitted); Report->SetStringField(TEXT("error"),FitError);
    Report->SetNumberField(TEXT("steps"),Build.Steps); Report->SetNumberField(TEXT("vertices"),Build.InitialCm.Num());
    Report->SetNumberField(TEXT("maxSteps"),Sampled.Input.MaxSteps);
    Report->SetNumberField(TEXT("diagnosticGravityCmPerSecSquared"),Sampled.Input.GravityCm);
    Report->SetNumberField(TEXT("diagnosticBendingStrengthPerIteration"),Sampled.Input.BendingStrength);
    Report->SetNumberField(TEXT("iterationsPerStep"),Sampled.Input.Iterations);
    Report->SetNumberField(TEXT("maxSeamGapCm"),Build.MaxSeamGapCm); Report->SetNumberField(TEXT("maxRelativeEdgeError"),Build.MaxRelativeEdgeError);
    Report->SetBoolField(TEXT("surfaceMeasured"),Build.bSurfaceMeasured);
    Report->SetNumberField(TEXT("maxVertexPenetrationCm"),Build.MaxPenetrationCm);
    Report->SetNumberField(TEXT("maxSpeedCmPerSec"),Build.MaxSpeedCmPerSec);
    Report->SetNumberField(TEXT("minAreaRatio"),Build.MinAreaRatio); Report->SetNumberField(TEXT("maxAreaRatio"),Build.MaxAreaRatio);
    Report->SetNumberField(TEXT("maxFacePenetrationCm"),Build.MaxFacePenetrationCm); Report->SetNumberField(TEXT("maxInternalFoldDegrees"),Build.MaxInternalFoldDegrees);
    auto Numbers=[](FVector3d P)
    { return TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}; };
    TArray<TSharedPtr<FJsonValue>> Colliders;
    for(const auto& C : Sampled.Input.Colliders)
    {
        auto Value=MakeShared<FJsonObject>(); Value->SetArrayField(TEXT("aCm"),Numbers(C.A)); Value->SetArrayField(TEXT("bCm"),Numbers(C.B)); Value->SetNumberField(TEXT("radiusCm"),C.RadiusCm);
        Colliders.Add(MakeShared<FJsonValueObject>(Value));
    }
    Report->SetArrayField(TEXT("capsules"),Colliders);
    TArray<TSharedPtr<FJsonValue>> Hulls;
    for(const auto& C : Sampled.Input.ConvexColliders)
    {
        auto Value=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Vertices,Planes;
        for(auto P : C.GetVertices()) Vertices.Add(MakeShared<FJsonValueArray>(Numbers(P)));
        for(const auto& P : C.GetPlanes())
        {auto Plane=Numbers(FVector3d(P.X,P.Y,P.Z));Plane.Add(MakeShared<FJsonValueNumber>(P.W));Planes.Add(MakeShared<FJsonValueArray>(Plane));}
        Value->SetArrayField(TEXT("verticesCm"),Vertices);Value->SetArrayField(TEXT("planesCm"),Planes);Hulls.Add(MakeShared<FJsonValueObject>(Value));
    }
    Report->SetArrayField(TEXT("convexes"),Hulls);
    double InitialPenetration=0;
    for(auto P : Build.InitialCm) for(const auto& C : Sampled.Input.ConvexColliders)
    {FVector3d N;InitialPenetration=FMath::Max(InitialPenetration,-C.SignedDistance(P,N));}
    for(auto P : Build.InitialCm) for(const auto& C : Sampled.Input.Colliders)
    {
        const auto Axis=C.B-C.A;
        const auto Q=C.A+Axis*FMath::Clamp(FVector3d::DotProduct(P-C.A,Axis)/FMath::Max(Axis.SquaredLength(),1.e-12),0.0,1.0);
        InitialPenetration=FMath::Max(InitialPenetration,C.RadiusCm-(P-Q).Length());
    }
    Report->SetNumberField(TEXT("initialVertexPenetrationCm"),InitialPenetration);
    auto Geometry=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Initial,Settled,Panels;
    for(auto P : Build.InitialCm) Initial.Add(MakeShared<FJsonValueArray>(Numbers(P)));
    for(auto P : Build.SettledCm) Settled.Add(MakeShared<FJsonValueArray>(Numbers(P)));
    for(int32 I=0;I<Build.Panels.Num();++I)
    {
        auto Panel=MakeShared<FJsonObject>(); TArray<TSharedPtr<FJsonValue>> Rest,Faces;
        for(auto P : Build.Panels[I].RestMm) Rest.Add(MakeShared<FJsonValueArray>(Numbers(FVector3d(P.X,P.Y,0))));
        for(auto T : Build.Panels[I].Triangles) Faces.Add(MakeShared<FJsonValueArray>(Numbers(FVector3d(T.X,T.Y,T.Z))));
        Panel->SetNumberField(TEXT("offset"),Build.Offsets[I]); Panel->SetArrayField(TEXT("restMm"),Rest); Panel->SetArrayField(TEXT("triangles"),Faces);
        Panels.Add(MakeShared<FJsonValueObject>(Panel));
    }
    Geometry->SetArrayField(TEXT("initialCm"),Initial); Geometry->SetArrayField(TEXT("settledCm"),Settled); Geometry->SetArrayField(TEXT("panels"),Panels);
    FString GeometryJson; FJsonSerializer::Serialize(Geometry,TJsonWriterFactory<>::Create(&GeometryJson));
    FFileHelper::SaveStringToFile(GeometryJson,*(Directory/TEXT("geometry.json")));
    bool Rendered=false;
    if(FParse::Param(*Params,TEXT("Render")) && !Build.InitialCm.IsEmpty())
    {
        const UWorld::InitializationValues Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false);
        UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("LaceGarmentFitWorld"),nullptr,true,ERHIFeatureLevel::Num,&Init);
        FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
        AActor* BodyActor=World->SpawnActor<AActor>();
        auto* BodyComponent=NewObject<USkeletalMeshComponent>(BodyActor); BodyActor->SetRootComponent(BodyComponent);
        BodyComponent->SetSkeletalMesh(Body); BodyComponent->RegisterComponent();
        AActor* GarmentActor=World->SpawnActor<AActor>();
        auto* Garment=NewObject<UDynamicMeshComponent>(GarmentActor); GarmentActor->SetRootComponent(Garment);
        UE::Geometry::FDynamicMesh3 Mesh;
        const auto& Positions=Build.SettledCm.IsEmpty()?Build.InitialCm:Build.SettledCm;
        for(auto P : Positions) Mesh.AppendVertex(P);
        for(int32 I=0;I<Build.Panels.Num();++I) for(auto T : Build.Panels[I].Triangles)
            Mesh.AppendTriangle(Build.Offsets[I]+T.X,Build.Offsets[I]+T.Y,Build.Offsets[I]+T.Z);
        Mesh.EnableVertexNormals(FVector3f::UnitZ()); UE::Geometry::FMeshNormals::QuickComputeVertexNormals(Mesh);
        Garment->SetMesh(MoveTemp(Mesh));
        Garment->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/LaceGarment/Development/M_NativeCloth.M_NativeCloth"))); Garment->RegisterComponent();
        const double Z=Kind==TEXT("TShirt")?8.2:5.9;
        Rendered=CaptureInspectionViews(World,Directory,FVector(0,0,Z),11,{{18,-18,Z+4},{20,0,Z},{-18,18,Z+3}});
        Garment->UnregisterComponent(); BodyComponent->UnregisterComponent();
        GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    }
    Report->SetBoolField(TEXT("rendered"),Rendered);
    FString Json; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
    const bool Saved=FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("fit.json")));
    Document->RemoveFromRoot();
    UE_LOG(LogTemp,Display,TEXT("Template fit %s: %s"),Fitted?TEXT("converged"):TEXT("failed"),*FitError);
    return Saved && Fitted && (!FParse::Param(*Params,TEXT("Render")) || Rendered)?0:7;
}
