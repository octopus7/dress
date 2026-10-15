# LaceLoft Garment Workflow Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans for implementation in the dedicated Astra / High chat. Delegate independent research/review where useful; keep implementation ownership and model settings aligned with the user's request. Track each phase with the checkboxes below.

**Goal:** 지정선 재단과 봉제로 반팔 티셔츠/돌핀쇼츠를 만들고, UV가 완성된 OB11 계열 흰 더미에 입혀 포즈를 취하는 흐름을 구현합니다.

**Architecture:** 옷본 원본을 UI·판정·의상 형상의 공통 데이터로 사용합니다. 제작 진행은 C++ 상태 모델, 화면은 실제 WBP, 인형은 Skeletal Mesh, 완성 의상은 에디터에서 제작한 Chaos Cloth Asset으로 구성합니다.

**Tech Stack:** UE 5.8 C++, UMG Widget Blueprint, Blender/FBX, Chaos Cloth Asset, Physics Asset, Pose Asset/Control Rig, SaveGame.

**Spec:** [전체 설계](../Design/LACELOFT_DESIGN.md)

**실행 대화:** `01a14019-82ac-7f20-a34b-a20f53c96744` — Astra (`gpt-6-astra`), High, 로컬 `D:\github\dress`. 이 계획의 단계 완료 여부는 해당 대화가 작성하는 `Docs/Implementation/STATUS.md`를 기준으로 확인합니다.

## Global Constraints

- 실제 구현은 별도 **Astra (`gpt-6-astra`) / High** 대화에서만 수행합니다. 설계·관리 대화에서는 제품 코드/애셋을 수정하지 않습니다.
- [AGENTS.md](../../AGENTS.md), [UMG 지침](../UMG_GUIDELINES.md), [요청 문서](../REQUESTS.md)를 먼저 읽습니다.
- OB11 계열 큰 머리·짧은 몸, 흰 더미, 고유 UV0와 균일 texel density는 필수입니다.
- 재단은 지정선입니다. 자유 재단 솔버를 새로 만들지 않습니다.
- 현재 실제 WBP와 세 레벨을 확장합니다. 기존 사용자 수정/애셋을 통째로 재생성하거나 덮어쓰지 않습니다.
- UI 계층은 항상 실제 WBP에 저장합니다. 클래스에서 WidgetTree/ConstructWidget/Slate로 화면을 조립하지 않습니다.
- 형상·치수·UV 수치는 설계의 제안값부터 시작하여 검증합니다. 사소한 조정은 요청 문서에 추가하지 않되 자산 치수표/검증 결과는 갱신합니다.
- 각 단계의 결과를 `Docs/Implementation/STATUS.md`에 기록합니다. 진행 중/검증됨/차단됨을 구별하고 실제 실행한 검증만 적습니다.

## Review Focus

1. 작은 인형에서 천 충돌 두께가 몸/의상 크기를 압도하는 문제 — 2단계의 실제 크기 packaged 검증.
2. UV 겹침은 없지만 머리와 몸의 도색 밀도가 다르거나 관절이 늘어나는 문제 — 3단계의 수치/체크무늬/비대칭 도색 검사.
3. 화면 확대·DPI 변경 또는 커서 점프로 재단/봉제 진행이 잘못 누적되는 문제 — 5단계의 좌표/구간 테스트.
4. Undo·레벨 이동·종료/재실행 후 완료 상태와 착장 가능 여부가 달라지는 문제 — 6·8단계의 실제 복원 검증.
5. 의상 하나씩은 정상이나 동시 착용·앉기에서 관통하거나 포즈가 맞지 않는 문제 — 7·9단계의 조합 검증.

## 의존 관계

`0 기준 확인 → 1 인형 블록아웃/골격 → 2 작은 착의 검증 → 3 인형·UV 확정 → 4 옷본/의상 제작 → 5 재단 UI → 6 봉제/저장 → 7 착의/포즈 → 8 전체 연결 → 9 패키지 검증`

4단계의 데이터 스키마가 확정되면 UI와 옷본 아트 제작을 병렬로 진행할 수 있습니다. 인형 치수/골격/UV를 확정하기 전에 두 의상의 완성 아트와 웨이트를 확정하지 않습니다.

## 0. 현재 프로젝트 기준 확인

**대상:** 기존 `Source/LaceLoft*`, `Config/`, `Content/Maps/`, `Content/UI/`, `Source/LaceLoftEditor/Private/Tests/`.

