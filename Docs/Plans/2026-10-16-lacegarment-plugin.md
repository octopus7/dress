# LaceGarment UE 5.8 Implementation Plan

> 실행: `superpowers:executing-plans` 방식으로 이 구현 대화에서 순차 실행. 승인 근거 REQ-012, 현재 범위 REQ-017. 기존 승인 설계를 다시 승인받는 절차는 반복하지 않는다.

**Goal:** 이번 구현 대화에서는 **Task 1~3**의 독립 호스트·패널, 봉제·안정화·Cloth 생성, 바디·대표 의상·문서 모델을 구현·검증한다. Task 4~7은 후속 범위로 보존하며 이번에는 실행하거나 완료 조건으로 요구하지 않는다.

## 이번 대화의 완료 판정 — REQ-017

- 활성 태스크는 1~3 세 개이다. 완료 수와 작업량 진척은 이 범위를 기준으로 산정하며 전체 제품 완성과 구분한다.
- 남은 실제 티셔츠·돌핀쇼츠의 피팅, 충돌·변형·UV·스킨·기본 포즈 검증을 계속한다. 작은 패널 성공만으로 대표 의상까지 완료 처리하지 않는다.
- 독립 호스트의 API·자동화·진단 commandlet에서 문서 편집/생성 경로를 검증할 수 있다. 마우스 저작 UI와 Designer 인수는 이번 완료 조건이 아니다. C++ 위젯 트리로 UI를 대체하지 않는다.
- `.lgarment` 원본 저장·열기는 제외한다. Task 2의 실제 Cloth/재질/Physics 등 UE 애셋 저장·재열기와 runtime 검증은 유지한다.
- 호스트 Editor 빌드·현재 범위 자동화·대표 의상 품질 검증은 계속한다. BuildPlugin 배포, 새 Blueprint/C++ 프로젝트 반복 인수, cook·Development/Shipping은 이번 완료 조건이나 기본 반복 검증으로 요구하지 않는다.
- 전체 제품 관문 A는 UI·새 프로젝트 항목도 포함하므로 이번 기술 경로 완료와 동치가 아니다. B/D/E/F 및 전체 A–F 통과를 이번 완료의 선행조건으로 삼지 않는다. 기존 선행 검증·산출물은 보존한다.

**Architecture:** `PluginDev/LaceGarmentHost/Plugins/LaceGarment/`의 Editor 모듈이 문서·생성·저장·검사·배포를 담당한다. 런타임은 엔진 기반 콘텐츠와 Chaos Cloth만 사용한다. UI는 저장된 WBP/EUW 애셋이다.

**Tech Stack:** UE 5.8.3 (CL 58210709, Win64 Editor BuildId 55116800), C++, UMG, Chaos Cloth, GeometryCore, JSON/ZIP/GLB.

**Spec:** `Docs/Design/LACE_GARMENT_PLUGIN_DESIGN.md`, `Docs/Design/LACE_GARMENT_PACKAGE_FORMAT.md`, `Docs/UMG_GUIDELINES.md`.

## Global Constraints

- 루트 Source/Content/Config/.uproject와 기존 게임 STATUS를 수정하지 않는다. 공유 저장소의 브랜치·스테이징을 바꾸지 않는다.
- Blender, 외부 솔버, 서버에 의존하지 않는다. 설치형 엔진의 로컬 소스로 API를 확인한다.
- UI 트리를 C++에서 생성하지 않는다. 모든 UI는 실제 Designer 편집 가능한 애셋이다.
- 실험용 작은 패널 성공은 관문 A 전체 성공이 아니다. 초안/실험/통과를 분리 기록한다.
- 전용 스크립트 Tools/LaceGarment/, 산출물 Dist/LaceGarment/, 근거 Docs/Implementation/LaceGarment/.
- 순차 빌드. 다른 대화의 프로세스를 종료하지 않는다. 명시적 통합 요청 전 게임에 연결하지 않는다.

