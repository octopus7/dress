# REQ-026 봉제 경계 길이 실험

상태: 최초 RED 확인 후 전용 소스 구현과 native 물리 검증을 마쳤다. 조정자의 Editor 빌드와 Create/Reopen에서 전체 12개 테스트(봉제 7개 포함), 원본 asset 해시 보존 및 새 프로세스 결과 일치가 검증됐다. 일부 화면에서 천 표면 없이 topology 선만 보이는 렌더링 한계는 남아 있으며 수정 완료로 보고하지 않는다. 수치·근거·부분 안정화와 화면 한계는 [RESULTS.md](RESULTS.md)에 기록한다.

최초 RED 근거: 조정자 실행 `Saved/Probe/REQ026-Red/Report/index.json`. Editor 빌드 성공 후 기존 회귀 2개 성공, 봉제 테스트 7개는 의도된 미구현/positive control 지점에서 실패했다. 이 결과를 받은 뒤 구현했다.

## 계약

- 독립 `GarmentFitClothProbe`의 두 패널만 대상으로 한다. A는 패널 0, B는 패널 1의 연속된 경계 정점 목록이다.
- 대응은 원본 `Source2D`의 누적 호 길이로 계산한다. 각 경계의 정규화된 원본 매듭을 합쳐 양쪽에 같은 매듭을 만든다.
- 원본 2D 정점의 값과 인덱스는 유지한다. 추가 매듭은 배열 뒤에 붙이고 경계 삼각형만 분할한다.
- `EqualOnly`는 명시적인 cm 허용 오차로 길이 차이를 거부한다. `NormalizedArcLength`는 길이 차이를 허용하며 원본 길이·B−A·B/A를 별도로 보고한다.
- 네이티브 seam weld에 필요한 공통 3D 점은 A 경계를 기준으로 준비한다. 이것은 명시적인 초기 배치 연산이며 원본 2D 크기 변경이 아니다. 원본 입력은 const로 유지하고 최대 배치 변위를 결과에 보고한다.
- 역방향 경계 목록, 각 경계의 `bReverse`, B의 추가 `bFlipSecond`를 구분한다.

## 확인한 UE 5.8 소스

설치 엔진 `E:/WorkTemp/Epic Games/UE_5.8`을 읽기만 했다.

- `Engine/Plugins/ChaosCloth/Source/ChaosCloth/Private/ChaosCloth/ChaosClothConstraints.cpp`의 `CreateStretchConstraints`: pattern data와 `XPBDAnisoStretchStiffnessWarp`가 활성화되면 `FXPBDStretchBiasElementConstraints`를 생성한다.
- `Engine/Source/Runtime/Experimental/Chaos/Private/Chaos/XPBDStretchBiasElementConstraints.cpp`의 `InitConstraintsAndRestData`: `XPBDAnisoStretchUse3dRestLengths=false`에서 원본 2D UV 역행렬과 `(1,1,0)`의 rest metric으로 삼각형 변형을 제약한다. true에서는 3D 배치로 rest 길이를 계산한다.
- `Engine/Plugins/ChaosClothAsset/Source/ChaosClothAssetEngine/Private/ChaosClothAsset/ClothSimulationModel.cpp`: `BuildSimulationMesh`가 `PatternPositions`, `PatternIndices`, `PatternToWeldedIndices`를 native simulation LOD로 전달한다.
- 선택한 triangle stretch 경로는 엔진에 남아 있는 native XPBD 요소 제약이다. 최신 anisotropic spring 노드와 혼합하지 않으며 사용자 정의 솔버를 만들지 않는다.

## 검증 설계

7개 자동화: EqualLength, NonuniformRefinement, DirectionFlip, MismatchPolicy, InvalidBoundary, InvalidCorrespondence, NativeExperiment. 네거티브 입력 테스트에도 정상 입력 control을 먼저 둬 항상 실패하는 구현이 검증을 통과하지 못하게 했다.

