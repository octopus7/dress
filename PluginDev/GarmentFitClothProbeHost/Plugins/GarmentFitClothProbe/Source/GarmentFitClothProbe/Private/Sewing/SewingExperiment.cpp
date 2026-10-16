#include "Sewing/SewingProbe.h"
#include "ProbeSupport.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSimPatternFacade.h"
#include "ChaosClothAsset/CollectionClothSeamFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "PreviewScene.h"
#include "DrawDebugHelpers.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Algo/Reverse.h"

namespace GarmentFitClothProbe::Sewing
{
namespace
{
using namespace UE::Chaos::ClothAsset;
using ::Chaos::Softs::FCollectionPropertyMutableFacade;
using ::Chaos::Softs::FCollectionPropertyConstFacade;
constexpr float StepSeconds = 1.f / 60.f;
constexpr int32 StepCount = 600;
const FString AssetRoot = TEXT("/GarmentFitClothProbe/Sewing/");

struct FCase
{
    FString Name;
    double LengthA = 100.;
    double LengthB = 100.;
    bool bNonuniform = false;
    bool bReversed = false;
    bool bUse3DRest = false;
};

FInput MakeFixture(const FCase& Case, FRequest& Request)
{
    FInput Input;
    Input.Panels.SetNum(2);
    Request.A.PanelIndex = 0; Request.B.PanelIndex = 1;
    Request.LengthPolicy = Case.LengthA == Case.LengthB ? ELengthPolicy::EqualOnly : ELengthPolicy::NormalizedArcLength;
    for (int32 P = 0; P < 2; ++P)
    {
        TArray<double> Rows;
        if (P == 1 && Case.bNonuniform) Rows = {0., .05, .15, .3, .45, .65, .8, .9, 1.};
        else for (int32 I = 0; I <= 10; ++I) Rows.Add(double(I) / 10.);
        FPanel& Panel = Input.Panels[P];
        for (const double Fraction : Rows)
            for (int32 X = 0; X <= 8; ++X)
            {
                Panel.Source2D.Emplace(X * 5.f, float(Fraction * (P == 0 ? Case.LengthA : Case.LengthB)));
                // All cases deliberately share a 100cm initial 3D span. This is
                // placement, not rest geometry: Source2D retains 100 or 150cm.
                const double Placement = P == 1 && Case.bReversed ? 1. - Fraction : Fraction;
                Panel.Initial3D.Emplace(X * 5.f + (P == 0 ? -40.f : 0.f), float(Placement * 100.), 120.f);
            }
        for (int32 Y = 0; Y + 1 < Rows.Num(); ++Y)
            for (int32 X = 0; X < 8; ++X)
            {
                const int32 I = Y * 9 + X;
                Panel.Triangles.Emplace(I, I + 1, I + 10);
                Panel.Triangles.Emplace(I, I + 10, I + 9);
            }
        TArray<int32>& Boundary = P == 0 ? Request.A.Vertices : Request.B.Vertices;
        for (int32 Y = 0; Y < Rows.Num(); ++Y) Boundary.Add(Y * 9 + (P == 0 ? 8 : 0));
    }
    if (Case.bReversed) Algo::Reverse(Request.B.Vertices);
    return Input;
}

void ConfigureNative2D(const TSharedRef<FManagedArrayCollection>& Collection, const FCase& Case, const FPreparedSeam& Prepared)
{
    Support::ApplyDefaultConfig(Collection);
    FCollectionPropertyMutableFacade Properties(Collection);
    // Default edge/area springs and tethers derive rest lengths from the draped
    // 3D mesh. Disable them so they do not silently dominate the 2D metric.
    for (const TCHAR* Name : {TEXT("EdgeSpringStiffness"), TEXT("AreaSpringStiffness"), TEXT("TetherStiffness"),
        TEXT("XPBDEdgeSpringStiffness"), TEXT("XPBDAreaSpringStiffness"), TEXT("XPBDAnisoSpringStiffnessWarp"),
        TEXT("BendingSpringStiffness")})
        Properties.SetEnabled(FName(Name), false);
    Properties.AddValue(FName(TEXT("XPBDAnisoStretchUse3dRestLengths")), Case.bUse3DRest);
    Properties.AddValue(FName(TEXT("XPBDAnisoStretchStiffnessWarp")), 10000.f);
    Properties.AddValue(FName(TEXT("XPBDAnisoStretchStiffnessWeft")), 10000.f);
    Properties.AddValue(FName(TEXT("XPBDAnisoStretchStiffnessBias")), 10000.f);
    Properties.AddValue(FName(TEXT("XPBDAnisoStretchDamping")), 1.f);
    Properties.AddValue(FName(TEXT("XPBDAnisoStretchWarpScale")), 1.f);
    Properties.AddValue(FName(TEXT("XPBDAnisoStretchWeftScale")), 1.f);
    // Bending elements use angle, not 3D edge rest lengths.
    Properties.AddValue(FName(TEXT("XPBDBendingElementStiffness")), 0.1f);
    Properties.AddValue(FName(TEXT("XPBDBendingElementDamping")), 1.f);
    Properties.SetValue(FName(TEXT("DampingCoefficient")), 0.1f);
    Properties.AddValue(FName(TEXT("SewingSourceLengthA")), float(Prepared.OriginalLengthA));
    Properties.AddValue(FName(TEXT("SewingSourceLengthB")), float(Prepared.OriginalLengthB));
    Properties.AddValue(FName(TEXT("SewingNormalizedArcLength")), Case.LengthA != Case.LengthB);

    FCollectionClothFacade Cloth(Collection);
    Cloth.AddWeightMap(TEXT("MaxDistance"));
    auto Weights = Cloth.GetWeightMap(TEXT("MaxDistance"));
    for (int32 I = 0; I < Weights.Num(); ++I)
        Weights[I] = Cloth.GetSimPosition3D()[I].Y <= 1.e-4f ? 0.f : 300.f;
    FClothGeometryTools::CopySimMeshToRenderMesh(Collection, FSoftObjectPath(FClothEngineTools::GetSimPreviewMaterial()), false);
}

bool ValidateSaved(UChaosClothAsset* Asset, const FCase& Case, const FInput& Source,
    const FPreparedSeam& Prepared, FString& Error)
{
    if (!Asset || !Asset->HasValidClothSimulationModels() || Asset->GetPhysicsAsset())
    { Error = TEXT("Saved sewing cloth missing, invalid, or unexpectedly has a collider"); return false; }
    const auto& Collections = static_cast<const UChaosClothAsset*>(Asset)->GetClothCollections();
    if (Collections.Num() != 1) { Error = TEXT("Sewing fixture must have one LOD"); return false; }
    const FCollectionClothConstFacade Cloth(Collections[0]);
    if (Cloth.GetNumSimPatterns() != 2 || Cloth.GetNumSeams() != 1 || Cloth.GetSeam(0).GetNumSeamStitches() != Prepared.Input.Seam.Num())
    { Error = TEXT("Saved sewing topology differs from prepared source"); return false; }
    const FCollectionPropertyConstFacade Properties(Collections[0]);
    if (!Properties.IsEnabled(FName(TEXT("XPBDAnisoStretchStiffnessWarp"))) ||
        Properties.GetValue<bool>(FName(TEXT("XPBDAnisoStretchUse3dRestLengths")), !Case.bUse3DRest) != Case.bUse3DRest ||
        Properties.IsEnabled(FName(TEXT("EdgeSpringStiffness"))) || Properties.IsEnabled(FName(TEXT("AreaSpringStiffness"))) ||
        Properties.IsEnabled(FName(TEXT("TetherStiffness"))) ||
        !FMath::IsNearlyEqual(Properties.GetValue<float>(FName(TEXT("SewingSourceLengthA"))), float(Prepared.OriginalLengthA)) ||
        !FMath::IsNearlyEqual(Properties.GetValue<float>(FName(TEXT("SewingSourceLengthB"))), float(Prepared.OriginalLengthB)))
    { Error = TEXT("Saved sewing configuration lost its explicit rest metric or source lengths"); return false; }
    const auto Lookup = Cloth.GetSimVertex3DLookup();
    const auto Rest = Cloth.GetSimPosition3D();
    int32 ExpectedVertexCount = -Prepared.Input.Seam.Num();
    for (int32 P = 0; P < 2; ++P)
    {
        const auto Pattern = Cloth.GetSimPattern(P);
        const auto Positions2D = Pattern.GetSimPosition2D();
        const FPanel& Expected = Prepared.Input.Panels[P];
        ExpectedVertexCount += Expected.Source2D.Num();
        if (Positions2D.Num() != Expected.Source2D.Num() || Pattern.GetNumSimFaces() != Expected.Triangles.Num())
        { Error = TEXT("Saved refined panel counts changed"); return false; }
        for (int32 I = 0; I < Positions2D.Num(); ++I)
        {
            if (!Positions2D[I].Equals(Expected.Source2D[I], 1.e-5f) ||
                !Rest[Lookup[Pattern.GetSimVertices2DOffset() + I]].Equals(Expected.Initial3D[I], 1.e-4f))
            { Error = TEXT("Saved source2D or explicit prepared3D changed"); return false; }
            if (I < Source.Panels[P].Source2D.Num() && Positions2D[I] != Source.Panels[P].Source2D[I])
            { Error = TEXT("Original 2D vertex value/index was not preserved"); return false; }
        }
        const auto Faces = Pattern.GetSimIndices2D();
        for (int32 I = 0; I < Faces.Num(); ++I)
            for (int32 K = 0; K < 3; ++K)
                if (Faces[I][K] != Expected.Triangles[I][K] + Pattern.GetSimVertices2DOffset())
                { Error = TEXT("Saved triangle refinement changed"); return false; }
    }
    if (Rest.Num() != ExpectedVertexCount) { Error = TEXT("Unexpected native weld vertex count"); return false; }
    const auto Stitches = Cloth.GetSeam(0).GetSeamStitch2DEndIndices();
    const int32 OffsetB = Cloth.GetSimPattern(1).GetSimVertices2DOffset();
    for (int32 I = 0; I < Stitches.Num(); ++I)
    {
        if (Stitches[I].X != Prepared.Input.Seam[I].X || Stitches[I].Y != Prepared.Input.Seam[I].Y + OffsetB ||
            Lookup[Stitches[I].X] != Lookup[Stitches[I].Y])
        { Error = TEXT("Native correspondence is not the requested seam weld"); return false; }
    }
    return true;
}

UChaosClothAsset* EnsureAsset(const FRunContext& Context, const FCase& Case, const FInput& Source,
    const FPreparedSeam& Prepared, FString& Error)
{
    const FString Name = TEXT("CA_") + Case.Name;
    UChaosClothAsset* Asset = nullptr;
    if (Context.bReopen)
    {
        Asset = LoadObject<UChaosClothAsset>(nullptr, *(AssetRoot + Name + TEXT(".") + Name));
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    else
    {
        const TSharedPtr<FManagedArrayCollection> Collection = Support::BuildPanelCollection(Prepared.Input, Error);
        if (!Collection) return nullptr;
        ConfigureNative2D(Collection.ToSharedRef(), Case, Prepared);
        Asset = Support::BuildNativeAsset(Collection.ToSharedRef(), AssetRoot + Name, Error);
    }
    return ValidateSaved(Asset, Case, Source, Prepared, Error) ? Asset : nullptr;
}

TArray<TSharedPtr<FJsonValue>> JsonPositions(TConstArrayView<FVector3f> Positions)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FVector3f& P : Positions)
        Result.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y), MakeShared<FJsonValueNumber>(P.Z)}));
    return Result;
}
bool SaveJson(const TSharedRef<FJsonObject>& Object, const FString& Filename, FString& Error)
{
    FString Text;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
    if (!FFileHelper::SaveStringToFile(Text, *Filename)) { Error = TEXT("Cannot save sewing evidence: ") + Filename; return false; }
    return true;
}

