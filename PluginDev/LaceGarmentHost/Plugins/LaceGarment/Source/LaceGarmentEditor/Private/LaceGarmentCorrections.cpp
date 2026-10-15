#include "LaceGarmentCorrections.h"
#include "Engine/SkeletalMesh.h"
#include "Serialization/MemoryWriter.h"
THIRD_PARTY_INCLUDES_START
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

namespace LaceGarment
{
namespace
{
FString Digest(const TArray<uint8>& Bytes)
{
    uint8 Hash[SHA256_DIGEST_LENGTH];
    if(!SHA256(Bytes.GetData(),Bytes.Num(),Hash)) return FString();
    return BytesToHex(Hash,SHA256_DIGEST_LENGTH).ToLower();
}
FGuid StableId(const FString& Text)
{
    TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FString Copy=Text;Writer<<Copy;
    FGuid Id;FGuid::ParseExact(Digest(Bytes).Left(32),EGuidFormats::Digits,Id);return Id;
}
}

bool MakeGeneratedCorrespondence(const ULaceGarmentDocument& Document,const FBuildResult& Build,FGeneratedCorrespondence& Out,FString& Error)
{
    Error.Reset();
    auto Fail=[&](const TCHAR* Message){Error=Message;return false;};
    FSampledDocument Source;if(!SampleDocument(Document,Source,Error)) return false;
    const int32 N=Build.SettledCm.Num();
    if(N<3 || N>65536 || Build.Panels.Num()!=Source.Input.Panels.Num() || Build.Offsets.Num()!=Build.Panels.Num())
        return Fail(TEXT("Generated correspondence requires complete panel topology"));
    TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);
    FString Version=TEXT("LaceGarment-generated-correspondence-v1");Writer<<Version;
    FGeneratedCorrespondence Result;Result.SimulationVertexIds.SetNum(N);
    int32 Offset=0;
    for(int32 I=0;I<Build.Panels.Num();++I)
    {
        FPanelMesh Expected;if(!BuildPanel(Source.Input.Panels[I],Expected,Error)) return false;
        const auto& Panel=Build.Panels[I];
        if(Build.Offsets[I]!=Offset || Panel.RestMm!=Expected.RestMm || Panel.Triangles!=Expected.Triangles
            || Panel.BoundaryVertices!=Expected.BoundaryVertices || Panel.InitialCm!=Expected.InitialCm || Offset+Panel.RestMm.Num()>N)
            return Fail(TEXT("Generated panel no longer matches the document; regenerate before applying corrections"));
        FGuid Piece=Source.Input.Panels[I].Id;Writer<<Piece;
        Writer<<Expected.RestMm;Writer<<Expected.Triangles;
        for(int32 V=0;V<Panel.RestMm.Num();++V)
        {
            TArray<uint8> VertexBytes;FMemoryWriter VertexWriter(VertexBytes);
            FVector2d Rest=Panel.RestMm[V];VertexWriter<<Piece;VertexWriter<<Rest;
            FGuid Id;FGuid::ParseExact(Digest(VertexBytes).Left(32),EGuidFormats::Digits,Id);
            if(!Id.IsValid()) return Fail(TEXT("Could not hash generated vertex identity"));
            Result.SimulationVertexIds[Offset+V]=Id;
        }
        Offset+=Panel.RestMm.Num();
    }
    if(Offset!=N) return Fail(TEXT("Panel topology does not cover generated vertices"));
    TArray<FIntVector2> ExpectedStitches;
    for(const auto& Seam : Source.Input.Seams)
    {
        auto Chain=[&](int32 Panel,const TArray<int32>& Edges,int32 Default,TArray<int32>& Vertices)
        {
            if(!Build.Panels.IsValidIndex(Panel)) return false;
            for(int32 Edge : (Edges.IsEmpty()?TArray<int32>{Default}:Edges))
            {
                if(!Build.Panels[Panel].BoundaryVertices.IsValidIndex(Edge)) return false;
                const auto& Points=Build.Panels[Panel].BoundaryVertices[Edge];
                if(Points.IsEmpty() || (!Vertices.IsEmpty() && Vertices.Last()!=Points[0])) return false;
                if(Vertices.IsEmpty()) Vertices.Add(Points[0]);
                for(int32 K=1;K<Points.Num();++K) Vertices.Add(Points[K]);
            }
            return true;
        };
        TArray<int32> A,B;
        if(!Chain(Seam.PanelA,Seam.BoundaryEdgesA,Seam.EdgeA,A) || !Chain(Seam.PanelB,Seam.BoundaryEdgesB,Seam.EdgeB,B) || A.Num()!=B.Num())
            return Fail(TEXT("Document seam cannot resolve generated boundaries"));
        for(int32 K=0;K<A.Num();++K)
            ExpectedStitches.Add(FIntVector2(Build.Offsets[Seam.PanelA]+A[K],Build.Offsets[Seam.PanelB]+B[Seam.bReverseB?B.Num()-1-K:K]));
    }
    if(Build.Stitches!=ExpectedStitches) return Fail(TEXT("Generated seams no longer match the document"));
    Writer<<ExpectedStitches;
    TArray<int32> Parents;Parents.SetNumUninitialized(N);for(int32 I=0;I<N;++I) Parents[I]=I;
    auto Root=[&](int32 I){while(Parents[I]!=I){Parents[I]=Parents[Parents[I]];I=Parents[I];}return I;};
    for(auto S : Build.Stitches)
    {
        if(!Parents.IsValidIndex(S.X) || !Parents.IsValidIndex(S.Y)) return Fail(TEXT("Invalid generated stitch"));
        int32 A=Root(S.X),B=Root(S.Y);Parents[FMath::Max(A,B)]=FMath::Min(A,B);
    }
    TMap<int32,TArray<FString>> Members;
    for(int32 I=0;I<N;++I) Members.FindOrAdd(Root(I)).Add(Result.SimulationVertexIds[I].ToString(EGuidFormats::Digits));
    TMap<int32,FGuid> GroupIds;
    for(auto& Group : Members)
    {
        Group.Value.Sort();const FGuid Id=StableId(FString::Join(Group.Value,TEXT("/")));
        if(!Id.IsValid()) return Fail(TEXT("Could not hash welded vertex identity"));
        GroupIds.Add(Group.Key,Id);
    }
    for(int32 I=0;I<N;++I) Result.SimulationVertexIds[I]=GroupIds[Root(I)];
    TArray<int32> Pins;
    for(const auto& Pin : Source.Input.Pins)
    {
        if(!Build.Panels.IsValidIndex(Pin.Panel) || !Build.Panels[Pin.Panel].RestMm.IsValidIndex(Pin.BoundaryPoint))
            return Fail(TEXT("Invalid document anchor"));
        Pins.AddUnique(Build.Offsets[Pin.Panel]+Pin.BoundaryPoint);
    }
    Pins.Sort();
    TArray<int32> ActualPins=Build.PinnedVertices.Array();ActualPins.Sort();
    if(Pins!=ActualPins) return Fail(TEXT("Generated anchors no longer match the document"));
    Writer<<Pins;
    FString BodyPath=Build.ReferenceMesh.ToString();Writer<<BodyPath;
    if(!Build.ReferenceMesh.IsNull())
    {
        auto* Body=Cast<USkeletalMesh>(Build.ReferenceMesh.TryLoad());
        if(!Body) return Fail(TEXT("Cannot resolve reference body for joint identities"));
        const auto& Skeleton=Body->GetRefSkeleton();TArray<FString> Paths;
        for(int32 I=0;I<Skeleton.GetNum();++I)
        {
            const int32 Parent=Skeleton.GetParentIndex(I);
            if(Parent>=I || Parent<INDEX_NONE) return Fail(TEXT("Reference skeleton hierarchy is invalid"));
            Paths.Add((Parent==INDEX_NONE?BodyPath:Paths[Parent])+TEXT("/")+Skeleton.GetBoneName(I).ToString());
            const FGuid Id=StableId(Paths.Last());if(!Id.IsValid()) return Fail(TEXT("Could not hash joint identity"));
            Result.JointIds.Add(Id);
        }
    }
    Writer<<Result.JointIds;Result.TopologyHash=Digest(Bytes);
    if(Result.TopologyHash.Len()!=64) return Fail(TEXT("Could not hash generated topology"));
    Out=MoveTemp(Result);return true;
}

