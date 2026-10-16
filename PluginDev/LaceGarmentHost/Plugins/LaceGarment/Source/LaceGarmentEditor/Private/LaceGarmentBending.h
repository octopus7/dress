#pragma once
#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

namespace LaceGarment
{
// Signed dihedral convention matches Chaos: flat panels have angle zero.
// Keep authoring coordinates and derivatives in double precision; converting
// small boundary triangles at doll-scale world coordinates to FSolverVec3
// quantizes the nonlinear objective independently of its derivative.
struct FDihedralSample
{
    double Angle=0;
    TStaticArray<FVector3d,4> Gradient;
};
inline FDihedralSample EvaluateDihedral(FVector3d A,FVector3d B,FVector3d C,FVector3d D)
{
    FDihedralSample Result;
    for(auto& G : Result.Gradient) G=FVector3d::ZeroVector;
    const auto Raw1=FVector3d::CrossProduct(A-C,B-C),Raw2=FVector3d::CrossProduct(B-D,A-D);
    const double Area1=Raw1.Length(),Area2=Raw2.Length(),Length=(B-A).Length();
    if(Area1<1.e-18 || Area2<1.e-18 || Length<1.e-12) return Result;
    const auto N1=Raw1/Area1,N2=Raw2/Area2,Edge=(B-A)/Length;
    const double Cos=FMath::Clamp(FVector3d::DotProduct(N1,N2),-1.0,1.0);
    const double Sin=FMath::Clamp(FVector3d::DotProduct(FVector3d::CrossProduct(N2,N1),Edge),-1.0,1.0);
    Result.Angle=FMath::Atan2(Sin,Cos);
    const auto U=(Cos*FVector3d::CrossProduct(Edge,N2)-Sin*N2)/Area1;
    const auto V=(Cos*FVector3d::CrossProduct(N1,Edge)-Sin*N1)/Area2;
    const auto AC=FVector3d::CrossProduct(B-C,U),BC=FVector3d::CrossProduct(U,A-C);
    const auto BD=FVector3d::CrossProduct(A-D,V),AD=FVector3d::CrossProduct(V,B-D);
    Result.Gradient[0]=AC+AD; Result.Gradient[1]=BC+BD;
    Result.Gradient[2]=-AC-BC; Result.Gradient[3]=-AD-BD;
    return Result;
}
}
