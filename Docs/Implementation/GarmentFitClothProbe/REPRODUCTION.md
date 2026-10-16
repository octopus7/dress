# 최소 Chaos Cloth 실험 재현

REQ-024 전용 실험이다. 메인 GarmentFitLab의 단계 2나 실제 의상 생성 완료를 의미하지 않는다.

후속 REQ-026 봉제/바디 핀은 같은 독립 호스트에 추가했다. 아래 기존 명령의 기본 suite는 `Regression`으로 유지한다. 후속 기능까지 생성·새 프로세스 재열기·결과 비교를 실행하려면:

```powershell
& 'D:\github\dress\Tools\GarmentFitClothProbe\Run-Probe.ps1' -Mode All -Suite All
```

`-Suite Sewing`, `-Suite BodyPins`, `-Suite Integration`으로 각각 7개/2개/1개 자동화를 실행할 수 있다. `All` suite는 기존 회귀 2개를 포함한 정확한 12개 테스트 이름을 확인한다. 통합 suite는 봉제 asset을 별도로 확보한 뒤 바디 핀을 적용한 사본을 실행한다. `Create`/`All`은 선택한 기능의 전용 fixture 애셋을 재생성한다. `Reopen`은 애셋을 읽기만 하며 플러그인 안의 모든 `.uasset` 목록과 SHA-256 보존을 검사한다. 생성/재열기의 모든 `*metrics.json`에서 최종 정점 좌표 차이 ≤0.05cm를 확인하고 `CrossProcess.json`, `SourceAssetHashes.json`에 기록한다.

두 구현의 API·검사 범위·한계는 [Sewing/IMPLEMENTATION.md](Sewing/IMPLEMENTATION.md), [BodyPins/IMPLEMENTATION.md](BodyPins/IMPLEMENTATION.md)를 참고한다. 바디 사본은 [고정 snapshot 기록](BodyPins/SNAPSHOT.md)에 따라 독립 경로에 저장했다. 실제 마네킹 사용 허용은 REQ-026 핀 실험에 한하며 아래 REQ-024 원래 범위를 확대 해석하지 않는다.

## 실행

UE 5.8.3 설치와 Visual Studio C++ 빌드 도구가 필요하다. PowerShell에서:

```powershell
& 'D:\github\dress\Tools\GarmentFitClothProbe\Run-Probe.ps1' -Mode All
```

`All`은 독립 Editor 빌드, 고정 실험 애셋 생성/재생성, 2개 자동화, 새 UE 프로세스 재열기와 동일 자동화, 생성/재열기 정점 결과 비교를 순서대로 실행한다. `Create`/`All`은 아래 세 전용 실험 애셋을 고정 입력으로 재생성하므로 수동 편집 보관용으로 사용하지 않는다. 기존 애셋을 수정하지 않고 검사만 하려면:

```powershell
& 'D:\github\dress\Tools\GarmentFitClothProbe\Run-Probe.ps1' -Mode Reopen
```

엔진 위치가 다르면 `-EngineRoot 'E:\WorkTemp\Epic Games\UE_5.8'`를 실제 위치로 바꾼다. `Build`와 `Create`도 개별 실행 가능하다. 빌드는 `-WaitMutex -MaxParallelActions=4`를 사용한다. 실행 결과는 각 호출의 독립 `PluginDev/GarmentFitClothProbeHost/Saved/Probe/Runs/<시각>/`에 남는다. 다른 UE/빌드 프로세스를 종료하거나 공용 캐시를 삭제하지 않는다.

호스트: `D:/github/dress/PluginDev/GarmentFitClothProbeHost/GarmentFitClothProbeHost.uproject`.

원본 애셋(커밋 대상):

