#include "BodyPins/BodyPinProbe.h"
#include "ProbeSupport.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothEngineTools.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace GarmentFitClothProbe::BodyPins
{
using namespace UE::Chaos::ClothAsset;

bool WorldTarget(const FResolvedPin& Pin, const FTransform& BodyTransform, FVector& Out, FString& Error)
{
    Error.Reset();
    if (Pin.BodyLocalTarget.ContainsNaN() || BodyTransform.ContainsNaN() ||
        !BodyTransform.GetScale3D().Equals(FVector::OneVector, 1.e-6) || !BodyTransform.GetRotation().IsNormalized())
    { Error = TEXT("Body pin requires finite target and rigid unit-scale body transform"); return false; }
    Out = BodyTransform.TransformPosition(FVector(Pin.BodyLocalTarget));
    return true;
}

bool ResolvePins(const UGarmentProbeBodyPinSet& Set, const FBodySurface& Body,
    const TSharedRef<const FManagedArrayCollection>& Collection, TArray<FResolvedPin>& Out, FString& Error)
{
    Error.Reset(); Out.Reset();
    auto Fail = [&Error](const TCHAR* Reason) { Error = Reason; return false; };
    if (Set.BodyMesh.IsNull() || Body.Mesh.IsNull() || Set.BodyMesh.ToSoftObjectPath() != Body.Mesh ||
        Body.Positions.IsEmpty() || Body.Triangles.IsEmpty())
        return Fail(TEXT("Missing body or surface belongs to a different body asset"));
    TSet<FName> UsedIds;
    TSet<int32> UsedWelded;
    TArray<FResolvedPin> Pending;
    for (const FGarmentProbeBodyPin& Pin : Set.Pins)
    {
        if (Pin.Id.IsNone() || UsedIds.Contains(Pin.Id)) return Fail(TEXT("Missing or duplicate pin ID"));
        if (!Body.Triangles.IsValidIndex(Pin.BodyTriangle) || Pin.Barycentric.ContainsNaN() ||
            !FMath::IsFinite(Pin.SurfaceOffsetCm) || Pin.SurfaceOffsetCm < 1.f ||
            Pin.Barycentric.GetMin() < 0.f || Pin.Barycentric.GetMax() > 1.f ||
            !FMath::IsNearlyEqual(Pin.Barycentric.X + Pin.Barycentric.Y + Pin.Barycentric.Z, 1.f, 1.e-4f))
            return Fail(TEXT("Invalid body triangle/barycentric target or offset below 1 cm collision clearance"));
        const FIntVector3 Triangle = Body.Triangles[Pin.BodyTriangle];
        for (int32 K = 0; K < 3; ++K)
            if (!Body.Positions.IsValidIndex(Triangle[K]) || Body.Positions[Triangle[K]].ContainsNaN())
                return Fail(TEXT("Invalid body triangle geometry"));
        const FVector3f A = Body.Positions[Triangle.X], B = Body.Positions[Triangle.Y], C = Body.Positions[Triangle.Z];
        FVector3f Normal = FVector3f::CrossProduct(B - A, C - A).GetSafeNormal();
        if (Normal.IsNearlyZero()) return Fail(TEXT("Degenerate body triangle"));
        if (Body.TriangleNormals.IsValidIndex(Pin.BodyTriangle))
        {
            const FVector3f ImportedNormal = Body.TriangleNormals[Pin.BodyTriangle];
            if (ImportedNormal.ContainsNaN() || ImportedNormal.IsNearlyZero()) return Fail(TEXT("Invalid body surface normal"));
            if (FVector3f::DotProduct(Normal, ImportedNormal) < 0.f) Normal *= -1.f;
        }
        FResolvedPin Resolved;
        Resolved.Id = Pin.Id; Resolved.bEnabled = Pin.bEnabled;
        Resolved.BodyLocalNormal = Normal;
        Resolved.BodyLocalTarget = A * Pin.Barycentric.X + B * Pin.Barycentric.Y + C * Pin.Barycentric.Z + Normal * Pin.SurfaceOffsetCm;
        if (!Support::ResolveVertex(Collection, {Pin.PanelIndex, Pin.VertexIndex}, Resolved.WeldedVertex, Error)) return false;
        if (UsedWelded.Contains(Resolved.WeldedVertex)) return Fail(TEXT("Duplicate or conflicting pin on a welded cloth particle (including disabled pins)"));
        for (const FSphere& Sphere : Body.CollisionSpheres)
            if (!FMath::IsFinite(Sphere.W) || Sphere.W <= 0 || Sphere.Center.ContainsNaN() ||
                FVector::Distance(FVector(Resolved.BodyLocalTarget), Sphere.Center) < Sphere.W + 1.f - 1.e-4f)
                return Fail(TEXT("Body pin target conflicts with an active collision proxy or its 1 cm clearance"));
        FVector Ignored;
        if (!WorldTarget(Resolved, Set.BodyTransform, Ignored, Error)) return false;
        UsedIds.Add(Pin.Id); UsedWelded.Add(Resolved.WeldedVertex); Pending.Add(Resolved);
    }
    Out = MoveTemp(Pending);
    return true;
}

bool CreatePin(UGarmentProbeBodyPinSet& Set, const FGarmentProbeBodyPin& Pin, const FBodySurface& Body,
    const TSharedRef<const FManagedArrayCollection>& Collection, FString& Error)
{
    Set.Pins.Add(Pin);
    TArray<FResolvedPin> Resolved;
    if (!ResolvePins(Set, Body, Collection, Resolved, Error)) { Set.Pins.Pop(); return false; }
    // Creation is authoring: the latest valid current set becomes the reset
    // baseline. Release followed by reauthoring must not retain stale duplicates.
    Set.AuthoredPins = Set.Pins;
    return true;
}

bool SetPinEnabled(UGarmentProbeBodyPinSet& Set, FName Id, bool bEnabled, FString& Error)
{
    Error.Reset();
    for (FGarmentProbeBodyPin& Pin : Set.Pins)
        if (Pin.Id == Id) { Pin.bEnabled = bEnabled; return true; }
    Error = TEXT("Cannot change enabled state of missing pin"); return false;
}

bool ReleasePin(UGarmentProbeBodyPinSet& Set, FName Id, FString& Error)
{
    Error.Reset();
    const int32 Removed = Set.Pins.RemoveAll([Id](const FGarmentProbeBodyPin& Pin) { return Pin.Id == Id; });
    if (Removed == 1) return true;
    Error = TEXT("Cannot release missing pin"); return false;
}

void ResetPins(UGarmentProbeBodyPinSet& Set) { Set.Pins = Set.AuthoredPins; }

bool ValidateSavedLifecycle(const UGarmentProbeBodyPinSet& Authored, const UGarmentProbeBodyPinSet& Disabled,
    const UGarmentProbeBodyPinSet& Released, const FBodySurface& Body,
    const TSharedRef<const FManagedArrayCollection>& Collection, FString& Error)
{
    Error.Reset();
    if (Authored.Pins.IsEmpty() || Authored.Pins.Num() != Authored.AuthoredPins.Num() ||
        Disabled.Pins.Num() != Authored.Pins.Num() || !Released.Pins.IsEmpty() ||
        Released.AuthoredPins.Num() != Authored.Pins.Num())
    { Error = TEXT("Saved pin lifecycle mismatch"); return false; }
    auto SameHeader = [&Authored](const UGarmentProbeBodyPinSet& Other)
    {
        return Other.BodyMesh.ToSoftObjectPath() == Authored.BodyMesh.ToSoftObjectPath() &&
            Other.BodyTransform.Equals(Authored.BodyTransform, 1.e-6) &&
            Other.SourceCommit == Authored.SourceCommit && Other.SourceBlob == Authored.SourceBlob && Other.SnapshotUtc == Authored.SnapshotUtc;
    };
    auto SamePins = [](TConstArrayView<FGarmentProbeBodyPin> Expected, TConstArrayView<FGarmentProbeBodyPin> Other, bool bEnabled)
    {
        if (Expected.Num() != Other.Num()) return false;
        for (int32 I = 0; I < Expected.Num(); ++I)
        {
            const FGarmentProbeBodyPin& A = Expected[I]; const FGarmentProbeBodyPin& B = Other[I];
            if (A.Id != B.Id || A.PanelIndex != B.PanelIndex || A.VertexIndex != B.VertexIndex ||
                A.BodyTriangle != B.BodyTriangle || A.Barycentric != B.Barycentric || A.SurfaceOffsetCm != B.SurfaceOffsetCm || B.bEnabled != bEnabled)
                return false;
        }
        return true;
    };
    if (!SameHeader(Disabled) || !SameHeader(Released) ||
        !SamePins(Authored.Pins, Authored.AuthoredPins, true) || !SamePins(Authored.Pins, Authored.Pins, true) ||
        !SamePins(Authored.Pins, Disabled.AuthoredPins, true) || !SamePins(Authored.Pins, Disabled.Pins, false) ||
        !SamePins(Authored.Pins, Released.AuthoredPins, true))
    { Error = TEXT("Saved lifecycle document has changed body, target identity, target coordinates, state, or reset baseline"); return false; }
    TArray<FResolvedPin> Expected;
    if (!ResolvePins(Authored, Body, Collection, Expected, Error)) return false;
    for (const UGarmentProbeBodyPinSet* State : {&Authored, &Disabled, &Released})
    {
        TArray<FResolvedPin> Current;
        if (!ResolvePins(*State, Body, Collection, Current, Error)) return false;
        UGarmentProbeBodyPinSet* ResetState = NewObject<UGarmentProbeBodyPinSet>();
        ResetState->BodyMesh = State->BodyMesh; ResetState->BodyTransform = State->BodyTransform;
        ResetState->Pins = State->Pins; ResetState->AuthoredPins = State->AuthoredPins;
        ResetPins(*ResetState);
        TArray<FResolvedPin> Restored;
        if (!ResolvePins(*ResetState, Body, Collection, Restored, Error)) return false;
        if (Restored.Num() != Expected.Num()) { Error = TEXT("Reset changed resolved pin count"); return false; }
        for (int32 I = 0; I < Expected.Num(); ++I)
            if (Restored[I].Id != Expected[I].Id || Restored[I].WeldedVertex != Expected[I].WeldedVertex ||
                Restored[I].BodyLocalTarget != Expected[I].BodyLocalTarget || !Restored[I].bEnabled)
            { Error = TEXT("Reset does not restore original resolved body targets"); return false; }
    }
    return true;
}

UChaosClothAsset* BuildPinAsset(UChaosClothAsset* Input, const UGarmentProbeBodyPinSet& Set,
    const FBodySurface& Body, const FString& PackageName, FString& Error)
{
    if (!Input || static_cast<const UChaosClothAsset*>(Input)->GetClothCollections().IsEmpty() || !PackageName.StartsWith(TEXT("/GarmentFitClothProbe/BodyPins/")))
    { Error = TEXT("Missing cloth source or package outside BodyPins ownership"); return nullptr; }
    TArray<FResolvedPin> Resolved;
    const TSharedRef<const FManagedArrayCollection> Source = static_cast<const UChaosClothAsset*>(Input)->GetClothCollections()[0];
    if (!ResolvePins(Set, Body, Source, Resolved, Error)) return nullptr;
    TSharedRef<FManagedArrayCollection> Copy = MakeShared<FManagedArrayCollection>(*Source);
    FCollectionClothFacade Cloth(Copy);
    Cloth.AddWeightMap(TEXT("MaxDistance"));
    for (float& Weight : Cloth.GetWeightMap(TEXT("MaxDistance"))) Weight = 2000.f;
    for (const FResolvedPin& Pin : Resolved)
        if (Pin.bEnabled)
        {
            Cloth.GetSimPosition3D()[Pin.WeldedVertex] = Pin.BodyLocalTarget;
            Cloth.GetWeightMap(TEXT("MaxDistance"))[Pin.WeldedVertex] = 0.f;
        }
    FClothGeometryTools::BindMeshToRootBone(Copy, true, false);
    FClothEngineTools::GenerateTethers(Copy, TEXT("MaxDistance"), true);
    FClothGeometryTools::CopySimMeshToRenderMesh(Copy,
        FSoftObjectPath(TEXT("/GarmentFitClothProbe/BodyPins/M_PinCloth.M_PinCloth")), false);
    return Support::BuildNativeAsset(Copy, PackageName, Error);
}
}