## Review Focus

1. 자기 교차·퇴화·NaN·복수 루프의 옷본은 생성 오류를 구체적으로 반환해야 한다(작업 1/3).
2. 초기 배치, 수렴 결과, 2D rest를 혼동하거나 AddStitch만으로 수렴으로 판정하지 않는다(작업 2).
3. topology 변경 때 문서의 수동 보정과 대응이 미해결로 남고 낡은 결과를 성공으로 처리하지 않는다(작업 3). 파일/배포 연결은 후속 범위이다.
4. ZIP 경로 탈출·중복·과도한 압축·해시/필수기능 불일치 검사는 후속 작업 4에 보존한다. 이번 완료 조건은 아니다.
5. GUI 없는 cook에서 soft reference 팩 포함과 Editor 콘텐츠 제외는 후속 작업 6/7에 보존한다. 이번 완료 조건은 아니다.

## Task 1: 독립 호스트와 패널 생성

**Files:** `PluginDev/LaceGarmentHost/LaceGarmentHost.uproject`, `Source/LaceGarmentHost*`, `Plugins/LaceGarment/{LaceGarment.uplugin,Source/LaceGarmentEditor/}`, `Tools/LaceGarment/Verify.ps1`.
**Interfaces:** `LaceGarment::FPanel` (안정 ID, mm 경계, cm 초기 변환), `FPanelMesh` (2D rest/3D initial/indices/UV), `BuildPanel(const FPanel&, FPanelMesh&, FString&)`.

- [x] 자동화 테스트를 먼저 추가: 20×30mm 패널의 면적·단위·삼각분할, 오목한 조각, 자기교차/퇴화 거부, 결정적 출력.
- [x] 최소 실패 구현으로 Editor 빌드 및 `LaceGarment.Geometry` 실행, 예상 실패 기록.
- [x] 로컬 GeometryCore API 기반 삼각분할과 내부 해상도 생성. 옷본 원본을 유지하고 결과를 분리한다.
- [x] 독립 호스트 Editor 빌드와 전체 `LaceGarment` 자동화 실행. 기대: 테스트 통과, 로그 기록.

## Task 2: 실제 크기의 봉제·안정화·Cloth 경로

**Files:** `Private/LaceGarmentPipeline.cpp`, `Public/LaceGarmentPipeline.h`, `Private/Tests/LaceGarmentPipelineTests.cpp`, `Private/LaceGarmentProbeCommandlet.cpp`.
**Interfaces:** `FBuildInput`은 패널·봉제 대응·충돌 프록시·고정 가이드, `FBuildResult`는 rest/initial/settled 위치·UV·skin·수렴 측정값을 분리한다. `BuildGarment`와 `SaveClothAsset`은 실패 시 오류를 반환한다.

- [x] 두 패널과 명시적 경계 봉제의 미완성 테스트를 실행한다.
- [ ] 봉제 연결·UE 내부 안정화·충돌·고정 가이드·수렴 기준을 구현한다. 15cm 더미 기준의 cm 단위를 유지한다.
- [ ] 고유 UV 패킹과 밀도/padding 측정, 정규화된 skin, Cloth Collection/Asset 생성·저장·재열기를 구현한다.
- [ ] 작은 패널의 실제 애셋을 재열고 초기 위치와 수렴 위치 차이, 봉제 거리·관통·면 뒤집힘·runtime sim을 측정한다. 실패면 관문 A 확장을 보류한다.

## Task 3: 바디·대표 의상·문서 모델

**Files:** `Public/LaceGarmentDocument.h`, `Private/LaceGarmentDocument.cpp`, `Private/LaceGarmentTemplates.cpp`, `Content/Authoring/`, 대응 Tests.
**Interfaces:** 안정 ID의 piece/control/edge/seam, cubic curve·시접·결·notch·배치·생성 설정·수동 보정 원본. `InvalidateDerived`는 파생 캐시와 보정 해석 상태를 갱신한다.