- `Plugins/GarmentFitClothProbe/Content/Experiment/CA_Free.uasset`: 무충돌 Cloth.
- `Plugins/GarmentFitClothProbe/Content/Experiment/CA_Sphere.uasset`: 동일 원본에 구 충돌을 지정한 Cloth.
- `Plugins/GarmentFitClothProbe/Content/Experiment/PA_Sphere.uasset`: 기본 Root 골격에 구 하나.

위 상대 경로는 독립 호스트 기준이다. 루트 프로젝트나 기존 플러그인을 참조하지 않는다. 별도 레벨/WBP는 없으며 실행 때 `FPreviewScene` 임시 월드를 사용한다. 애셋은 독립 호스트의 Content Browser에서 Plugin Content를 표시해 `/GarmentFitClothProbe/Experiment`에서 열 수 있다.

## 입력과 네이티브 경로

- `Probe.cpp::MakeInput`: 패널마다 로컬 2D 범위 `(0,0)..(50,100)` cm, 5cm 간격 11×21 정점과 400 삼각형. 두 패널의 2D 원본은 별개다.
- 초기 3D: 첫 패널 X=-50..0, 둘째 X=0..50, 공통 Y=-50..50, Z=80cm. X=0의 21쌍이 이미 맞닿아 있다.
- `FCollectionClothSimPatternFacade::Initialize`로 원본을 만들고 `FCollectionClothSeamFacade::Initialize`로 봉제 경계를 연결한다. 462개의 2D 정점을 유지하고, 21쌍을 공유하는 441개의 3D 정점이 된다. 실제 시뮬레이션 중 두 봉제 끝은 같은 입자를 참조한다. 이는 떨어진 옷본을 당겨 모으는 봉제 솔버가 아니다.
- Y=-50의 21개 정점을 `MaxDistance=0`으로 고정한다. 나머지 420개는 `MaxDistance=200cm`, 네이티브 geodesic tether를 사용한다. 모든 정점은 기본 Root 골격에 연결하며 캐릭터/스킨 전송은 없다.
- 설치 엔진의 기본 config Dataflow 노드가 사용하는 `FClothingSimulationConfig::Initialize`/property collection 경로로 설정한다. 별도 Dataflow 그래프를 저장하지 않고 `UChaosClothAsset::Build`에 직접 Collection을 전달한다. 시뮬레이션은 실제 `UChaosClothComponent`와 엔진 Chaos Cloth solver가 수행한다.
- 중력 `(0,0,-980.665)` cm/s², bending 0.05, damping 0.05, collision thickness 1cm, friction 0.3, 4 substeps, 8 iterations. 각 외부 step은 1/60초이며 600 step=10초다. 같은 호스트 안에서 생성/재열기 모두 동일하다.
- 구 중심 `(0,-40,40)` cm, 반지름 20cm. 렌더용 구는 충돌을 끈 표시물이다. 솔버 충돌은 `PA_Sphere`의 하나뿐이다.

설치 소스 확인 위치:

- `Engine/Plugins/ChaosClothAsset/Source/ChaosClothAsset/Public/ChaosClothAsset/CollectionClothSeamFacade.h`
- `Engine/Plugins/ChaosClothAssetDataflowNodes/Source/ChaosClothAssetDataflowNodes/Private/ChaosClothAsset/SimulationDefaultConfigNode.cpp`
- `Engine/Plugins/ChaosClothAsset/Source/ChaosClothAssetEngine/Public/ChaosClothAsset/ClothAsset.h`
- `Engine/Plugins/ChaosClothAsset/Source/ChaosClothAssetEngine/Public/ChaosClothAsset/ClothComponent.h`

관련 공식 API: [CollectionClothFacade](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosClothAsset/FCollectionClothFacade), [CollectionClothSeamFacade](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ChaosClothAsset/FCollectionClothSeamFacade). 구현 판단은 실제 설치된 5.8.3 소스를 기준으로 했다.

## 검사와 결과 읽기