struct FMetrics
{
    TArray<FVector3f> Initial;
    TArray<FVector3f> Final;
    float MaxPinError = 0, MaxStepMotion = 0, LastSecondMaxMotion = 0, ResetError = 0, ReplayError = 0;
    double SeamLength = 0, InitialSeamLength = 0, MaxSeamGap = 0;
    double MinSegmentRatio[2] = {TNumericLimits<double>::Max(), TNumericLimits<double>::Max()};
    double MaxSegmentRatio[2] = {0., 0.};
    double MaxPanelEdgeStrain[2] = {0., 0.};
};

double SeamLength(const FCollectionClothConstFacade& Cloth, TConstArrayView<FVector3f> Positions)
{
    const auto Vertices = Cloth.GetSeam(0).GetSeamStitch3DIndex();
    double Length = 0;
    for (int32 I = 1; I < Vertices.Num(); ++I) Length += (Positions[Vertices[I]] - Positions[Vertices[I - 1]]).Size();
    return Length;
}

bool Capture(FPreviewScene& Scene, UChaosClothComponent* Component, const FCollectionClothConstFacade& Cloth,
    TConstArrayView<FVector3f> Positions, const FString& Filename, FString& Error)
{
    if (!Support::EnsureClothRenderReady(Component, Error)) return false;
    FlushPersistentDebugLines(Scene.GetWorld());
    Component->MarkRenderDynamicDataDirty();
    if (Filename.EndsWith(TEXT("-10s.png")))
    {
        const FBoxSphereBounds Bounds = Component->GetBounds();
        const UMaterialInterface* Material = Component->GetMaterial(0);
        UE_LOG(LogTemp, Display, TEXT("Sewing render %s: lod=%d materials=%d material0=%s boundsOrigin=%s extent=%s radius=%.5f"),
            *FPaths::GetBaseFilename(Filename), Component->GetPredictedLODLevel(), Component->GetNumMaterials(),
            Material ? *Material->GetPathName() : TEXT("null"), *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString(), Bounds.SphereRadius);
        // Evidence of the native rendered surface alone; the following standard
        // capture also overlays measured solver topology, seam and fixed points.
        const FString SurfaceFile = Filename.LeftChop(4) + TEXT("-surface.png");
        if (!Support::CapturePreview(Scene, SurfaceFile, FVector(200, 230, 200), FVector(0, 40, 65), Error)) return false;
    }
    for (int32 P = 0; P < 2; ++P)
    {
        const auto Pattern = Cloth.GetSimPattern(P);
        for (const FIntVector3& T : Pattern.GetSimIndices3D())
            for (int32 K = 0; K < 3; ++K)
                DrawDebugLine(Scene.GetWorld(), FVector(Positions[T[K]]), FVector(Positions[T[(K + 1) % 3]]),
                    P == 0 ? FColor(30, 190, 230) : FColor(245, 155, 45), true, -1.f, 0, .3f);
    }
    const auto Seam = Cloth.GetSeam(0).GetSeamStitch3DIndex();
    for (int32 I = 1; I < Seam.Num(); ++I)
        DrawDebugLine(Scene.GetWorld(), FVector(Positions[Seam[I - 1]]), FVector(Positions[Seam[I]]), FColor::Yellow, true, -1.f, 0, 1.2f);
    const auto Weights = Cloth.GetWeightMap(TEXT("MaxDistance"));
    for (int32 I = 0; I < Positions.Num(); ++I)
        if (Weights[I] == 0.f) DrawDebugPoint(Scene.GetWorld(), FVector(Positions[I]), 8.f, FColor::Green, true);
    Component->MarkRenderDynamicDataDirty();
    return Support::CapturePreview(Scene, Filename, FVector(200, 230, 200), FVector(0, 40, 65), Error);
}

