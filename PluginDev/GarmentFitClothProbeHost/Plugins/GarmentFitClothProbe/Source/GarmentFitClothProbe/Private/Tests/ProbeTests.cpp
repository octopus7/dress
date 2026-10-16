#include "Probe.h"
#include "Misc/AutomationTest.h"

using namespace GarmentFitClothProbe;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProbeValidation, "GarmentFitClothProbe.InputValidation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProbeValidation::RunTest(const FString&)
{
    // A removed validation branch must allow a malformed fixture and fail here.
    FInput Good;
    Good.Panels = {
        { {{0,0},{1,0},{0,1}}, {{0,0,0},{1,0,0},{0,1,0}}, {{0,1,2}} },
        { {{0,0},{1,0},{1,1}}, {{0,0,0},{1,0,0},{1,-1,0}}, {{0,1,2}} }
    };
    Good.Seam = {{0,0},{1,1}};
    FString Error;
    TestTrue(TEXT("Valid pre-aligned seam accepted"), Validate(Good, Error));
    auto Reject = [this, &Error](const TCHAR* Name, const FInput& Bad) {
        TestFalse(Name, Validate(Bad, Error));
        TestFalse(TEXT("Reason returned"), Error.IsEmpty());
    };
    FInput Bad = Good; Bad.Panels.Pop(); Reject(TEXT("Missing panel"), Bad);
    Bad = Good; Bad.Seam[0].Y = 30; Reject(TEXT("Bad seam index"), Bad);
    Bad = Good; Bad.Panels[0].Triangles[0].Z = 0; Reject(TEXT("Degenerate triangle"), Bad);
    Bad = Good; Bad.Panels[1].Initial3D[0].Z = 2; Reject(TEXT("Separated seam"), Bad);
    Bad = Good; Bad.Seam.Add(FIntVector2(0,0)); Reject(TEXT("Duplicate correspondence"), Bad);
    Bad = Good; Bad.Panels[0].Source2D.Pop(); Reject(TEXT("Mismatched 2D/3D counts"), Bad);
    Bad = Good; Bad.Seam.Empty(); Reject(TEXT("Missing seam"), Bad);
    Bad = Good; Bad.Panels[0].Triangles[0].Y = 30; Reject(TEXT("Bad triangle index"), Bad);
    Bad = Good; Bad.Panels[0].Initial3D[2] = FVector3f(0.5f,0,0); Reject(TEXT("Zero area 3D"), Bad);
    Bad = Good; Bad.Panels[0].Source2D[2] = FVector2f(0.5f,0); Reject(TEXT("Zero area 2D"), Bad);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProbeNative, "GarmentFitClothProbe.NativeTenSecondsAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProbeNative::RunTest(const FString&)
{
    // Missing simulation, collision, reset, saved asset or finite/deforming output must fail.
    FString Error;
    const bool bResult = Run(Error);
    if (!bResult) AddError(Error);
    return bResult;
}