`GarmentFitClothProbe.InputValidation`은 유효 입력을 허용하고 패널 누락, 봉제/삼각형 인덱스 오류, 퇴화 2D/3D 삼각형, 떨어진 봉제 끝, 중복 대응, 원본 배열 수 불일치와 빈 봉제선을 거부한다. 입력 검증에는 유한 좌표와 연속 경계선 검사도 포함된다.

`GarmentFitClothProbe.NativeTenSecondsAndReset`은 아래 항목을 검사한다.

1. 저장된 원본의 2D/초기 3D, 패널/삼각형/정점 수, 봉제선 공유 인덱스, 하나뿐인 구와 무충돌 대조군.
2. 매 1/60초의 유한 좌표, 1000cm 밖으로 이탈 여부, 고정점 오차 ≤0.01cm, 최대 edge 길이 비율 ≤1.25, 최대 step 이동 ≤20cm, 마지막 1초 최대 step 이동 ≤1cm. 자유 영역 평균 낙하 ≥10cm.
3. 구를 켠 경우 정점 기준 최대 관통 ≤0.25cm, 표면에서 1.5cm 이내인 접촉 표본이 적어도 하나 존재. 끈 경우 같은 구 내부로 2cm 이상 들어가고 두 결과의 최대 위치 차이가 2cm 이상이어야 한다.
4. `RecreateClothSimulationProxy` hard reset 직후 원본 3D 오차 ≤0.001cm, 같은 600 step 재실행 결과의 최대 정점 차이 ≤0.05cm. 시뮬레이션은 Collection 원본을 덮어쓰지 않는다.
5. 재열기는 새 프로세스에서 기존 Cloth/Physics 애셋을 읽으며 생성 경로를 호출하지 않는다. 스크립트가 전후 세 애셋 SHA-256의 동일성과 생성/재열기 결과 차이 ≤0.05cm를 확인한다.

`Create-Report/index.json`, `Reopen-Report/index.json`의 `failed=0`, `notRun=0`, 성공 합계 2를 검사한다. UE 프로세스 종료 코드가 0이어도 자동화 실패가 있을 수 있어 보고서를 따로 확인한다. 원시 로그, PNG, 정점별 JSON은 `Saved/`의 생성물이며 커밋하지 않는다.

각 `Create/`, `Reopen/` 아래:

- `free-initial.png`, `sphere-initial.png`: 초기 배치.
- `free-10s.png`, `sphere-10s.png`, `sphere-10s-back.png`: 10초 변형.
- `free-reset.png`, `sphere-reset.png`: hard reset 직후.
- `free-metrics.json`, `sphere-metrics.json`: 수치, 초기/최종 좌표.
- `summary.json`: 충돌 대조군 결과와 재열기 여부.

화면의 청록/주황 선은 두 패널, 노랑은 봉제선, 초록은 고정점, 분홍 선은 구이다. 표면은 실제 Cloth Component, 선은 네이티브 솔버 좌표를 이용한 검사 오버레이다. 뷰포트가 없는 offscreen 렌더이므로 새 제품 UI를 만들지 않는다.

## 한계

충돌 측정은 외부 step의 정점 표본 검사다. 삼각형 전체의 관통, substep 사이의 연속 충돌, 자가 충돌, 면의 뒤집힘, 실제 의상의 착장 품질은 검증하지 않는다. 1m 크기 단순 패널의 10초 안정성은 인형 크기 의상·다양한 재질·메시 해상도·애니메이션에 대한 보증이 아니다. 같은 설치/머신의 재현 오차만 비교하며 다른 GPU/엔진/플랫폼의 비트 단위 재현은 요구하지 않는다.

실제 캐릭터, Luna, 바디 분석, 셔츠/원피스, 일반 피부 보정, 별도 솔버, 제품 UI/통합, 패키징/cook은 범위 밖이고 실행하지 않는다. 메인 LaceLoft나 LaceGarment 자동화도 이 독립 호스트 실험의 통과로 대체되지 않는다.