- [ ] 소매/쇼츠 길이 변경, 잘못된 ID, 봉제 방향/여유 길이, topology 보정 무효화 테스트를 먼저 실행한다.
- [ ] 독립 흰 더미·골격·UV·충돌 및 티셔츠/돌핀쇼츠 원본을 만들고 출처·치수·고유 UV 근거를 저장한다. 작성자·이용조건은 REQ-013에 따라 미정으로 두고 임의 확정하지 않는다.
- [ ] 빈 조각·곡선·봉제·변환·Undo/Redo·웨이트/천 맵 보정의 문서/API 경로를 구현한다. 사용자용 저작 UI는 후속 Task 5이다.
- [ ] 실제 의상과 기본 포즈를 검증하고 이번 기술 범위의 결과를 기록한다. UI·새 프로젝트 인수를 포함하는 전체 관문 A 통과와 구분한다.

## Task 4: 독립 원본 파일 — 후속 범위, 이번 실행 제외

**Files:** `Private/LaceGarmentArchive.cpp`, `Private/LaceGarmentCanonicalJson.cpp`, `Private/LaceGarmentGlb.cpp`, 대응 Public/Tests.
**Interfaces:** `SaveDocument`/`OpenDocument`는 계약의 ZIP payload와 JCS/SHA-256, 버전·좌표·bodyProfile·authoring/topology/builtWith 상태를 검증한다. 실패한 읽기는 열린 문서를 변경하지 않는다.

- [ ] 계약 예제와 RFC 8785 벡터, 손상/경로 탈출/중복 키/NaN/외부 URI/한도 초과/낡은 validated 거부 테스트를 먼저 실행한다.
- [ ] draft와 validated, GLB/PNG/기준 바디 포함, source/derived 구분, topology 미해결 보정을 구현한다.
- [ ] 다른 독립 호스트에서 제어점·봉제 수정→재생성→저장 왕복을 실행한다. 기대: ID·단위·골격·UV·웨이트 의미 보존(관문 B).

## Task 5: 실제 저작 WBP/EUW — 후속 범위, 이번 실행 제외

**Files:** `Content/UI/Editor/`의 EUW/WBP, `Public/LaceGarmentWorkbench.h`, 입력·문서 연결 구현. 애셋 작성용 전용 스크립트는 Tools/LaceGarment/.

- [ ] 저장된 애셋 존재/필수 바인딩/부모 클래스/런타임 참조 격리 테스트를 먼저 실행한다.
- [ ] 승인 설계의 조각·봉제 목록/2D·3D 뷰/속성/검사/열기·저장·Undo·생성·배포를 실제 애셋에 구성한다.
- [ ] Designer에서 계층·예제 옷본·스타일을 확인하고 Compile/Save. 실제 마우스 작업과 새 빈 조각 생성 확인.

## Task 6: 콘텐츠 팩·선컴파일 배포·cook 등록 — 후속 범위, 이번 실행 제외

**Files:** `Private/LaceGarmentDistribution.cpp`, `Private/LaceGarmentCookRegistry.cpp`, `Content/Runtime/`, `Tools/LaceGarment/Package.ps1`, `Dist/LaceGarment/`.

- [ ] /Game/, /Script/LaceLoft, Editor 참조, 누락 파일, ID 충돌을 배포 전에 차단하는 테스트 실행.
- [ ] 불변 PackId의 콘텐츠 플러그인과 엔진 기반 의상 정의/카탈로그 생성. commandlet 로딩에서 AssetManager에 재귀 AlwaysCook 등록, 도구 콘텐츠 제외.
- [ ] BuildPlugin 산출물과 재편집 원본을 배포 ZIP으로 패키징. BuildId 우회 금지.
- [ ] 동일 표시 이름의 다른 팩, 업데이트/재시작, soft reference cook을 시험(관문 D/E).