- [x] 기존 변경 상태와 실제 애셋을 확인하고 보존합니다. 저장소에 아직 첫 커밋이 없을 수 있으므로 임의 reset/clean을 하지 않습니다.
- [x] UE 5.8 Editor/Game 빌드와 기존 `LaceLoft.Content.AuthoredAssets`, `LaceLoft.Navigation.AllRoutesPIE`를 실행해 출발점을 기록합니다.
- [x] 제작 화면의 부모 클래스를 확장할 때 필수 이동 버튼의 이름·타입 계약을 유지하도록 정합니다.
- [x] `Docs/Implementation/STATUS.md`에 전체 단계와 사용 엔진/모델/현재 차단 요인을 기록합니다.

**완료:** 기존 프로젝트가 실행되고, 확장 전 검증 결과와 보존할 애셋 목록이 남습니다.

## 1. 흰 더미 블록아웃과 골격

**신규 원본:** `ArtSource/Dummy/LaceLoftDummy.blend`, `ArtSource/Dummy/LaceLoftDummy.fbx`, `ArtSource/Dummy/Measurements.json`.

**UE 애셋:** `Content/Characters/Dummy/SK_Dummy`, `SKEL_Dummy`, `PHYS_Dummy`, `M_Dummy`, `MI_DummyWhite`.

- [x] 전체 높이 15cm/약 3등신을 출발값으로 정·측·후면 실루엣을 만들고 큰 머리·짧은 몸 비율을 검수합니다. 단순 구/원통을 붙인 상태를 완성품으로 취급하지 않습니다.
- [x] A포즈, 필수 본 구조, 관절 피벗을 확정하고 웨이트를 작성합니다.
- [x] 실제 UE Skeletal Mesh로 임포트한 뒤 크기·축·스케일·본 목록·UV 채널 존재를 검사합니다.
- [x] A포즈/팔꿈치·무릎 90°/고개 회전을 적용해 관절과 파트 연결을 확인합니다.
- [x] 메시에서 측정한 치수표와 skeleton 버전을 저장합니다.

**검증:** `LaceLoft.Dummy.ImportContract`에서 크기, 필수 본, 미할당 웨이트, 유효 참조를 검사. 화면 검수는 정·측·후면과 변형 포즈 이미지로 남깁니다.

## 2. 인형 크기에서의 천 수직 검증

**대상:** 임시 티셔츠 패널, `Content/Development/ClothProbe/`, 관련 개발 전용 검증.

- [x] 인형 몸에 맞는 최소 티셔츠 패널을 만들고 동일 skeleton에 웨이트를 전이합니다.
- [x] Chaos Cloth Asset/Dataflow로 시뮬레이션 메시·렌더 메시·부착 맵·Physics Asset을 구성합니다.
- [x] 인형 SkeletalMesh에 붙인 Cloth Component에서 `SetAsset()`으로 의상을 표시하고 두 포즈를 전환합니다.
- [x] 15cm 스케일에서 충돌 두께, 안정화 시간, 떨림/폭발/몸 관통을 확인합니다.
- [x] 이 작은 장면을 Win64로 cook/package하여 실행합니다. 에디터 Dataflow 의존성이 게임 모듈로 새지 않았는지 확인합니다.

**완료:** Editor와 패키지에서 같은 크기와 포즈의 천이 작동합니다. 실패하면 여기서 원인을 해결하고 후속 아트 대량 제작을 진행하지 않습니다. 해결되지 않는 엔진 제약은 구체적인 로그와 대안을 설계·관리 대화에서 검토할 수 있게 기록합니다.

## 3. 인형 모델·UV·재질 최종화

**추가 원본:** `ArtSource/Dummy/UVLayout.png`, `BaseColor_White.png`, `UVValidation.json`, `QA/` 이미지.

- [x] 블록아웃을 최종 토폴로지/노멀/관절 형태로 다듬고 치수·웨이트·Physics Asset을 다시 검수합니다.
- [x] 설계 4.3의 UV0 기준에 맞춰 seam, unwrap, 밀도 정규화, stretch 완화, packing을 수행합니다.
- [x] UV 누락/영역 밖/비퇴화/중복 영역, island별 texel density, 여백을 측정하는 검증을 먼저 작성하고 실제 실패 표본으로 검사기를 확인합니다.
- [x] 체크무늬와 좌우 문자 도색으로 머리·몸·팔·다리·손발을 검수합니다. 2K/1K/512px와 UE 임포트 후에도 확인합니다.
- [x] 흰 BaseColorTexture를 적용한 `MI_DummyWhite`를 만들고 임시 체크 재질로 바꿔도 UV가 그대로 유지되는지 확인합니다.
- [x] 최종 FBX 재임포트 후 본/웨이트/UV/스케일을 재검증하고 치수 버전을 고정합니다.