void MeasureStrain(const FCollectionClothConstFacade& Cloth, const FPreparedSeam& Prepared,
    TConstArrayView<FVector3f> Current, FMetrics& Metrics)
{
    const auto Stitches = Cloth.GetSeam(0).GetSeamStitch2DEndIndices();
    const auto Lookup = Cloth.GetSimVertex3DLookup();
    const auto Source2D = Cloth.GetSimPosition2D();
    Metrics.SeamLength = SeamLength(Cloth, Current);
    for (int32 I = 0; I < Stitches.Num(); ++I)
    {
        Metrics.MaxSeamGap = FMath::Max(Metrics.MaxSeamGap, double((Current[Lookup[Stitches[I].X]] - Current[Lookup[Stitches[I].Y]]).Size()));
        if (I == 0) continue;
        for (int32 P = 0; P < 2; ++P)
        {
            const int32 A = Stitches[I - 1][P], B = Stitches[I][P];
            const double Ratio = (Current[Lookup[A]] - Current[Lookup[B]]).Size() / double((Source2D[A] - Source2D[B]).Size());
            Metrics.MinSegmentRatio[P] = FMath::Min(Metrics.MinSegmentRatio[P], Ratio);
            Metrics.MaxSegmentRatio[P] = FMath::Max(Metrics.MaxSegmentRatio[P], Ratio);
        }
    }
    for (int32 P = 0; P < 2; ++P)
        for (const FIntVector3& T : Cloth.GetSimPattern(P).GetSimIndices2D())
            for (int32 K = 0; K < 3; ++K)
            {
                const int32 A = T[K], B = T[(K + 1) % 3];
                const double Ratio = (Current[Lookup[A]] - Current[Lookup[B]]).Size() / double((Source2D[A] - Source2D[B]).Size());
                Metrics.MaxPanelEdgeStrain[P] = FMath::Max(Metrics.MaxPanelEdgeStrain[P], FMath::Abs(Ratio - 1.));
            }
}