bool ApplyDocumentCorrections(ULaceGarmentDocument& Document,FBuildResult& Build,FString& Error)
{
    FGeneratedCorrespondence Map;
    if(!MakeGeneratedCorrespondence(Document,Build,Map,Error))
    {
        Document.Modify();Document.bDerivedCurrent=false;
        for(auto& C : Document.Corrections) C.State=ELGCorrectionState::Unresolved;
        return false;
    }
    FBuildResult Result=Build;
    const int32 N=Map.SimulationVertexIds.Num();
    Result.MaxDistancesCm.Init(1,N);
    TMap<FGuid,TArray<int32>> Targets;
    for(int32 I=0;I<N;++I) Targets.FindOrAdd(Map.SimulationVertexIds[I]).Add(I);
    TSet<FGuid> Pinned;
    for(int32 V : Build.PinnedVertices) Pinned.Add(Map.SimulationVertexIds[V]);
    for(auto Id : Pinned) for(int32 V : Targets[Id]) Result.MaxDistancesCm[V]=0;
    TArray<ELGCorrectionState> States;States.Init(ELGCorrectionState::Unresolved,Document.Corrections.Num());
    TMap<FString,int32> Applied;bool bValid=true;
    for(int32 I=0;I<Document.Corrections.Num();++I)
    {
        const auto& C=Document.Corrections[I];const auto* Vertices=Targets.Find(C.Target);
        bool bCorrection=C.Domain==TEXT("simulationVertex") && C.TopologyHash==Map.TopologyHash && Vertices;
        const FString Key=C.Target.ToString()+TEXT("/")+(C.Skin.IsEmpty()?C.Property:TEXT("skin"));
        if(const int32* Duplicate=Applied.Find(Key)) {States[*Duplicate]=ELGCorrectionState::Unresolved;bCorrection=false;}
        else Applied.Add(Key,I);
        if(bCorrection && !C.Skin.IsEmpty())
        {
            bCorrection=Build.SkinJoints.Num()==N && Build.SkinWeights.Num()==N;
            TArray<int32> Joints;TArray<float> Weights;
            for(const auto& W : C.Skin)
            {
                const int32 Joint=Map.JointIds.IndexOfByKey(W.Joint);
                if(Joint==INDEX_NONE || !FMath::IsFinite(float(W.Weight)) || float(W.Weight)<=0) {bCorrection=false;break;}
                Joints.Add(Joint);Weights.Add(float(W.Weight));
            }
            if(bCorrection) for(int32 V : *Vertices) {Result.SkinJoints[V]=Joints;Result.SkinWeights[V]=Weights;}
        }
        else if(bCorrection)
        {
            bCorrection=C.Property==TEXT("maxDistance") && C.Value<=100 && (!Pinned.Contains(C.Target) || C.Value==0);
            if(bCorrection) for(int32 V : *Vertices) Result.MaxDistancesCm[V]=float(C.Value);
        }
        States[I]=bCorrection?ELGCorrectionState::Resolved:ELGCorrectionState::Unresolved;
        bValid&=bCorrection;
    }
    Document.Modify();Document.GeneratedTopologyHash=Map.TopologyHash;Document.bDerivedCurrent=bValid;
    for(int32 I=0;I<States.Num();++I) Document.Corrections[I].State=States[I];
    if(!bValid) {Error=TEXT("Manual corrections are unresolved; regenerate or explicitly remap their source targets");return false;}
    Build=MoveTemp(Result);Error.Reset();return true;
}
}