NativeExperiment는 동일 길이·다른 샘플·반대 방향·1:1.5·1.5:1의 10초 시뮬레이션, 고정점 오차·seam 원본 대비 변형률·수렴·초기화/재실행, 실제 asset 저장/새 프로세스 재열기를 검사한다. 같은 prepared3D와 topology에서 2D 원본 길이를 바꾼 대조군 및 3D rest-metric 대조군으로 길이 차이의 실제 솔버 영향을 분리한다.

| Native case | 2D 원본 길이 A/B | 특징 |
| --- | --- | --- |
| `Equal100` | 100/100 cm | 동일 길이 기본 대조군 |
| `Nonuniform100` | 100/100 cm | A 11개, B 9개 불균등 경계 샘플; 경계 삼각형만 분할 |
| `Reversed100` | 100/100 cm | B의 원본 경계 방향과 3D 배치 방향을 역전 |
| `Ratio100_150` | 100/150 cm | 2D rest metric, 비율 1.5 |
| `Ratio150_100` | 150/100 cm | 2D rest metric, 비율 2/3 |
| `Ratio100_150_Rest3D` | 100/150 cm | 같은 원본/배치의 3D rest metric 대조군 |

각 케이스는 1/60초 × 600 step과 초기화 후 같은 600 step을 실행한다. 고정점은 초기 Y=0 행이며 MaxDistance=0이다. PBD edge/area spring과 3D tether는 비활성화하고 native XPBD triangle stretch와 angle bending을 사용한다. 현재 검사 기준은 고정점 오차 ≤0.01 cm, 한 step 이동 ≤30 cm, 마지막 1초 최대 step 이동 ≤1 cm, reset 오차 ≤0.001 cm, replay 차이 ≤0.05 cm, 용접 seam 간격 ≤0.0001 cm다. 수렴 기준은 관측 구간의 변위 기준이지 영구 평형 증명이 아니다.

비율 양방향은 동일 길이 대조군과, 1:1.5는 3D rest 대조군과 각각 최대 정점 위치 차이가 1 cm를 넘어야 한다. 양방향 seam 결과 길이도 동일 길이 대조군과 1 cm 이상 달라져야 한다. 이 대조군들의 초기 실제 3D 배열이 완전히 같은지도 검사한다. 하나라도 충족하지 않으면 길이 차이 지원을 검증하지 못한 것으로 실패한다.

`*-metrics.json`에는 원본 길이·비율, 준비 배치 변위, 최종 seam 길이, A/B 별 전체 및 구간 strain, 패널 edge 최대 절대 strain, 안정성·고정점·reset/replay, `initial_positions`/`final_positions`를 저장한다. 원본 2D prefix와 추가 정점, 삼각형, 용접 correspondence, explicit prepared3D, rest metric 설정은 실행 전후 모두 실제 asset collection에서 검사한다.

애셋은 `/GarmentFitClothProbe/Sewing/CA_<Case>`에 저장한다. Reopen 모드는 기존 애셋을 LoadObject로 읽고 재빌드/저장하지 않는다. 통합용 `CA_IntegrationEqual`은 별도 진입점이 독립 생성/읽기하므로 단독 Sewing 실행의 선행 여부에 의존하지 않는다. 이 원본은 9열×11행 두 패널, 11개 seam, 용접 후 187개 정점이다.

스크린샷은 공통 `Support::CapturePreview`로 실제 `UChaosClothComponent`를 촬영한다. 장식용 대체 천이나 별도 UI는 만들지 않는다. 실험에는 바디 충돌체·접촉·범용 봉제 이즈 품질·임의 의상 피팅 검증이 포함되지 않는다.

## 실행 책임

에이전트 A는 전용 소스·테스트·결과 문서만 작성한다. Editor 빌드, UE 실행, asset 생성/재열기, 최종 검증, Git 커밋은 조정자가 순차 수행한다. 결과 수치와 통과 여부는 실제 실행 근거를 받은 후 기록한다.
