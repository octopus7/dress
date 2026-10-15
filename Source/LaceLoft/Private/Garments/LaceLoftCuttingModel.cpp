#include "Garments/LaceLoftCuttingModel.h"
namespace
{
constexpr double ToleranceMm=.8;
// Slate's float geometry can place a closed endpoint microns off either segment.
// Resolve this numerical ambiguity by continuity without widening input tolerance.
constexpr double ProjectionTieMm=.001;
struct FProjection { double Arc=0,Distance=MAX_dbl; };
FProjection Project(const FLaceLoftPatternPath& Path,FVector2D Point,double PreviousArc)
{
    FProjection Best;double Offset=0;
    for(int32 I=1;I<Path.PointsMm.Num();++I)
    {
        const FVector2D A=Path.PointsMm[I-1],Delta=Path.PointsMm[I]-A;
        const double Length=Delta.Size();if(Length<.000001)continue;
        const double T=FMath::Clamp(FVector2D::DotProduct(Point-A,Delta)/(Length*Length),0.,1.);
        const double Distance=FVector2D::Distance(Point,A+Delta*T),Arc=Offset+Length*T;
        if(Distance<Best.Distance-ProjectionTieMm || (FMath::IsNearlyEqual(Distance,Best.Distance,ProjectionTieMm)
            && FMath::Abs(Arc-PreviousArc)<FMath::Abs(Best.Arc-PreviousArc)))Best={Arc,Distance};
        Offset+=Length;
    }
    return Best;
}
void AddInterval(FLaceLoftPathProgress& Progress,double A,double B)
{
    if(FMath::Abs(B-A)<.00001)return;
    Progress.Intervals.Emplace(FMath::Min(A,B),FMath::Max(A,B));
    Progress.Intervals.Sort([](const auto& X,const auto& Y){return X.StartMm<Y.StartMm;});
    TArray<FLaceLoftInterval> Union;
    for(const auto& Interval:Progress.Intervals)
    {
        if(Union.IsEmpty() || Interval.StartMm>Union.Last().EndMm+.00001)Union.Add(Interval);
        else Union.Last().EndMm=FMath::Max(Union.Last().EndMm,Interval.EndMm);
    }
    Progress.Intervals=MoveTemp(Union);
}
}
double FLaceLoftPathProgress::CoveredMm() const
{double Sum=0;for(const auto& Interval:Intervals)Sum+=Interval.EndMm-Interval.StartMm;return Sum;}
void FLaceLoftCuttingModel::SetPaths(const TArray<FLaceLoftPatternPath>& InPaths)
{
    Paths.Reset();Coverage.Reset();ActivePath=NAME_None;bTracking=false;
    for(const auto& Path:InPaths)Paths.Add(Path.PathId,Path);
}
bool FLaceLoftCuttingModel::BeginStroke(FName Id,FVector2D Point,bool bDirected)
{
    EndStroke();const auto* Path=Paths.Find(Id);
    if(!Path || Point.ContainsNaN())return false;
    const auto Projection=Project(*Path,Point,0);
    if(Projection.Distance>ToleranceMm)return false;
    ActivePath=Id;Before=Progress(Id);PreviousPoint=Point;PreviousArc=Projection.Arc;
    bTracking=true;bForward=bDirected;return true;
}
bool FLaceLoftCuttingModel::UpdateStroke(FVector2D Point)
{
    if(!bTracking)return false;
    const auto* Path=Paths.Find(ActivePath);
    if(!Path || Point.ContainsNaN()){bTracking=false;return false;}
    const auto Projection=Project(*Path,Point,PreviousArc);
    const double ArcTravel=Projection.Arc-PreviousArc,Travel=FVector2D::Distance(Point,PreviousPoint);
    // A fast jump or a chord cutting across a curved path cannot create coverage.
    if(Projection.Distance>ToleranceMm || Travel>3. || FMath::Abs(ArcTravel)>Travel+ToleranceMm
        || (bForward && ArcTravel<-.00001)) {bTracking=false;return false;}
    double Offset=0;
    for(int32 I=1;I<Path->PointsMm.Num();++I)
    {
        Offset+=FVector2D::Distance(Path->PointsMm[I-1],Path->PointsMm[I]);
        if(Offset<=FMath::Min(PreviousArc,Projection.Arc) || Offset>=FMath::Max(PreviousArc,Projection.Arc))continue;
        const FVector2D Delta=Point-PreviousPoint;
        const double T=Travel>.00001?FMath::Clamp(FVector2D::DotProduct(Path->PointsMm[I]-PreviousPoint,Delta)/(Travel*Travel),0.,1.):0;
        if(FVector2D::Distance(Path->PointsMm[I],PreviousPoint+Delta*T)>ToleranceMm){bTracking=false;return false;}
    }
    AddInterval(Coverage.FindOrAdd(ActivePath),PreviousArc,Projection.Arc);
    PreviousArc=Projection.Arc;PreviousPoint=Point;return true;
}
TOptional<FLaceLoftStrokeChange> FLaceLoftCuttingModel::EndStroke()
{
    bTracking=false;const FName Id=ActivePath;ActivePath=NAME_None;
    if(Id.IsNone() || Progress(Id).CoveredMm()<=Before.CoveredMm()+.00001)return {};
    return FLaceLoftStrokeChange{Id,Before,Progress(Id)};
}
const FLaceLoftPathProgress& FLaceLoftCuttingModel::Progress(FName Id) const
{static const FLaceLoftPathProgress Empty;const auto* Found=Coverage.Find(Id);return Found?*Found:Empty;}
bool FLaceLoftCuttingModel::IsComplete(FName Id) const
{const auto* Path=Paths.Find(Id);return Path && Path->LengthMm()>0 && Progress(Id).CoveredMm()>=Path->LengthMm()-.02;}
void FLaceLoftCuttingModel::ApplyChange(const FLaceLoftStrokeChange& Change,bool bUndo)
{EndStroke();RestoreProgress(Change.PathId,bUndo?Change.Before:Change.After);}
bool FLaceLoftCuttingModel::RestoreProgress(FName Id,const FLaceLoftPathProgress& Value)
{
    const auto* Path=Paths.Find(Id);if(!Path)return false;
    FLaceLoftPathProgress Validated;
    for(const auto& Interval:Value.Intervals)
    {
        if(!FMath::IsFinite(Interval.StartMm) || !FMath::IsFinite(Interval.EndMm) || Interval.StartMm<0
            || Interval.EndMm>Path->LengthMm()+.00001 || Interval.EndMm<=Interval.StartMm)return false;
        AddInterval(Validated,Interval.StartMm,Interval.EndMm);
    }
    Coverage.Add(Id,MoveTemp(Validated));return true;
}
FVector2D FLaceLoftCuttingModel::ScreenToMm(FVector2D Screen,FVector2D Origin,double Dpi,double Zoom,FVector2D Pan,double Px)
{return Dpi>0?LocalToMm((Screen-Origin)/Dpi,Zoom,Pan,Px):FVector2D::ZeroVector;}
FVector2D FLaceLoftCuttingModel::LocalToMm(FVector2D Local,double Zoom,FVector2D Pan,double Px)
{return Zoom>0 && Px>0?(Local-Pan)/(Zoom*Px):FVector2D::ZeroVector;}