## Task 7: 새 프로젝트 인수 — 후속 범위, 이번 실행 제외

- [ ] Blank Blueprint/C++ 프로젝트에서 ZIP 설치 후 라이브러리를 열지 않고 cook.
- [ ] C++ 컴파일러 없는 Editor에서 생성·수정·재열기·배치 확인. 환경을 증거와 함께 기록.
- [ ] Development/Shipping 빌드·실행·cloth sim과 Editor UI/모듈 제외 확인(관문 F).
- [ ] 구현 검토 및 전체 `LaceGarment` 자동화, 인수 근거를 STATUS에 기록. 미실행을 통과로 적지 않는다.

## Execution Ledger

- 사용자 최종 지시: 작업을 중단하고 추가 빌드·테스트 없이 중간 커밋한다. 이 지시가 이전의 이 대화 stage/commit 금지를 대체한다. 중단 전 마지막 검증은 BendingControl-Green Editor 빌드·49/49(오류/경고0)이며 실제 쇼츠/티셔츠 진단은 여전히 실패했다. Task 1~3은 미완료로 보존하고 후속 구현·검토를 중단한다. 상세 결과와 남은 항목은 플러그인 STATUS에 기록한다.

- 2026-10-16: 기존 설계/실행 승인 REQ-012 확인. 모든 요청이 기존 기록에 포함되어 중복 REQ는 추가하지 않는다.
- Pre-flight: Task 1의 mm rest와 cm initial을 Task 2가 소비한다. 파일 단계에서만 m/GLB 좌표 변환한다. Task 3 문서 원본이 Task 4 해시와 Task 5 UI의 유일한 기준. Task 6은 Task 4 검증 원본과 Task 3 출력만 배포한다.
- 실행 환경: 공유 저장소에 대부분 파일이 미추적 상태. 브랜치/커밋 기반 스킬 스크립트 대신 이 원장과 전용 로그를 사용하며 타 작업을 스테이징하지 않는다.
- Task 1: Geometry-Red-2 → Geometry-Green, 3/3 통과. 코드와 테스트는 플러그인 전용 경로.
- Task 2 진행: Pipeline-Red → Pipeline-Green 6/6. Capsule-Upright 실패 → Capsule-SmallScaleStep 7/7. 수렴 결과와 제한을 STATUS에 기록.
- Task 2 결정: 외부 솔버 없이 Editor 모듈 내부 constrained relaxation을 저작 안정화에 사용하고, 저장 결과의 런타임은 엔진 Chaos Cloth로 별도 검증한다. 단순 패널 성공은 대표 의상/관문 A 통과가 아니다.
- Task 2 후속: 입력 좌표/예산/Collection 구조 회귀 수정, 실제 렌더의179.6도 접힘 회귀를52.5도로 개선, 면-캡슐 거리·면적비 검사 추가. 저장과 새 프로세스 재열기에서 원본 형상/UV/skin/고정점 동일성 및240frame runtime 검사 실행.
- Task 3 바디 선행: 고정 Resources 스냅샷의 독립 UE import,15cm/20bones/19Physics bodies. UE 출력의 독립 UV audit는73628faces/16charts/면적겹침0. 자동 Physics의 전체 착장 적합성은 아직 미검증.
- Skin 결정: SurfaceVertex와 BarycentricStrict를 명시적으로 구분한다. 전자는 선택 표면 정점의 전체 원래 영향을 복사하고, 후자는4초과 영향을 잘라내지 않고 수동 재분배 오류를 반환한다. 실제 의상의 경계·포즈 품질은 후속 검증.
- BodySkin-Probe-1 / BodySkin-Probe-Reopen-1: 별도 Cloth의20bone reference와 sim/render skin 동일성·240frame runtime·렌더 통과. 여전히 두 패널 프로브이며 대표 의상 관문 전체 통과가 아니다.
- Task 3 문서 시작: 안정 ID/직선·cubic/봉제 구간/보정 topology/Undo-Redo의 미구현 회귀를 먼저 실행. UI·파일 archive와 아직 연결하지 않음.
- Task 3 문서→생성: 곡선 arclength, 부분 봉제/ease, notch의 방향·짝 대응, 원본 ID 고정점 구현. 길이가5% 다른 경계의 native drape 통합 확인. 검토에서 찾은 성긴 샘플의 원곡선 교차 누락 및 arc/chord 길이 허용치 차이는 회귀 재현 후 수정. CurveReview-And-InnerLoops 29/29. 안쪽 루프 메시 코어는 구현했고 문서 연결은 이어서 진행.
- REQ-013: 작성자 표시·이용조건 미정, 라이선스 임의 추가 금지와 향후 비공개 전환 예정은 AGENTS.md에 반영. 사용자 결정 없이 배포 이용조건을 확정하지 않음.
- 독립성 조기 검사: FreshBlueprint-Checkpoint-1의 BuildPlugin UHT Category 오류 수정 후 Checkpoint-2에서 선컴파일·새 Blueprint 프로젝트30/30·Cloth재열기 통과. D/F 관문 전체는 미통과. 새 C++/컴파일러 없는 환경/cook/게임 패키지는 미실행.
- Task 3 대표 의상 진단: stable ID 티셔츠/쇼츠 draft, 실제19개 body collider 추출, 목표 고정점을 추가해34/34 자동화. 실제 티셔츠는 아직 실패. 원본 head proxy 과대 범위는 실제 head-dominant 표면을 포함하는 sphere로 수정했으나, 소매 치수와 중첩 collider 투영 문제가 이어서 확인됨. Fit-4의 저장 좌표에서 재현한 CollisionUnion-Red 34/35를 수정 중이며 수렴 기준을 완화하지 않는다.
- Task 2/3 후속: 중첩 capsule 출구 투영과 연속 이동 충돌 회귀를 수정해36/36. 실제 팔·spine 표면을 보존하는 측정 capsule로 갱신. Fit-11에서도 봉제 간격0.697cm·변 길이 오차408.8%로 대표 의상은 실패. 충돌체/고정점/초기 배치의 구조적 적합성을 조사하며 관문 A를 완료하지 않는다.
- FreshProjects-Checkpoint-3: 최신0.3.0-dev 선컴파일, 새 Blueprint/C++에서 각각36/36 및 저장 Cloth 재열기 통과. 새 C++ Editor도 빌드됨. 컴파일러 없는 환경·cook·Development/Shipping은 여전히 미실행.
- 목길이 불가능 조건의 독립 검토 및 회귀: NeckLength-Red36/37 → 뒤목 깊이 수정 후 Green37/37. Checkpoint-4의 새 Blueprint/C++에서도 각각37/37와 Cloth 재열기 통과. Fit-12는 봉제 간격·관통0으로 개선되었으나 변형 기준 실패. 이후 진단 문서 재실행으로 원본의 한 조건씩 비교하고 있으며 기본 템플릿을 임의로 연속 조정하지 않는다.
- REQ-017 적용: 앞으로 Task 1~3과 호스트/UE 애셋 품질 검증만 진행. 전달 전에 시작한 Checkpoint-5는 성공 결과를 보존하고 새 프로젝트 반복 검증은 종료한다.
- Task 2 충돌 확장: capsule만으로 근사한 실제 어깨/겨드랑이의 공간 손실이 계속 확인됨. UE Cloth가 native convex를 지원하므로 기존 capsule을 보존하면서 convex 생성·접촉·면 검사를 회귀부터 추가한다. 외부 도구는 사용하지 않는다. 런타임과 같은 FConvex 거리/두께 sweep을 사용하며, 평면 이동으로 둥근 두께 경계를 과대 근사하지 않는다. 실제 저장된 cooked geometry·bone-local 좌표·단위 scale·재열기 검증 후 바디에 적용한다.
- Task 2 convex 하위 경로: float 정밀도에서 hull을 만들고 coplanar polygon face를 병합한 native FConvex로 거리·두께 sweep을 계산한다. 모서리 관통/이탈과 float 붕괴 회귀를 실제 UE에서 재현·수정하여 ConvexOutward-Green-2 Editor 빌드·40/40 통과. 바디 PhysicsAsset 연동은 미완료이며 기존22개 capsule/sphere를 유지한다. CA_NativePanelProbe 최신 재열기는 source 불일치로 실패해 재생성 대상이다. 커밋 조율용15개 안정 파일 manifest를 Saved/CommitCoordination에 기록하고 해당 파일만 동결했다.
- 커밋 조율 종료:15개 파일은 게임 대화에서 a9fe0ac로 커밋했고 동결 해제됨. 이 대화는 stage/commit하지 않는다. 해제 뒤 이어가지 못했던 작업을 사용자 지적 후 재개했다.
- Task 2/3 바디 convex: 실제 cooked shape의 unit bone-space 검증 및26방향 지지 다면체로 clavicle/upperarm6개를 저장.19bodies/16capsule+sphere/6convex, 새 프로세스41/41. Z8.0cm 틈과 원본 dominant 정점 포함 확인. TShirt-ShoulderConvex-1은 여전히283.9% 최대 변 길이 오차와 면 관통으로 실패. Z8.2cm의 실제 틈을 proxy가 메우는 잔여 문제 확인.
- Task 3 수동 보정: generated/welded/joint 안정 ID와 내부 SHA256 topology 대응, maxDistance·skin 실제 Cloth 적용, 재생성 재적용 및 변경된 topology 미해결 보존. CorrectionMap-Red41/42 → Green-2 42/42. float 언더플로 회귀42/43 → CorrectionFailures-Green43/43. 파일 계약 canonical 해시나 archive 구현으로 간주하지 않는다. Task 1~3은 대표 의상·포즈 검증이 남아 전체 완료 아님.

