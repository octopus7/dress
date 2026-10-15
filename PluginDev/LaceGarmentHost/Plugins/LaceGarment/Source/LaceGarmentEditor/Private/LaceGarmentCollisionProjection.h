#pragma once
#include "LaceGarmentPipeline.h"

namespace LaceGarment
{
bool ReadPhysicsConvex(const FKConvexElem& Element,const FTransform& Bone,FConvexCollider& Out,FString& Error);
// Internal solver operation, shared with collision regression fixtures.
FVector3d ProjectBodyCollision(FVector3d Position,TConstArrayView<FCapsule> Capsules,double ThicknessCm,TConstArrayView<FConvexCollider> Convexes={});
FVector3d SweepBodyCollision(FVector3d Previous,FVector3d Target,TConstArrayView<FCapsule> Capsules,double ThicknessCm,TConstArrayView<FConvexCollider> Convexes={});
}
