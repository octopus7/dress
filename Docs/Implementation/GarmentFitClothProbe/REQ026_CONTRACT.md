# REQ-026 병렬 작업 공통 계약

기준: 47f5dc7, 승인 계획 32e1779. 현재 checkout의 세 실험 경계를 사용하고 새 대화/브랜치를 만들지 않는다. 공유 REQUESTS에는 서로 다른 REQ-026 제목이 관찰되므로 이 작업은 **Cloth 봉제선 길이 처리와 바디 핀** 제목과 해당 계획으로 식별한다. 공유 요청 문서는 수정하지 않는다.

## 소유권

- A: 플러그인 `Private/Sewing/`, `Private/Tests/Sewing/`, `Content/Sewing/`, `Docs/Implementation/GarmentFitClothProbe/Sewing/`.
- B: `Private/BodyPins/`, `Private/Tests/BodyPins/`, `Content/BodyPins/`, `Docs/Implementation/GarmentFitClothProbe/BodyPins/`. 고정 마네킹 사본/임포트도 B가 담당한다.
- 조정자: 기존 Probe.h/.cpp, 새 ProbeSupport.h/.cpp, 실행 연결/공통 도구/Build.cs/STATUS/REPRODUCTION/이 계약. 공용 빌드·에디터·Git 커밋은 조정자만 순차 실행한다.

## 공통 API

`Probe.h`의 `FPanel`, `FInput`은 유지한다. 새 `FVertexRef{PanelIndex,VertexIndex}`는 패널 로컬 2D 원본 정점 식별자다. 봉제 재샘플링은 기존 정점 순서를 유지하고 새 정점을 뒤에 추가한다. 용접된 3D 정점은 Collection의 패널 offset과 2D→3D lookup을 통해 해석한다. 핀 충돌 판정은 이 용접된 3D 식별자를 기준으로 한다.

`FRunContext{OutputDirectory,bReopen}`는 전용 절대 결과 경로와 새 프로세스 읽기 모드를 전달한다. `ProbeSupport.h`의 공통 엔진 연산은 조정자가 구현한다. 에이전트는 공통 파일을 수정하지 않고 요구 변경을 조정자에게 전달한다.

A 공개 진입점은 `Sewing/SewingProbe.h`의 `GarmentFitClothProbe::Sewing::RunExperiment(const FRunContext&, FString&)` 및 `CreateIntegrationAsset(const FRunContext&, FString&) -> UChaosClothAsset*`다. 순수 봉제 대응/재샘플링 API는 A가 해당 헤더 안에 설계한다.

B 공개 진입점은 `BodyPins/BodyPinProbe.h`의 `GarmentFitClothProbe::BodyPins::RunExperiment(const FRunContext&, FString&)`, `RunCombined(UChaosClothAsset*, const FRunContext&, FString&)`다. 입력 asset은 읽기 전용 원본이며 B가 자기 패키지로 복사/재빌드한다. 핀 API/직렬화 타입은 B가 자기 헤더에서 정의한다.

자동화 이름은 A `GarmentFitClothProbe.Sewing.*`, B `GarmentFitClothProbe.BodyPins.*`, 조정자 `GarmentFitClothProbe.Integration.*`를 쓴다. 기존 두 회귀 테스트는 유지한다. 각 에이전트는 테스트를 먼저 작성한 뒤 조정자에게 최초 RED 실행을 요청한다. 에이전트 자체의 공용 빌드/UE 실행/Git 커밋은 금지한다.

마네킹은 안정 커밋 `69583eb`의 `Content/GarmentFitLab/Source/SKM_LunaMk2_Mannequin.fbx`를 고정 사본으로 추출해 독립 경로에 임포트하는 방식을 우선한다. 원본 FBX Git blob은 `8d7916124c01666172a92bbb4b424dfbaa9d0237`이다. 메인 원본 및 진행 중인 비율 보정은 실행 의존으로 삼지 않는다.

## 사전 인터페이스 점검

| 작업 조합 | 공유 접점 | 판단 |
| --- | --- | --- |
| A↔B | 봉제 후 패널 정점 식별 | 기존 정점 순서 보존, 네이티브 lookup으로 용접 후 핀 해석 |
| A↔조정자 | 네이티브 collection/config/build | 원본 2D 길이 보존. 실제 솔버가 어느 rest metric을 쓰는지 검사해 길이 차이를 물리적으로 실증 |
| B↔조정자 | 실행·독립 마네킹·핀 데이터 저장 | B 고유 패키지/본문 데이터, 조정자 단독 실행. 강제 핀을 자연스러운 피팅으로 판단하지 않음 |
| A 자체 | 재샘플링과 원본 보존 | 선형 경계 분할은 원본 형상을 보존하며 기존 점 수정/암묵 스케일 금지 |
| B 자체 | 고정/추종/해제/저장 | 바디 이동·회전과 월드 고정을 구분. 골격 포즈 지원은 실제 검사한 경우만 보고 |

진행: 공통 계약 확정 후 두 Astra/xhigh 구현 에이전트가 병렬 구현을 마쳤다. 조정자가 공통 지원 코드와 두 기능의 테스트/stub를 포함해 Editor 빌드를 실행해 성공했다 (`Saved/Probe/Runs/20261017-024217-314/Build.log`). 최초 RED는 `Saved/Probe/REQ026-Red/Report/index.json`: 기존 회귀 2개 성공, 봉제 7개/바디 핀 2개는 의도한 미구현 positive control에서 실패했다. 이 증거를 전달한 뒤 본 구현을 시작했다. 공용 실행 도구의 기본 suite는 기존 Regression 2개이며, 기능별 suite와 All을 명시적으로 선택한다. 전체 12개 자동화와 새 프로세스 재열기, 27개 원본 해시 보존, 18개 최종 위치 결과 일치를 확인했다. 검토에서 발견한 저장된 핀 lifecycle 검증 누락도 RED 재현 후 수정했다. 일부 표면 캡처 한계는 남겨 명시했다. 최종 증거·측정·커밋은 [STATUS](STATUS.md)와 기능별 RESULTS에 기록한다.
