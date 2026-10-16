#pragma once
#include "Probe.h"

class UChaosClothAsset;
class UChaosClothComponent;
class FPreviewScene;
struct FManagedArrayCollection;

namespace GarmentFitClothProbe::Support
{
bool GetRunContext(const FString& Feature, FRunContext& Out, FString& Error);
bool SaveSourceAsset(UObject* Asset, FString& Error);
// PackageName is a long package name below /GarmentFitClothProbe/, without .Object suffix.
UChaosClothAsset* BuildNativeAsset(const TSharedRef<FManagedArrayCollection>& Collection,
    const FString& PackageName, FString& Error);
TSharedPtr<FManagedArrayCollection> BuildPanelCollection(const FInput& Input, FString& Error);
void ApplyDefaultConfig(const TSharedRef<FManagedArrayCollection>& Collection);
bool ResolveVertex(const TSharedRef<const FManagedArrayCollection>& Collection,
    FVertexRef Vertex, int32& WeldedVertex, FString& Error);
void TickCloth(UChaosClothComponent* Component, float Dt);
bool ReadNativePositions(UChaosClothComponent* Component, TArray<FVector3f>& Out, FString& Error);
bool EnsureClothRenderReady(UChaosClothComponent* Component, FString& Error);
// With SurfaceToVerify, first isolate that native component and require >=1000
// material pixels; save cloth-only PNG + coverage JSON, then the full scene PNG.
// Call after clearing debug lines for a clean surface capture.
bool CapturePreview(FPreviewScene& Scene, const FString& Filename, const FVector& CameraLocation,
    const FVector& LookAt, FString& Error, UChaosClothComponent* SurfaceToVerify = nullptr);
}
