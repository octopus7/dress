# REQ-026 봉제 길이 실험 결과

최종 소스 재확인: `Runs/20261017-030947-370`에서 Editor 빌드·Create 12개·Reopen 12개가 경고 없이 성공했고 27개 애셋 해시와 18개 결과의 프로세스 간 위치 일치를 다시 확인했다. 아래 측정 JSON은 이 최종 Reopen 결과와 파일 해시까지 동일하다. 최종 실행에는 surface-only 진단이 포함돼 있다.

실행: 조정자의 UE 5.8 독립 호스트 실행. 에이전트 A는 빌드·UE 실행·커밋을 별도로 수행하지 않았다.

판정: 봉제 경계 대응·원본 보존·native 2D rest metric 물리 영향·저장/재열기·reset/replay는 아래 근거로 검증됐다. Equal100의 실제 표면이 오프스크린 캡처에서 누락되는 한계는 남아 있으며, 모든 케이스의 표면 렌더링을 검증 완료로 표시하지 않는다.

## 검증 상태

- 최초 RED: `PluginDev/GarmentFitClothProbeHost/Saved/Probe/REQ026-Red/Report/index.json`. 기존 회귀 2개 성공, 봉제 테스트 7개 의도된 실패.
- 구현 Editor 빌드: 조정자가 성공 확인. 첫 컴파일에서 봉제 소스 두 파일은 오류 없이 컴파일됐다.
- 최초 Create: `PluginDev/GarmentFitClothProbeHost/Saved/Probe/Runs/20261017-025255-243/Create-Report/index.json`. 전체 12개 테스트 성공(보고서 `succeeded=11`, `succeededWithWarnings=1`, `failed=0`, `notRun=0`), 그중 봉제 테스트 7개와 `Integration.SewnBodyPins` 성공을 직접 보고서에서 확인했다.
- 최종 전체 runtime 검증: `PluginDev/GarmentFitClothProbeHost/Saved/Probe/Runs/20261017-030302-984/`. `Build-console.log`의 `Result: Succeeded`와 Create/Reopen 보고서를 직접 읽었다. 각 프로세스에서 전체 **12개 성공, 경고 0, 실패 0, 미실행 0**이다.
- 같은 실행의 `CrossProcess.json`: **18개 metrics 전체 최대 정점 차이 0 cm**. `SourceAssetHashes.json`: **27개 원본 asset의 전후 SHA-256 동일**, 그중 Sewing asset 7개 모두 보존됐다. Reopen의 `Sewing/summary.json`도 2D rest metric 대조군 검증 성공을 기록한다.
- 전체 runtime 검증 이후 추가한 표면-only 캡처 진단: `Runs/20261017-030509-142/Reopen-Report/index.json`에서 봉제 **7개 성공, 경고/실패/미실행 0**을 확인했다. 이 후속 변경은 렌더 증거 수집만 추가하며 물리/원본 asset 설정을 바꾸지 않았다.

## 10초 측정값

근거 폴더: `PluginDev/GarmentFitClothProbeHost/Saved/Probe/Runs/20261017-025255-243/Create/Sewing/`. 각 `<Case>-metrics.json`에 전체 정점 배열과 아래 수치를 저장했다. cm 단위이며 strain은 `(실제 3D seam 길이 / 해당 패널의 원본 2D 길이) − 1`이다.

| 케이스 | 원본 A/B 길이 | 결과 seam 길이 | A strain | B strain | 마지막 1초 최대 step 이동 |
| --- | --- | --- | --- | --- | --- |
| Equal100 | 100 / 100 | 104.955185 | +4.9552% | +4.9552% | 0.000182 |
| Nonuniform100 | 100 / 100 | 105.638501 | +5.6385% | +5.6385% | 0.000113 |
| Reversed100 | 100 / 100 | 104.959126 | +4.9591% | +4.9591% | 0.000162 |
| Ratio100_150 | 100 / 150 | 121.266025 | +21.2660% | −19.1560% | **0.435583** |
| Ratio150_100 | 150 / 100 | 121.680346 | −18.8798% | +21.6803% | 0.000723 |
| Ratio100_150_Rest3D | 100 / 150 | 100.113369 | +0.1134% | −33.2578% | 0.000041 |

모든 케이스에서 원본 보존 검사 성공, 고정점 최대 오차 0 cm, 용접 seam 간격 0 cm, hard reset 오차 0 cm, 10초 replay 최대 차이 0 cm였다. 첫 10초 중 최대 step 이동은 5.723~6.445 cm다.

`Nonuniform100`은 A 원본 99개 정점에 4개를, B 원본 81개 정점에 6개를 뒤에 추가했다. 경계 매듭은 15개, 용접 후 정점은 175개이며 원본 인덱스와 2D 좌표를 유지했다.

## 2D 원본 길이의 물리적 영향

실제 초기 3D 배열의 최대 차이는 대조군마다 0 cm다. 동일한 배치와 topology를 유지하며 원본 2D 길이만 바꾼 결과:

