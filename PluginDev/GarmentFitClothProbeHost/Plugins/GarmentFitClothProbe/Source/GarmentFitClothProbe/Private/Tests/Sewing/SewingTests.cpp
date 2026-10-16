#include "Sewing/SewingProbe.h"
#include "ProbeSupport.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include <limits>

using namespace GarmentFitClothProbe;
using namespace GarmentFitClothProbe::Sewing;

namespace
{
FInput StripFixture(TArray<float> RowsA = {0.f, 50.f, 100.f}, TArray<float> RowsB = {0.f, 50.f, 100.f})
{
    FInput Input;
    Input.Panels.SetNum(2);
    const TArray<float> Rows[2] = {RowsA, RowsB};
    for (int32 P = 0; P < 2; ++P)
    {
        FPanel& Panel = Input.Panels[P];
        for (float Y : Rows[P])
            for (int32 X = 0; X < 2; ++X)
            {
                Panel.Source2D.Emplace(X * 10.f, Y);
                Panel.Initial3D.Emplace(X * 10.f + (P == 0 ? -10.f : 0.f), Y, 100.f);
            }
        for (int32 I = 0; I < Rows[P].Num() - 1; ++I)
        {
            Panel.Triangles.Emplace(I * 2, I * 2 + 1, I * 2 + 3);
            Panel.Triangles.Emplace(I * 2, I * 2 + 3, I * 2 + 2);
        }
    }
    return Input;
}
FRequest Edges(const FInput& Input)
{
    FRequest R;
    R.A.PanelIndex = 0;
    R.B.PanelIndex = 1;
    for (int32 I = 1; I < Input.Panels[0].Source2D.Num(); I += 2) R.A.Vertices.Add(I);
    for (int32 I = 0; I < Input.Panels[1].Source2D.Num(); I += 2) R.B.Vertices.Add(I);
    return R;
}
double Area(const FPanel& Panel)
{
    double Sum = 0;
    for (const FIntVector3 T : Panel.Triangles)
    {
        const FVector2f A = Panel.Source2D[T.Y] - Panel.Source2D[T.X];
        const FVector2f B = Panel.Source2D[T.Z] - Panel.Source2D[T.X];
        Sum += 0.5 * FMath::Abs(double(A.X) * B.Y - double(A.Y) * B.X);
    }
    return Sum;
}
constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

// Catches confusing vertex count or 3D placement with original 2D arc length.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSewEqual, "GarmentFitClothProbe.Sewing.EqualLength", Flags)
bool FSewEqual::RunTest(const FString&)
{
    const FInput Source = StripFixture();
    FPreparedSeam Result; FString Error;
    if (!TestTrue(TEXT("Equal 100cm edges prepare"), PrepareSeam(Source, Edges(Source), Result, Error))) { AddInfo(Error); return false; }
    TestEqual(TEXT("A source arc length"), Result.OriginalLengthA, 100.0);
    TestEqual(TEXT("B source arc length"), Result.OriginalLengthB, 100.0);
    TestEqual(TEXT("Ratio"), Result.LengthRatioBOverA, 1.0);
    TestEqual(TEXT("Three stitches"), Result.Input.Seam.Num(), 3);
    TestEqual(TEXT("First pair A"), Result.Input.Seam[0].X, 1);
    TestEqual(TEXT("First pair B"), Result.Input.Seam[0].Y, 0);
    return true;
}

// Catches index interpolation, dropping original knots, or unrefined triangles.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSewRefine, "GarmentFitClothProbe.Sewing.NonuniformRefinement", Flags)
bool FSewRefine::RunTest(const FString&)
{
    const FInput Source = StripFixture({0.f, 25.f, 100.f}, {0.f, 50.f, 75.f, 100.f});
    FPreparedSeam Result; FString Error;
    if (!TestTrue(TEXT("Different sample counts prepare"), PrepareSeam(Source, Edges(Source), Result, Error))) { AddInfo(Error); return false; }
    if (!TestEqual(TEXT("Union has five knots"), Result.NormalizedKnots.Num(), 5)) return false;
    const double Expected[] = {0., .25, .5, .75, 1.};
    for (int32 I = 0; I < 5; ++I) TestEqual(TEXT("Arc-length knot"), Result.NormalizedKnots[I], Expected[I]);
    TestEqual(TEXT("A appends two vertices"), Result.Input.Panels[0].Source2D.Num(), 8);
    TestEqual(TEXT("B appends one vertex"), Result.Input.Panels[1].Source2D.Num(), 9);
    TestEqual(TEXT("A boundary triangles refined"), Result.Input.Panels[0].Triangles.Num(), 6);
    TestEqual(TEXT("B boundary triangles refined"), Result.Input.Panels[1].Triangles.Num(), 7);
    for (int32 P = 0; P < 2; ++P)
    {
        for (int32 I = 0; I < Source.Panels[P].Source2D.Num(); ++I)
            TestTrue(TEXT("Original 2D value and index retained"), Result.Input.Panels[P].Source2D[I] == Source.Panels[P].Source2D[I]);
        TestEqual(TEXT("Source polygon area retained"), Area(Result.Input.Panels[P]), 1000.0);
    }
    for (const FIntVector2 Pair : Result.Input.Seam)
    {
        const FVector2f A = Result.Input.Panels[0].Source2D[Pair.X], B = Result.Input.Panels[1].Source2D[Pair.Y];
        TestEqual(TEXT("Matching physical arc position"), A.Y, B.Y);
        TestTrue(TEXT("Explicit prepared 3D points coincide"), Result.Input.Panels[0].Initial3D[Pair.X].Equals(Result.Input.Panels[1].Initial3D[Pair.Y], 1.e-5f));
    }
    return true;
}

// Catches ignoring input direction or applying the explicit flip twice.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSewDirection, "GarmentFitClothProbe.Sewing.DirectionFlip", Flags)
bool FSewDirection::RunTest(const FString&)
{
    const FInput Source = StripFixture();
    FRequest R = Edges(Source); Algo::Reverse(R.B.Vertices);
    FPreparedSeam Result; FString Error;
    if (!TestTrue(TEXT("Reversed authored boundary prepares"), PrepareSeam(Source, R, Result, Error))) { AddInfo(Error); return false; }
    TestEqual(TEXT("Reverse pairs first A to last B"), Result.Input.Seam[0].Y, 4);
    R.bFlipSecond = true;
    if (!TestTrue(TEXT("Explicit flip restores direction"), PrepareSeam(Source, R, Result, Error))) return false;
    TestEqual(TEXT("Flip pairs first to first"), Result.Input.Seam[0].Y, 0);
    R.A.bReverse = true; R.B.bReverse = true;
    if (!TestTrue(TEXT("Both traversal flags honored"), PrepareSeam(Source, R, Result, Error))) return false;
    TestEqual(TEXT("Reversed A begins at last original vertex"), Result.Input.Seam[0].X, 5);
    TestEqual(TEXT("Reversed B with flip begins at last"), Result.Input.Seam[0].Y, 4);
    return true;
}

// Catches accepting mismatch in EqualOnly or secretly scaling original geometry.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSewMismatch, "GarmentFitClothProbe.Sewing.MismatchPolicy", Flags)
bool FSewMismatch::RunTest(const FString&)
{
    for (int32 Swap = 0; Swap < 2; ++Swap)
    {
        const FInput Source = Swap == 0 ? StripFixture({0.f, 50.f, 100.f}, {0.f, 75.f, 150.f}) : StripFixture({0.f, 75.f, 150.f}, {0.f, 50.f, 100.f});
        FRequest R = Edges(Source); FPreparedSeam Result; FString Error;
        TestFalse(TEXT("Equal-only rejects 50cm mismatch"), PrepareSeam(Source, R, Result, Error));
        R.LengthPolicy = ELengthPolicy::NormalizedArcLength;
        if (!TestTrue(TEXT("Deliberate normalized mismatch prepares"), PrepareSeam(Source, R, Result, Error))) { AddInfo(Error); return false; }
        TestEqual(TEXT("Actual A original length"), Result.OriginalLengthA, Swap == 0 ? 100.0 : 150.0);
        TestEqual(TEXT("Actual B original length"), Result.OriginalLengthB, Swap == 0 ? 150.0 : 100.0);
        TestEqual(TEXT("Signed original difference"), Result.LengthDifferenceBMinusA, Swap == 0 ? 50.0 : -50.0);
        TestEqual(TEXT("Actual ratio B/A"), Result.LengthRatioBOverA, Swap == 0 ? 1.5 : 2.0 / 3.0);
        for (int32 P = 0; P < 2; ++P)
            for (int32 I = 0; I < Source.Panels[P].Source2D.Num(); ++I)
                TestTrue(TEXT("Mismatch never scales original 2D"), Result.Input.Panels[P].Source2D[I] == Source.Panels[P].Source2D[I]);
    }
    return true;
}

// Each invalid request is checked after a successful control, so an always-false
// placeholder cannot pass the negative-validation suite.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSewInvalidBoundary, "GarmentFitClothProbe.Sewing.InvalidBoundary", Flags)
bool FSewInvalidBoundary::RunTest(const FString&)
{
    const FInput Valid = StripFixture(); FPreparedSeam Result; FString Error;
    if (!TestTrue(TEXT("Positive boundary control"), PrepareSeam(Valid, Edges(Valid), Result, Error))) { AddInfo(Error); return false; }
    FRequest R = Edges(Valid); R.A.Vertices[1] = 99;
    TestFalse(TEXT("Out-of-range vertex"), PrepareSeam(Valid, R, Result, Error));
    R = Edges(Valid); R.A.PanelIndex = 8;
    TestFalse(TEXT("Out-of-range panel"), PrepareSeam(Valid, R, Result, Error));
    R = Edges(Valid); R.A.Vertices[1] = R.A.Vertices[0];
    TestFalse(TEXT("Repeated boundary vertex"), PrepareSeam(Valid, R, Result, Error));
    R = Edges(Valid); R.A.Vertices = {0, 3};
    TestFalse(TEXT("Interior triangle edge is not a boundary"), PrepareSeam(Valid, R, Result, Error));
    R = Edges(Valid); R.A.Vertices = {1, 5};
    TestFalse(TEXT("Skipping a boundary edge"), PrepareSeam(Valid, R, Result, Error));
    FInput Bad = Valid; for (FVector2f& P : Bad.Panels[0].Source2D) P.Y = 0;
    TestFalse(TEXT("Zero-length boundary"), PrepareSeam(Bad, Edges(Bad), Result, Error));
    Bad = Valid; Bad.Panels[0].Source2D[3].Y = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Infinite coordinate"), PrepareSeam(Bad, Edges(Bad), Result, Error));
    Bad = Valid; Bad.Panels[1].Initial3D[0].X = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN placement"), PrepareSeam(Bad, Edges(Bad), Result, Error));
    R = Edges(Valid); R.EqualLengthToleranceCm = -1;
    TestFalse(TEXT("Negative tolerance"), PrepareSeam(Valid, R, Result, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSewInvalidPairs, "GarmentFitClothProbe.Sewing.InvalidCorrespondence", Flags)
bool FSewInvalidPairs::RunTest(const FString&)
{
    const FInput Source = StripFixture(); FString Error;
    TArray<FIntVector2> Pairs = {{1, 0}, {3, 2}, {5, 4}};
    if (!TestTrue(TEXT("Positive correspondence control"), ValidateCorrespondence(Source, Pairs, Error))) { AddInfo(Error); return false; }
    Pairs = {{1, 0}, {1, 0}};
    TestFalse(TEXT("Duplicate stitch"), ValidateCorrespondence(Source, Pairs, Error));
    Pairs = {{1, 0}, {1, 2}};
    TestFalse(TEXT("Conflicting A"), ValidateCorrespondence(Source, Pairs, Error));
    Pairs = {{1, 0}, {3, 0}};
    TestFalse(TEXT("Conflicting B"), ValidateCorrespondence(Source, Pairs, Error));
    Pairs = {{1, 0}, {99, 2}};
    TestFalse(TEXT("Invalid stitch index"), ValidateCorrespondence(Source, Pairs, Error));
    return true;
}

// Real 10s native XPBD runs, saved/reopened sources, reset/replay, and a same-3D
// control with equal 2D rest lengths must show the mismatch affects the solver.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSewNative, "GarmentFitClothProbe.Sewing.NativeExperiment", Flags)
bool FSewNative::RunTest(const FString&)
{
    FRunContext Context; FString Error;
    if (!Support::GetRunContext(TEXT("Sewing"), Context, Error)) { AddError(Error); return false; }
    const bool bOk = RunExperiment(Context, Error);
    if (!bOk) AddError(Error);
    return bOk;
}