**완료:** UV 검사 결과와 도색 검수 이미지가 있어야 하며, 흰 모델 스크린샷만으로 통과시키지 않습니다.

## 4. 공통 옷본 데이터와 두 의상 애셋

**C++ 파일:** `Source/LaceLoft/Public/Garments/LaceLoftPatternDefinition.h`, `LaceLoftGarmentTypes.h`, 대응 `Private/Garments/` 구현.

**원본/애셋:** `ArtSource/Patterns/TShirt.json`, `DolphinShorts.json`, `ArtSource/Garments/`, `Content/Garments/Patterns/DA_TShirt`, `DA_DolphinShorts`, `Content/Garments/Cloth/CA_TShirt`, `CA_DolphinShorts` 및 Dataflow 원본.

**계약:** `ULaceLoftPatternDefinition`은 조각/경로/봉제/완성 애셋과 버전을 제공하고, 읽기 전용 원본과 작업 진행 상태는 분리합니다. `ValidatePattern(const ULaceLoftPatternDefinition&)`은 잘못된 ID·길이비·순환 선행조건·열린 경계 봉제를 결과 목록으로 반환합니다.

- [x] 조각 ID 중복, 없는 EdgeId, 잘못된 방향, 봉제 선행조건 순환, 허용되지 않은 길이비를 잡는 `LaceLoft.Patterns.*` 테스트를 작성합니다.
- [x] 설계의 티셔츠/돌핀쇼츠 조각과 재단선·봉제선·시접·notch·열린 경계를 같은 mm 원본에서 만듭니다.
- [x] 일반 봉제, 접어박기, 바인딩, 시보리/허리의 의도한 길이비를 구별합니다.
- [x] 같은 조각과 seam mapping에서 몸에 맞춘 3D 의상을 만들고 UI 패턴과 PieceId별 대응을 검사합니다.
- [x] 의상별 시뮬레이션/렌더 메시, UV, 스킨 웨이트, 고정 영역, Dataflow/Cloth Asset을 저장합니다.
- [x] 파일 원본 hash·PatternVersion·DummyMeasurementsVersion을 애셋에 기록하고 불일치를 검증에서 실패시킵니다.

**완료:** 두 의상과 옷본 데이터가 실제 애셋으로 존재하고, 패턴 조각/봉제 정보와 3D 결과의 대응을 추적할 수 있습니다.

## 5. 제작 화면과 지정선 재단

**C++ 파일:** `UI/LaceLoftCreationWidget.*`, `UI/LaceLoftPatternWorkspaceWidget.*`, `Garments/LaceLoftCuttingModel.*` (Public/Private 구분).

**WBP:** 기존 `WBP_GarmentCreation` 확장, 신규 `WBP_PatternWorkspace`, `WBP_PatternPieceEntry`.

**계약:** `BeginStroke(PathId, PointMm)`, `UpdateStroke(PointMm)`, `EndStroke()`는 활성 경로의 연속 완료 구간과 하나의 Undo 명령을 산출합니다. 화면 좌표 → 로컬 → 줌/이동 역변환 → mm 변환을 공용 함수로 둡니다.

- [x] 배율/DPI, 시작점 밖 클릭, 곡선을 가로지른 점프, 동일 구간 반복, 캡처 상실, 창 밖 마우스 release를 검증하는 `LaceLoft.Cutting.*` 테스트부터 작성합니다.
- [x] 실제 WBP에서 레이아웃/목록/버튼/작업대를 제작하고 Designer에 예제 옷본 미리보기를 제공합니다.
- [x] 마우스를 누른 상태로 활성 선을 따라갈 때만 진행합니다. 진행률은 거리 합산이 아니라 연속 구간의 합집합으로 계산합니다.
- [x] 경로 밖 입력은 진행시키지 않고 가까운 선을 표시합니다. 닫힌 재단 경계를 완료하면 그 조각을 분리하여 이동/정리할 수 있게 합니다.
- [x] 한 번의 스트로크 단위 Undo/Redo, 확대/축소, 작업대 이동, 활성 조각 선택을 연결합니다.