- Equal100 대비 Ratio100_150: 최종 정점 최대 차이 **39.959679 cm**, seam 길이 차이 **16.310840 cm**.
- Equal100 대비 Ratio150_100: 최종 정점 최대 차이 **40.934082 cm**, seam 길이 차이 **16.725162 cm**.
- 동일한 100/150 원본의 2D rest metric과 3D rest metric: 최종 정점 최대 차이 **43.470055 cm**.

따라서 이 fixture의 비율 봉제는 2D 길이를 메타데이터로만 보존한 결과가 아니다. 두 패널의 충돌하는 2D rest metric이 native `FXPBDStretchBiasElementConstraints`에 들어가 공통 seam의 늘어남/줄어듦을 실제로 바꾼다. 3D rest metric 대조군은 100 cm 초기 배치 근처로 유지되어 원본 150 cm 패널의 길이 차이가 물리 rest 길이로 사용되지 않는 차이를 드러낸다.

## 화면과 해석 범위

`Ratio100_150-10s.png`를 직접 열어 실제 native cloth 표면, 청록/주황 패널 topology, 노란 seam, 녹색 고정점 및 길이 차이에 따른 주름과 패널 비대칭을 확인했다. 초기/1초/10초/reset PNG는 각 케이스에 있다.

최초 Create의 `Equal100-10s.png`는 선만 보이고 실제 표면이 빠져 있었다. PSO 준비와 메시 프록시 존재를 확인하는 공통 `Support::EnsureClothRenderReady`를 모든 봉제 Capture 시작에 연결했다. 그러나 후속 `20261017-030029-480/Create/Sewing/Equal100-10s.png`에서도 선만 보이는 것을 직접 확인해, 프록시 존재만으로 실제 표면 렌더링 성공을 주장하지 않는다. 추가 `*-10s-surface.png`는 topology debug 선을 제거한 실제 컴포넌트만 촬영하고, 로그에 LOD·재질·bounds를 기록한다.

`20261017-030509-142/Reopen/Sewing/`에서 이 진단을 실행했다. Equal100의 surface-only는 비어 있고 Ratio100_150의 surface-only는 실제 주름진 금색 천 표면이 보이는 것을 직접 확인했다. 두 케이스 모두 LOD 0, 재질 2개와 `CameraLitDoubleSided`, 유효한 cloth bounds를 사용한다. 조정자가 기본 CPU 스키닝 경로도 별도 실행했으나 표면 누락이 그대로여서 효과 없는 CPU override를 제거했다. 렌더링 원인은 확정하지 못했으며 추가 렌더러 조사로 범위를 확장하지 않았다. 최종 코드에는 원래 native 렌더 경로, 프록시 확인 및 surface-only 진단이 남는다.

따라서 wire overlay는 native solver 위치 증거로만 사용하며 표면 렌더링 증거와 구분한다. 최종 native runtime 근거는 `20261017-030302-984`, 후속 표면-only 진단 근거는 `20261017-030509-142`다. 전체 케이스 표면 표시나 Equal100 캡처 문제가 해결됐다고 주장하지 않는다.

**Ratio100_150은 10초 시점에도 진동이 남았다.** 마지막 1초의 최대 step 이동 0.435583 cm는 정한 1 cm/step 안정성 문턱 이내지만 정적 평형이나 완전 수렴을 의미하지 않는다. 현재는 유한 상태·관측 구간 안정성·부분 안정화가 검증된 것이며, 일반적인 이즈/주름 품질이나 양방향 완전 대칭은 보장하지 않는다. Ratio150_100의 작은 잔여 이동과 대조해 방향별 결과 차이도 그대로 기록한다.

물리 실험은 두 평면 패널의 명시적 경계 용접과 지정 초기 배치에 한정한다. 바디 충돌·self collision·면 관통·substep CCD·봉제 지퍼의 시간에 따른 닫힘·임의 의상 제작 UI·범용 피팅은 검증하지 않았다. 공통 seam은 네이티브 정점 용접이므로 양쪽의 3D seam 길이는 같다. 각 원본 2D 길이 대비 strain이 다른 것이 의도한 측정이다.

## 파일과 실행 계약

전용 구현은 `Private/Sewing/SewingProbe.h`, `SewingProbe.cpp`, `SewingExperiment.cpp`; 테스트는 `Private/Tests/Sewing/SewingTests.cpp`다. 생성 asset은 실험 플러그인 `Content/Sewing/CA_*.uasset`만 사용한다. 입력점은 `RunExperiment`, 통합용 읽기 전용 원본 제공은 `CreateIntegrationAsset`이다. 새 module dependency는 없으며, 표면 캡처 검증을 위해 조정자가 추가한 공통 `EnsureClothRenderReady` API를 사용한다.

실행 방법과 최종 build/run/commit 기록은 조정자 소유의 공통 재현 문서와 STATUS에 따른다. Reopen에서는 기존 source asset만 읽고 rebuild/save하지 않으며, 기대한 원본·refinement·rest metric·native weld가 실제 asset에 남았는지 다시 검사한다.
