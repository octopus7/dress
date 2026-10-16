#include "BodyPins/BodyPinProbe.h"
#include "ProbeSupport.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Misc/AutomationTest.h"
#include <limits>

using namespace GarmentFitClothProbe;
using namespace GarmentFitClothProbe::BodyPins;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBodyPinContractTest, "GarmentFitClothProbe.BodyPins.TargetAndLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBodyPinContractTest::RunTest(const FString&)
{
    // Catches world-fixed targets, surface-offset sign errors, missing lifecycle
    // changes, and duplicate detection that uses 2D aliases rather than welded IDs.
    FInput Input;
    Input.Panels = {
        { {{0,0},{2,0},{0,2}}, {{0,0,0},{2,0,0},{0,2,0}}, {{0,1,2}} },
        { {{0,0},{2,0},{2,2}}, {{0,0,0},{2,0,0},{2,-2,0}}, {{0,1,2}} }
    };
    Input.Seam = {{0,0},{1,1}};
    FString Error;
    TSharedPtr<FManagedArrayCollection> Mutable = Support::BuildPanelCollection(Input, Error);
    if (!TestTrue(TEXT("Native welded fixture built"), Mutable.IsValid())) return false;
    TSharedRef<const FManagedArrayCollection> Collection = Mutable.ToSharedRef();
    FBodySurface Body;
    Body.Mesh = FSoftObjectPath(TEXT("/GarmentFitClothProbe/BodyPins/TestBody.TestBody"));
    Body.Positions = {{0,0,10},{6,0,10},{0,6,10}};
    Body.Triangles = {{0,1,2}};
    UGarmentProbeBodyPinSet* Set = NewObject<UGarmentProbeBodyPinSet>();
    Set->BodyMesh = TSoftObjectPtr<UStaticMesh>(Body.Mesh);
    FGarmentProbeBodyPin Pin;
    Pin.Id = TEXT("Shoulder"); Pin.PanelIndex = 0; Pin.VertexIndex = 0; Pin.BodyTriangle = 0;
    Pin.SurfaceOffsetCm = 2.f;
    if (!TestTrue(TEXT("Create a surface pin"), CreatePin(*Set, Pin, Body, Collection, Error))) return false;
    TArray<FResolvedPin> Resolved;
    if (!TestTrue(TEXT("Resolve valid set"), ResolvePins(*Set, Body, Collection, Resolved, Error))) return false;
    if (!TestEqual(TEXT("One resolved pin"), Resolved.Num(), 1)) return false;
    TestTrue(TEXT("Triangle barycenter plus outward offset"), Resolved[0].BodyLocalTarget.Equals(FVector3f(2,2,12), 1.e-4f));
    FVector World;
    TestTrue(TEXT("Rigid target accepted"), WorldTarget(Resolved[0], FTransform(FRotator(0,90,0), FVector(10,20,30)), World, Error));
    TestTrue(TEXT("Target translates and rotates with body"), World.Equals(FVector(8,22,42), 1.e-4));
    TestTrue(TEXT("Disable"), SetPinEnabled(*Set, Pin.Id, false, Error));
    TestFalse(TEXT("Disabled state visible"), Set->Pins[0].bEnabled);
    TestTrue(TEXT("Enable"), SetPinEnabled(*Set, Pin.Id, true, Error));
    TestTrue(TEXT("Enabled state visible"), Set->Pins[0].bEnabled);
    FGarmentProbeBodyPin Alias = Pin; Alias.Id = TEXT("Alias"); Alias.PanelIndex = 1;
    TestFalse(TEXT("Welded duplicate rejected"), CreatePin(*Set, Alias, Body, Collection, Error));
    Alias.SurfaceOffsetCm = 3.f;
    TestFalse(TEXT("Conflicting welded target rejected"), CreatePin(*Set, Alias, Body, Collection, Error));
    TestEqual(TEXT("Rejected additions are atomic"), Set->Pins.Num(), 1);
    TestTrue(TEXT("Release"), ReleasePin(*Set, Pin.Id, Error));
    TestEqual(TEXT("Released pin removed"), Set->Pins.Num(), 0);
    ResetPins(*Set);
    TestEqual(TEXT("Reset restores authored pin"), Set->Pins.Num(), 1);
    TestTrue(TEXT("Reset restores authored enabled state"), Set->Pins[0].bEnabled);
    TestFalse(TEXT("Missing ID rejected"), SetPinEnabled(*Set, TEXT("Missing"), true, Error));
    auto Reject = [this, &Body, &Collection, &Resolved, &Error, &Set](const TCHAR* Name, const FGarmentProbeBodyPin& Bad) {
        Set->Pins = {Bad}; TestFalse(Name, ResolvePins(*Set, Body, Collection, Resolved, Error));
        TestFalse(TEXT("Rejection has reason"), Error.IsEmpty());
    };
    auto Bad = Pin; Bad.VertexIndex = 100; Reject(TEXT("Bad cloth vertex"), Bad);
    Bad = Pin; Bad.BodyTriangle = 3; Reject(TEXT("Bad body triangle"), Bad);
    Bad = Pin; Bad.SurfaceOffsetCm = -1; Reject(TEXT("Inside surface target rejected"), Bad);
    Bad = Pin; Bad.Barycentric = FVector3f(-1,1,1); Reject(TEXT("Outside triangle target"), Bad);
    Bad = Pin; Bad.Barycentric.X = std::numeric_limits<float>::quiet_NaN(); Reject(TEXT("Nonfinite target"), Bad);
    Set->Pins = {Pin}; Set->BodyMesh.Reset();
    TestFalse(TEXT("Missing body rejected"), ResolvePins(*Set, Body, Collection, Resolved, Error));
    TestFalse(TEXT("Non-rigid scale rejected"), WorldTarget(Resolved.IsEmpty() ? FResolvedPin{} : Resolved[0], FTransform(FQuat::Identity, FVector::ZeroVector, FVector(2,1,1)), World, Error));
    TestFalse(TEXT("Zero scale rejected"), WorldTarget(FResolvedPin{}, FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), World, Error));
    TestFalse(TEXT("Negative scale rejected"), WorldTarget(FResolvedPin{}, FTransform(FQuat::Identity, FVector::ZeroVector, FVector(-1)), World, Error));
    Set->BodyMesh = TSoftObjectPtr<UStaticMesh>(Body.Mesh);
    Set->Pins = {Pin};
    Body.CollisionSpheres = {FSphere(FVector(2,2,12), 4.)};
    TestFalse(TEXT("Positive offset target inside active collision proxy rejected"), ResolvePins(*Set, Body, Collection, Resolved, Error));
    Body.CollisionSpheres.Reset();
    // Catches stale authored history: release+recreate must not make ResetPins
    // restore two identical IDs or two pins on the same welded particle.
    TestTrue(TEXT("Release before reauthoring"), ReleasePin(*Set, Pin.Id, Error));
    Pin.SurfaceOffsetCm = 3.f;
    TestTrue(TEXT("Recreate same released ID at new target"), CreatePin(*Set, Pin, Body, Collection, Error));
    ResetPins(*Set);
    TestEqual(TEXT("Reset restores one latest authored pin"), Set->Pins.Num(), 1);
    TestTrue(TEXT("Reset after reauthoring stays valid"), ResolvePins(*Set, Body, Collection, Resolved, Error));
    if (Set->Pins.Num() == 1) TestEqual(TEXT("Latest authored target survives reset"), Set->Pins[0].SurfaceOffsetCm, 3.f);
    UGarmentProbeBodyPinSet* Disabled = NewObject<UGarmentProbeBodyPinSet>();
    UGarmentProbeBodyPinSet* Released = NewObject<UGarmentProbeBodyPinSet>();
    Disabled->BodyMesh = Released->BodyMesh = Set->BodyMesh;
    Disabled->AuthoredPins = Released->AuthoredPins = Set->AuthoredPins;
    Disabled->Pins = Set->Pins;
    Disabled->Pins[0].bEnabled = false;
    TestTrue(TEXT("Valid persisted lifecycle accepted"), ValidateSavedLifecycle(*Set, *Disabled, *Released, Body, Collection, Error));
    // A counts-only checker passes these corruptions and is caught here.
    Released->AuthoredPins[0].BodyTriangle = 99;
    TestFalse(TEXT("Corrupt released reset triangle rejected"), ValidateSavedLifecycle(*Set, *Disabled, *Released, Body, Collection, Error));
    Released->AuthoredPins = Set->AuthoredPins;
    Released->AuthoredPins[0].SurfaceOffsetCm = 9.f;
    TestFalse(TEXT("Corrupt released reset target rejected"), ValidateSavedLifecycle(*Set, *Disabled, *Released, Body, Collection, Error));
    Released->AuthoredPins = Set->AuthoredPins;
    Disabled->Pins[0].VertexIndex = 2;
    TestFalse(TEXT("Corrupt disabled cloth target rejected"), ValidateSavedLifecycle(*Set, *Disabled, *Released, Body, Collection, Error));
    Disabled->Pins = Set->Pins; Disabled->Pins[0].bEnabled = false;
    Disabled->BodyTransform.SetTranslation(FVector(10,0,0));
    TestFalse(TEXT("Corrupt disabled body transform rejected"), ValidateSavedLifecycle(*Set, *Disabled, *Released, Body, Collection, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBodyPinNativeTest, "GarmentFitClothProbe.BodyPins.NativeFollowReleaseAndReopen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBodyPinNativeTest::RunTest(const FString&)
{
    // Missing native pin motion, free release, independently imported body,
    // saved pin state, or fresh-process reopening must fail this boundary test.
    FString Error;
    FRunContext Context;
    if (!Support::GetRunContext(TEXT("BodyPins"), Context, Error) || !RunExperiment(Context, Error))
    { AddError(Error); return false; }
    return true;
}