**완료:** 두 패턴을 UI에서 직접 재단할 수 있고, 클릭만 반복하거나 동일 선을 왕복하여 부정확하게 완료되지 않습니다.

## 6. 봉제·작업 저장

**C++ 파일:** `Garments/LaceLoftSewingModel.*`, `Session/LaceLoftWardrobeSubsystem.*`, `Session/LaceLoftSaveGame.*`.

**WBP:** `WBP_SewingStepEntry`, `WBP_ConfirmDialog`, 제작 화면 단계 연결.

**계약:** `CanStartSeam(SeamId)`, `BeginSeam(SeamId, PointMm)`, `UpdateSeamStroke(PointMm)`, `EndSeamStroke()`, `Undo()`, `Redo()`는 선행조건과 작업 완료를 관리합니다. 봉제도 재단의 경로 투영·연속 구간 합집합 판정을 재사용하며, 외부에서 임의 진행률을 주어 완료시키는 API를 만들지 않습니다. 서브시스템의 `SaveSession()` / `LoadSession()`은 ID·버전·값 데이터만 저장합니다.

- [x] 미재단 조각 봉제, 틀린 짝/방향, 열린 입구 봉합, 중복 완료, 종속 작업 Undo를 검사하는 `LaceLoft.Sewing.*` 테스트를 작성합니다.
- [x] 봉제의 커서 점프, 반복 구간, 역방향, 캡처 상실을 검사합니다. 확대/DPI가 바뀌어도 같은 선 구간만 완료되어야 합니다.
- [x] 안내된 봉제선 쌍과 진행 방향을 표시하고 바늘 커서를 따라 연속 구간만 완료합니다. 마감은 지정된 접어박기/바인딩 작업으로 진행합니다.
- [x] 2D 작업대에서 조각 정렬, 반투명 목표 윤곽, 연결된 봉제선으로 단계별 조립 상태를 표시합니다. 부분 의상의 3D 천 시뮬레이션은 추가하지 않습니다.
- [x] 완료/미완료 봉제선을 구분하고, 모든 필수 작업이 완료되면 결과 의상을 해금합니다.
- [x] 완료 후 Undo는 해당 ProjectId의 결과 자격을 취소하고 장착 중이면 해제합니다. Load에서도 실제 진행/패턴 버전으로 자격을 재계산합니다.
- [x] 작업물 여러 개, 상·하의 완성 결과, 현재 장착 상태를 SaveGame으로 저장합니다. 원본 Data Asset을 수정하지 않습니다.
- [x] 저장은 완료된 입력 명령 경계에서 수행하고 실패를 UI에 표시합니다. 손상/구버전 데이터는 원본을 보존하며 복구 가능한 기본 상태로 엽니다.

**완료:** 도중 종료·재시작으로 재단/봉제 작업을 이어갈 수 있고, 완료 전에 의상을 장착할 수 없습니다.

## 7. 착의·카메라·포즈

**C++ 파일:** `Fitting/LaceLoftDummyActor.*`, `Fitting/LaceLoftFittingController.*`, `UI/LaceLoftFittingWidget.*`.

기존 PlayerController는 계속 공용으로 사용합니다. `ULaceLoftFittingController`는 화면 수명의 UObject로 인형에 대한 약한 참조와 카메라/포즈 명령을 담당하며, 레벨 전환 뒤 기존 월드 객체를 붙잡지 않습니다.

**애셋:** `Content/Characters/Dummy/Animation/ABP_Dummy`, `PA_DummyPoses`, 필요 시 `CR_Dummy`; 기존 `WBP_FittingRoom`, 신규 `WBP_GarmentEntry`, `WBP_PoseControls`.

- [x] 완성 의상만 장착, 상/하의 슬롯 독립, 두 벌 동시 착용, 해제 후 stale component 제거를 `LaceLoft.Fitting.*`로 검증합니다.
- [x] 실제 인형과 Cloth Component를 연결하고 완성한 패턴 버전과 대응하는 애셋만 장착합니다.
- [x] 포즈 프리셋 4종(중립/팔 들기/팔 앞으로/앉기), 주요 관절 회전·범위 제한·초기화를 제공합니다.
- [x] 카메라 회전/줌/초기화를 UI 입력 영역과 연결하고 버튼 조작 중 월드 카메라가 함께 움직이지 않게 합니다.
- [x] 천 reset/teleport·포즈 전환 안정화, 양 의상 허리 겹침을 검사합니다. 단일 컴포넌트 조합이 필요하면 이유와 검증을 기록합니다.

