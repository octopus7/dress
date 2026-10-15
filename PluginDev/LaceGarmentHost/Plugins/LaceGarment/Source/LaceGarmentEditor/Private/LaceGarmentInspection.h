#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace LaceGarment
{
bool CaptureInspectionViews(UWorld* World,const FString& Directory,FVector Center,float OrthoWidth,const TArray<FVector>& Cameras);
}