- Task 2/3 추가 검증: CA_NativePanelProbe 최신 재생성·새 프로세스 재열기/240frame 통과로 과거 source 불일치 해소. 쇼츠의 허리 가이드/가랑이 경로 길이 필요조건 수정 및 회귀44개로 확장. 접촉면 coverage 검사 추가 후 공기 틈 회귀를 재현·수정하여 BodyAngularSupport-Green45/45. 실제 cooked 삼각형29454개 전체 포함을 독립 검사했고19bodies/10capsule+sphere/19convex. Z8.2cm 공간, 대표 의상 변형/착장/포즈는 미완료이며 현재 쇼츠 비교 실행 중.
- Task 2/3 겨드랑이 추가 공간: 실제 삼각형 보존과 공기 구 보호를 동시에 만족하는 whole-triangle 분할·병합 및 bounded support fallback으로 collisionFit0.3.9 저장. ArmNegativeSpaceSupports-Green46/46, 독립 접촉면29454/29454포함. 쇼츠의 양옆 가이드/밑단 폭 비교는 실패하여 기본 템플릿에 적용하지 않았고, 독립 제약 진단에서 확인한 중앙 밑위 길이를 비교 중. 대표 의상/포즈 인수는 아직 미완료.
- Task 2/3 허벅지 테이퍼: 실제 단면3.573cm에 비해 proxy 둘레4.776cm가 밑단을 아래로 강제하는 과대 근사 확인.8구간/74방향과26방향 fallback으로0.3.10 저장, 문제 단면3.872cm. ThighTaperSupports-Green48/48 및 독립29454면 포함. 직접 CCD가 막지 않는 실제 두 의상 경로도 native 회귀로 확인. 대표 피팅은 여전히 검증 중이며 완료로 처리하지 않음.