**완료:** 프리셋과 관절 조절로 인형이 실제 골격 포즈를 취하며, 상의·하의가 같은 포즈를 따르고 필요한 부위에 천 움직임이 있습니다.

## 8. 세 레벨 전체 흐름 연결

**수정:** 기존 PlayerController/NavigationWidget, 타이틀 WBP, 시작/쿠킹 설정, 기존 자동화.

- [x] 제목 화면의 새 제작/이어하기/입어보기 버튼을 세 레벨 흐름에 연결합니다.
- [x] 이동 전 작업 상태 저장, 재진입 시 작업 복원, 다른 의상 선택 시 기존 작업 보존을 구현합니다.
- [x] 모든 신규 WBP/의상/인형/포즈가 packaged 실행에서도 로드되도록 명시적 애셋 관리/쿠킹 경로를 구성합니다.
- [x] 기존 6방향 이동 검사에 재단 중/봉제 중/완성 후 이동과 재방문을 추가합니다.
- [x] 완성 → 장착 → 제작 화면 복귀 → Undo → 저장 → 재실행에서 해당 결과가 해제되고 미완성 자격으로 복원되는지 검사합니다. 다른 작업물은 유지되어야 합니다.
- [x] 빈 저장, 정상 저장, 손상 저장, 패턴 버전 변경, 누락 의상 참조를 검사합니다.

**완료:** 티셔츠와 쇼츠 각각 처음부터 제작하고 저장 후 재실행하여 인형에 동시에 입힐 수 있습니다.

## 9. 최종 검증과 인계

- [x] Editor/Game 빌드, `LaceLoft.*` 전체 자동화, 실제 WBP Compile/Save를 실행합니다.
- [x] Win64 Development 패키지에서 두 의상의 전체 사용자 흐름, 저장 복원, 포즈·카메라·천을 실행합니다. 검증 방식은 실제 Slate 입력 자동화와 렌더 이미지 시각 검수이며 인간 수동 플레이로 보고하지 않습니다.
- [x] 1280×720 / 1920×1080 / 2560×1440과 DPI 변화에서 UI 겹침/절단과 입력 좌표를 검수합니다.
- [x] 인형 UV 수치/도색/관절 검수와 의상 포즈별 정·측·후면 스크린샷을 정리합니다.
- [x] 개발 PC/해상도/cloth 품질을 기록하고 프레임 시간·메모리를 측정합니다. 초기 목표 1080p 60fps는 측정 후 달성 여부를 보고하며 보장으로 쓰지 않습니다.
- [x] 원본 `.blend`/FBX/패턴 JSON/UV PNG/Dataflow와 실제 UE 애셋을 모두 인계합니다. 개발 probe는 제품 기본 흐름에 노출하지 않습니다.
- [x] `Docs/Implementation/STATUS.md`와 README에 완료 범위, 실행 방법, 검증 결과, 남은 문제를 기록합니다.

**최종 합격:** 설계 9절의 완료 조건을 전부 증빙합니다. 소스만 작성하거나 에디터에서만 표시되는 상태, UV가 없는 흰 모델, 재단 데이터와 무관한 의상 표시를 완료로 보고하지 않습니다.

## 실행/검증 참고

설치된 엔진은 현재 `E:\WorkTemp\Epic Games\UE_5.8`이고 확인 버전은 5.8.3입니다. 실제 실행 시 설치 경로를 다시 확인합니다. 일반 Build.bat이 실행 환경에서 실패하면 엔진 번들 dotnet으로 UnrealBuildTool.dll을 실행할 수 있습니다.

```powershell
$UE = (Get-ItemProperty 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8').InstalledDirectory
& "$UE\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" "$UE\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" '-Target=LaceLoftEditor Win64 Development' '-Target=LaceLoft Win64 Development' "-Project=$PWD\LaceLoft.uproject" -WaitMutex -NoHotReloadFromIDE
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\LaceLoft.uproject" -unattended -nosplash -nullrhi -nosound '-ExecCmds=Automation RunTests LaceLoft' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD\Saved\Automation\Implementation"
```

명령 종료 코드뿐 아니라 보고서의 테스트 개수·실패·경고도 확인합니다. NullRHI 자동화는 렌더링/UV/천의 시각 품질 검사를 대체하지 않습니다. 실제 렌더링과 packaged 실행을 별도로 검증합니다.