bool Simulate(UChaosClothAsset* Asset, const FCase& Case, const FInput& Source, const FPreparedSeam& Prepared,
    const FRunContext& Context, FMetrics& Metrics, FString& Error)
{
    const FCollectionClothConstFacade Cloth(static_cast<const UChaosClothAsset*>(Asset)->GetClothCollections()[0]);
    const TArray<FVector3f> Initial(Cloth.GetSimPosition3D());
    Metrics.Initial = Initial;
    const auto Weights = Cloth.GetWeightMap(TEXT("MaxDistance"));
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetTransactional(false));
    UChaosClothComponent* Component = NewObject<UChaosClothComponent>();
    Component->SetAsset(Asset);
    Component->SetSimulateInEditor(true);
    Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Component->SetForcedLOD(1);
    Scene.AddComponent(Component, FTransform::Identity);
    FAssetCompilingManager::Get().FinishAllCompilation();
    GShaderCompilingManager->FinishAllCompilation();
    if (!static_cast<USceneComponent*>(Component)->IsVisible()) { Error = TEXT("Native sewing cloth surface is hidden"); return false; }
    Component->MarkRenderStateDirty();
    Scene.GetWorld()->SendAllEndOfFrameUpdates();
    FlushRenderingCommands();
    Support::TickCloth(Component, 0.f);
    TArray<FVector3f> Current, Previous;
    if (!Support::ReadNativePositions(Component, Current, Error) || Current.Num() != Initial.Num())
    { if (Error.IsEmpty()) Error = TEXT("Native vertex count differs from source"); return false; }
    for (int32 I = 0; I < Initial.Num(); ++I)
        if (!Current[I].Equals(Initial[I], 1.e-3f)) { Error = TEXT("Initial native state differs from explicit prepared3D"); return false; }
    const FString Prefix = Context.OutputDirectory / Case.Name;
    if (!Capture(Scene, Component, Cloth, Current, Prefix + TEXT("-initial.png"), Error)) return false;
    Metrics.InitialSeamLength = SeamLength(Cloth, Current);
    Previous = Current;
    for (int32 Step = 0; Step < StepCount; ++Step)
    {
        Support::TickCloth(Component, StepSeconds);
        if (!Support::ReadNativePositions(Component, Current, Error) || Current.Num() != Initial.Num()) return false;
        for (int32 I = 0; I < Current.Num(); ++I)
        {
            if (Current[I].Size() > 2000.f) { Error = TEXT("Runaway sewing simulation"); return false; }
            const float Motion = (Current[I] - Previous[I]).Size();
            Metrics.MaxStepMotion = FMath::Max(Metrics.MaxStepMotion, Motion);
            if (Step >= 540) Metrics.LastSecondMaxMotion = FMath::Max(Metrics.LastSecondMaxMotion, Motion);
            if (Weights[I] == 0.f) Metrics.MaxPinError = FMath::Max(Metrics.MaxPinError, (Current[I] - Initial[I]).Size());
        }
        Previous = Current;
        if (Step == 59 && !Capture(Scene, Component, Cloth, Current, Prefix + TEXT("-1s.png"), Error)) return false;
    }
    Metrics.Final = Current;
    MeasureStrain(Cloth, Prepared, Current, Metrics);
    if (!Capture(Scene, Component, Cloth, Current, Prefix + TEXT("-10s.png"), Error)) return false;
    const FClothSimulationProxy* Proxy = Component->GetClothSimulationProxy();
    const int32 KinematicCount = Proxy->GetNumKinematicParticles();
    const int32 DynamicCount = Proxy->GetNumDynamicParticles();
    int32 ExpectedKinematicCount = 0;
    for (float Weight : Weights) ExpectedKinematicCount += Weight == 0.f ? 1 : 0;

    Component->RecreateClothSimulationProxy();
    Support::TickCloth(Component, 0.f);
    if (!Support::ReadNativePositions(Component, Current, Error) || Current.Num() != Initial.Num()) return false;
    for (int32 I = 0; I < Initial.Num(); ++I) Metrics.ResetError = FMath::Max(Metrics.ResetError, (Current[I] - Initial[I]).Size());
    if (!Capture(Scene, Component, Cloth, Current, Prefix + TEXT("-reset.png"), Error)) return false;
    for (int32 Step = 0; Step < StepCount; ++Step)
    {
        Support::TickCloth(Component, StepSeconds);
        if (!Support::ReadNativePositions(Component, Current, Error) || Current.Num() != Initial.Num()) return false;
        for (const FVector3f& P : Current)
            if (P.Size() > 2000.f) { Error = TEXT("Runaway sewing reset replay"); return false; }
    }
    for (int32 I = 0; I < Initial.Num(); ++I) Metrics.ReplayError = FMath::Max(Metrics.ReplayError, (Current[I] - Metrics.Final[I]).Size());
    const bool bSourcePreserved = ValidateSaved(Asset, Case, Source, Prepared, Error);
    const bool bThresholds = bSourcePreserved && KinematicCount == ExpectedKinematicCount && DynamicCount == Initial.Num() - ExpectedKinematicCount &&
        Metrics.MaxPinError <= .01f && Metrics.MaxStepMotion <= 30.f && Metrics.LastSecondMaxMotion <= 1.f &&
        Metrics.ResetError <= 1.e-3f && Metrics.ReplayError <= .05f && Metrics.MaxSeamGap <= 1.e-4 &&
        Metrics.SeamLength > 50. && Metrics.SeamLength < 200.;

    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("case"), Case.Name);
    Json->SetStringField(TEXT("policy"), Case.LengthA == Case.LengthB ? TEXT("EqualOnly") : TEXT("NormalizedArcLength"));
    Json->SetStringField(TEXT("native_constraint"), TEXT("FXPBDStretchBiasElementConstraints"));
    Json->SetBoolField(TEXT("use_3d_rest_lengths"), Case.bUse3DRest);
    Json->SetBoolField(TEXT("reopened_existing_asset"), Context.bReopen);
    Json->SetBoolField(TEXT("source_preserved"), bSourcePreserved);
    Json->SetBoolField(TEXT("thresholds_passed"), bThresholds);
    Json->SetNumberField(TEXT("steps"), StepCount);
    Json->SetNumberField(TEXT("dt_seconds"), StepSeconds);
    Json->SetNumberField(TEXT("source_length_a_cm"), Prepared.OriginalLengthA);
    Json->SetNumberField(TEXT("source_length_b_cm"), Prepared.OriginalLengthB);
    Json->SetNumberField(TEXT("source_difference_b_minus_a_cm"), Prepared.LengthDifferenceBMinusA);
    Json->SetNumberField(TEXT("source_ratio_b_over_a"), Prepared.LengthRatioBOverA);
    Json->SetNumberField(TEXT("max_explicit_preparation_displacement_cm"), Prepared.MaxPreparationDisplacementCm);
    Json->SetNumberField(TEXT("initial_seam_length_cm"), Metrics.InitialSeamLength);
    Json->SetNumberField(TEXT("result_seam_length_cm"), Metrics.SeamLength);
    Json->SetNumberField(TEXT("seam_strain_a"), Metrics.SeamLength / Prepared.OriginalLengthA - 1.);
    Json->SetNumberField(TEXT("seam_strain_b"), Metrics.SeamLength / Prepared.OriginalLengthB - 1.);
    Json->SetNumberField(TEXT("max_seam_gap_cm"), Metrics.MaxSeamGap);
    for (int32 P = 0; P < 2; ++P)
    {
        const FString Suffix = P == 0 ? TEXT("_a") : TEXT("_b");
        Json->SetNumberField(TEXT("min_seam_segment_ratio") + Suffix, Metrics.MinSegmentRatio[P]);
        Json->SetNumberField(TEXT("max_seam_segment_ratio") + Suffix, Metrics.MaxSegmentRatio[P]);
        Json->SetNumberField(TEXT("max_abs_panel_edge_strain") + Suffix, Metrics.MaxPanelEdgeStrain[P]);
        Json->SetNumberField(TEXT("original_vertex_count") + Suffix, Source.Panels[P].Source2D.Num());
        Json->SetNumberField(TEXT("refined_vertex_count") + Suffix, Prepared.Input.Panels[P].Source2D.Num());
    }
    Json->SetNumberField(TEXT("kinematic_particles"), KinematicCount);
    Json->SetNumberField(TEXT("dynamic_particles"), DynamicCount);
    Json->SetNumberField(TEXT("seam_stitches"), Prepared.Input.Seam.Num());
    Json->SetNumberField(TEXT("max_pin_error_cm"), Metrics.MaxPinError);
    Json->SetNumberField(TEXT("max_step_motion_cm"), Metrics.MaxStepMotion);
    Json->SetNumberField(TEXT("last_second_max_step_motion_cm"), Metrics.LastSecondMaxMotion);
    Json->SetNumberField(TEXT("hard_reset_error_cm"), Metrics.ResetError);
    Json->SetNumberField(TEXT("replay_max_difference_cm"), Metrics.ReplayError);
    Json->SetArrayField(TEXT("initial_positions"), JsonPositions(Initial));
    Json->SetArrayField(TEXT("final_positions"), JsonPositions(Metrics.Final));
    Json->SetStringField(TEXT("scope"), TEXT("Two-panel explicit normalized-arc weld; vertex strain and sampled convergence. No body/self collision or general gathering quality claim."));
    if (!SaveJson(Json, Prefix + TEXT("-metrics.json"), Error)) return false;
    UE_LOG(LogTemp, Display, TEXT("Sewing %s: source=%.4f/%.4f result=%.4f strain=%.5f/%.5f pin=%.6f motion=%.6f last=%.6f reset=%.6f replay=%.6f metric3d=%d pass=%d"),
        *Case.Name, Prepared.OriginalLengthA, Prepared.OriginalLengthB, Metrics.SeamLength,
        Metrics.SeamLength / Prepared.OriginalLengthA - 1., Metrics.SeamLength / Prepared.OriginalLengthB - 1.,
        Metrics.MaxPinError, Metrics.MaxStepMotion, Metrics.LastSecondMaxMotion, Metrics.ResetError, Metrics.ReplayError, Case.bUse3DRest, bThresholds);
    if (!bThresholds)
    { if (Error.IsEmpty()) Error = TEXT("Native sewing stability/pin/reset thresholds failed for ") + Case.Name + TEXT("; see saved metrics"); return false; }
    return true;
}

