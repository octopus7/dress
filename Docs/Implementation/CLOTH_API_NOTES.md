# UE 5.8.3 Cloth 제작 조사

2026-10-16. 독립 Astra/High 조사 결과이며 실행 검증 결과가 아니다.
소스 기준: `E:/WorkTemp/Epic Games/UE_5.8/Engine`.

- `FCollectionClothFacade::DefineSchema`, `AddGetSimPattern().Initialize(2D,3D,indices)`로 패널 생성. index는 패널 내부 값.
- `AddGetSeam().Initialize`의 stitch는 전역 2D index. 봉제는 3D 정점을 실제 용접/재번호화하므로 이후 최종 정점 기준으로 weights/maps 작성.
- 5.8의 `SetSkeletalMeshPathName`, `SetPhysicsAssetPathName`은 deprecated **빈 함수**다. `SetSkeletalMeshSoftObjectPathName`, `SetPhysicsAssetSoftObjectPathName`을 사용해야 한다.
- `FClothGeometryTools::CopySimMeshToRenderMesh`는 마지막에 render weights를 root로 덮어쓴다. 호출 후 sim lookup에 따라 render weights를 다시 복사해야 한다.
- `UChaosClothConfig`와 `UChaosClothSharedSimConfig` → `Chaos::FClothingSimulationConfig::Initialize` → `GetPropertyCollection(0)->CopyTo`로 기본 설정을 작성할 수 있다. `FClothEngineTools::GenerateTethers`는 최종 MaxDistance 맵 후 실행.
- `UChaosClothAsset::Build`는 editor에서 실행하고 save/cook한다. skeleton path가 없으면 root-only skeleton이 만들어질 수 있으므로 실제 reference skeleton을 검사한다.
- Dataflow는 등록 node factory로 생성. `FChaosClothAssetTerminalNode_v2::CollectionLods`는 저장된 collection을 입력 기본값으로 사용할 수 있다. 최소 graph는 제작 근거를 저장할 수 있지만 파라미터 편집성은 추가 노드가 필요하다.
- `UChaosClothComponent::SetAsset`, 실제 `USkinnedMeshComponent` 부모에 attach하여 pose를 공유. 급격한 포즈 변경 시 `ForceNextUpdateTeleportAndReset`.
- 기본 collision thickness 1cm, self collision 0.5cm는 15cm 인형에 과도할 수 있다. 초기 실험값은 0.03~0.08cm, 실제 간극·안정성으로 결정.
- 기본 최소 입자 질량 0.0001kg의 clamp도 검토. 액터 스케일로 문제를 숨기지 않는다.
- `ChaosClothAsset`, `ChaosClothAssetEngine`은 Runtime. `ChaosClothAssetDataflowNodes`, `ChaosClothAssetTools`는 Editor. `UDataflow::IsEditorOnly()`는 true이며 ClothAsset Serialize는 cooked render/simulation data를 별도로 저장한다.

주요 헤더는 각 plugin의 `Public/ChaosClothAsset/CollectionClothFacade.h`, `CollectionClothSimPatternFacade.h`, `ClothAsset.h`, `ClothComponent.h`, `TerminalNode.h`이다.

공식 자료: [UE 5.8 Cloth 변경](https://dev.epicgames.com/community/learning/tutorials/Wb2V/unreal-engine-chaos-cloth-updates-5-8), [Cloth Asset 개요](https://dev.epicgames.com/documentation/en-us/unreal-engine/panel-cloth-editor-overview), [SetAsset](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/ClothComponent/SetAsset).

## 실제 시뮬레이션 검증 경로

- 게임/PIE 월드의 후반 tick에서 `WaitForExistingParallelClothSimulation_GameThread()` 호출 후 proxy의 `GetCurrentSimulationData_AnyThread()` 읽기. 5.8의 component `GetUpdateClothSimulationData_AnyThread`는 protected라 직접 호출할 수 없다(컴파일로 확인).
- `FClothSimulData::Positions`는 reference-bone space. `Data.Transform.TransformPosition`으로 월드 cm 좌표 복원. 반환 ComponentToWorld만 적용하면 잘못된 결과가 될 수 있다.
- 실제 시뮬레이션의 증거: enabled, not suspended, 유효 proxy, dynamic particles/iterations/substeps > 0, nonempty data, BlendWeight > 0, 정지 골격 상태에서 초기 settling 움직임. 스킨 fallback이나 고정 정점만으로 통과시키지 않는다.
- `GetSimulationTime()`은 누적 시간이 아니라 **솔버 CPU 비용(ms)**이다.
- finite 좌표, 일정한 particle count, 변위/속도 및 안정화 창의 RMS와 최대값을 기록한다. 충돌체 표면과의 거리는 별도로 검사한다. 유한한 위치라는 이유만으로 관통 없음으로 판정하지 않는다.
- `SetForcedLOD(1)`은 LOD0을 뜻한다. 포즈 reset 직후와 LOD 전환 직후를 안정화 통계와 섞지 않는다.
- 실제 솔버는 MaxDistance < 0.1cm를 kinematic으로 분류한다. 고정점도 관통 검사에 포함하고, 고정점을 통계에서 제외해 문제를 숨기지 않는다.
- TG_PostUpdateWork는 camera cache 갱신보다 늦다. 카메라를 움직인 다음 프레임에 screenshot을 요청해야 실제 front/side/back과 파일명이 일치한다.
- uncooked `-game`에서는 Editor Dataflow 노드 모듈이 로드되지 않아 missing terminal 로그가 생길 수 있다. 이 프로세스에서 graph를 저장하지 않는다. 패키지에서는 editor-only graph가 제거되는지 확인한다.
- 개발 probe에서 `p.ClothPhysics.WaitForParallelClothTask=1`, `p.ChaosClothAsset.WaitForAsyncClothInitialization=1`, `p.ChaosClothAsset.SimulationDataCheckFinite=1`, `p.ChaosClothAsset.SimulationDataResetOnNonFinite=0`를 이용할 수 있다. 비유한 값의 자동 reset이 오류를 숨기지 않게 한다. 에디터 검사는 원래 값을 복구한다.
- 실제 packaged GPU 화면은 `FScreenshotRequest::RequestScreenshot(AbsolutePath,false,false)`로 다음 viewport frame을 저장한다. 완료 delegate만 믿지 않고 파일과 크기도 검사한다. `UGameViewportClient::OnScreenshotCaptured`에 바인딩하면 기본 파일 저장을 차단하므로 두 경로를 혼용하지 않는다.

## 독립 probe 패키지

UAT: 번들 dotnet으로 `Engine/Binaries/DotNET/AutomationTool/AutomationTool.dll` 실행, 작업 디렉터리는 해당 AutomationTool 폴더. 번들 DOTNET_ROOT/PATH와 DOTNET_MULTILEVEL_LOOKUP=0 사용.

`BuildCookRun -project=D:/github/dress/LaceLoft.uproject -noP4 -unattended -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=D:/github/dress/Saved/ClothProbePackage -map=/Game/Development/ClothProbe/L_ClothProbe -AdditionalCookerOptions=-NoDefaultMaps`

`-map`이 MapsToCook를 지정하고 `-NoDefaultMaps`는 별도 추가되는 기본 맵을 제외한다. 의존 애셋은 여전히 포함되어야 한다. `-CookMapsOnly`는 맵 선택 인자가 아니다. 아직 실행한 명령이 아닌 조사 결과다.