double MaxDifference(const TArray<FVector3f>& A, const TArray<FVector3f>& B)
{
    if (A.Num() != B.Num()) return -1.;
    double Difference = 0.;
    for (int32 I = 0; I < A.Num(); ++I) Difference = FMath::Max(Difference, double((A[I] - B[I]).Size()));
    return Difference;
}
}

UChaosClothAsset* CreateIntegrationAsset(const FRunContext& Context, FString& Error)
{
    const FCase Case{TEXT("IntegrationEqual")};
    FRequest Request;
    const FInput Source = MakeFixture(Case, Request);
    FPreparedSeam Prepared;
    if (!PrepareSeam(Source, Request, Prepared, Error)) return nullptr;
    return EnsureAsset(Context, Case, Source, Prepared, Error);
}

bool RunExperiment(const FRunContext& Context, FString& Error)
{
    if (FPaths::IsRelative(Context.OutputDirectory) || !FPaths::IsUnderDirectory(Context.OutputDirectory,
        FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Probe"))))
    { Error = TEXT("Sewing output must stay under this independent host Saved/Probe"); return false; }
    IFileManager::Get().MakeDirectory(*Context.OutputDirectory, true);
    const TArray<FCase> Cases = {
        {TEXT("Equal100"), 100., 100.},
        {TEXT("Nonuniform100"), 100., 100., true},
        {TEXT("Reversed100"), 100., 100., false, true},
        {TEXT("Ratio100_150"), 100., 150.},
        {TEXT("Ratio150_100"), 150., 100.},
        {TEXT("Ratio100_150_Rest3D"), 100., 150., false, false, true}
    };
    TArray<FMetrics> Metrics;
    for (const FCase& Case : Cases)
    {
        FRequest Request;
        const FInput Source = MakeFixture(Case, Request);
        FPreparedSeam Prepared;
        if (!PrepareSeam(Source, Request, Prepared, Error)) { Error = Case.Name + TEXT(": ") + Error; return false; }
        UChaosClothAsset* Asset = EnsureAsset(Context, Case, Source, Prepared, Error);
        if (!Asset || !Simulate(Asset, Case, Source, Prepared, Context, Metrics.AddDefaulted_GetRef(), Error)) return false;
    }
    // Equal100, both ratios and Rest3D have exactly the same initial3D/topology.
    // Their changing 2D coordinates must alter the solver, not only metadata.
    const double RatioForward = MaxDifference(Metrics[0].Final, Metrics[3].Final);
    const double RatioBackward = MaxDifference(Metrics[0].Final, Metrics[4].Final);
    const double MetricControl = MaxDifference(Metrics[3].Final, Metrics[5].Final);
    const double InitialForward = MaxDifference(Metrics[0].Initial, Metrics[3].Initial);
    const double InitialBackward = MaxDifference(Metrics[0].Initial, Metrics[4].Initial);
    const double InitialMetricControl = MaxDifference(Metrics[3].Initial, Metrics[5].Initial);
    const double ForwardLengthChange = FMath::Abs(Metrics[3].SeamLength - Metrics[0].SeamLength);
    const double BackwardLengthChange = FMath::Abs(Metrics[4].SeamLength - Metrics[0].SeamLength);
    const bool bCausal = InitialForward == 0. && InitialBackward == 0. && InitialMetricControl == 0. &&
        RatioForward > 1. && RatioBackward > 1. && MetricControl > 1. && ForwardLengthChange > 1. && BackwardLengthChange > 1.;
    TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
    Summary->SetBoolField(TEXT("reopened_existing_assets"), Context.bReopen);
    Summary->SetBoolField(TEXT("two_d_rest_metric_effect_verified"), bCausal);
    Summary->SetNumberField(TEXT("ratio100_150_vs_equal_max_difference_cm"), RatioForward);
    Summary->SetNumberField(TEXT("ratio150_100_vs_equal_max_difference_cm"), RatioBackward);
    Summary->SetNumberField(TEXT("same_mismatch_2d_vs_3d_rest_max_difference_cm"), MetricControl);
    Summary->SetNumberField(TEXT("equal_vs_forward_initial_difference_cm"), InitialForward);
    Summary->SetNumberField(TEXT("equal_vs_backward_initial_difference_cm"), InitialBackward);
    Summary->SetNumberField(TEXT("metric_control_initial_difference_cm"), InitialMetricControl);
    Summary->SetNumberField(TEXT("ratio100_150_vs_equal_seam_length_change_cm"), ForwardLengthChange);
    Summary->SetNumberField(TEXT("ratio150_100_vs_equal_seam_length_change_cm"), BackwardLengthChange);
    Summary->SetStringField(TEXT("mismatch_support_scope"), TEXT("Explicit normalized arc-length welding with native per-triangle 2D rest metric; measured compromise strain, not a general ease/gathering quality guarantee."));
    if (!SaveJson(Summary, Context.OutputDirectory / TEXT("summary.json"), Error)) return false;
    if (!bCausal) { Error = TEXT("Preserved 2D mismatch did not materially drive native simulation; mismatch support is unverified"); return false; }
    return true;
}
}
