# 의상 제작 확장 구현 상태

기준 계획: `Docs/Plans/2026-10-15-laceloft-garment-workflow.md`

실행 대화: `01a14019-82ac-7f20-a34b-a20f53c96744` (지정: Astra / High). 시작: 2026-10-16 KST.
엔진: UE 5.8.3, CL 58210709, `E:/WorkTemp/Epic Games/UE_5.8`. Blender: 5.2.0 LTS.

## 단계

| 단계 | 상태 | 근거 / 남은 일 |
|---|---|---|
| 0 기준 확인 | 검증됨 | Editor/Game 빌드 및 기존 자동화 2/2 성공, 경고 0 |
| 1 인형 블록아웃·골격 | 검증됨 (블록아웃) | 15cm / 20본 / 19 충돌 바디, +X 전방, UE A포즈·팔/무릎 90°·고개·팔 올리기 12개 렌더 검수. 최종 표면/UV 승인 아님 |
| 2 실제 크기 천 검증 | 검증됨 | Editor 자동화 5/5, GPU 및 Win64 패키지에서 892 동적 입자/두 포즈/6개 화면, 관통 0cm |
| 3 인형·UV 최종화 | 검증됨 | dummy-1, 명시적 seam 16 UV 섬, Blender/UE 겹침 0·밀도·여백 통과, 도색/6 포즈·Cloth 회귀 확인 |
| 4 옷본·의상 | 검증됨 | 두 JSON/DA/Cloth/Dataflow, 5/7조각·봉제 대응·원본 해시 검사, 네 포즈 실제 표면 검증 |
| 5 재단 UI | 검증됨 | 실제 WBP/Designer 미리보기, 전체 두 옷본 Slate hit-test 재단·봉제, DPI 입력·Undo·캡처 상실 검사 |
| 6 봉제·저장 | 검증됨 | 다중 작업·독립 착장·복원, 원자적 저장 교체 실패 주입·이전 파일 보존, Undo 안내 회귀 통과 |
| 7 착의·포즈 | 검증됨 | 네 포즈/관절/두 슬롯, 실제 hit-test 카메라·포즈 버튼, 동시 의상 교차 0, 관절별 한도 DA 검증 |
| 8 전체 흐름 | 검증됨 | 6방향 이동 중 열린 재단/봉제 저장, 완성·착용·복귀·Undo·fresh session 복원 통과 |
| 9 최종 패키지 검증 | 검증됨 | 최종 Win64 패키지, 16/16 자동화·경고 0, 세 해상도 실제 입력·복원·천·성능 검증 완료 |

## 보존 및 실행 결정

- 첫 커밋이 없는 저장소이며 기존 파일은 전부 미추적 상태. reset/clean 없이 지정된 현재 폴더에서 작업한다. 기존 6개 애셋의 SHA256은 `BaselineAssets.json`에 보존했다.
- Ruling: Git HEAD가 없고 사용자가 현재 폴더의 작업 보존을 지정했으므로 worktree/commit 기반 실행 스크립트 대신 이 상태 문서를 실행 원장으로 사용한다. 잘못된 판단일 경우 파일 단위 복구 비용이 발생하므로 기존 애셋을 변경하기 전에 별도 보존한다.
- UI 확장 부모는 `ULaceLoftNavigationWidget`을 상속하고 `TitleButton`, `CreationButton`, `FittingButton`의 UButton 계약을 유지한다.
- 단계 연결 검토: 1→2는 동일 skeleton/실제 cm 크기, 2→3은 packaged 천 동작 승인, 3→4는 치수 버전, 4→5·6은 Piece/Edge/Seam ID, 6→7·8은 작업 상태에서 재계산한 완료 자격을 전달한다. 현재 모순 없음.
- Blender MCP headless의 단순 버전 조회가 30초 시간 초과. 설치 바이너리 직접 실행은 4.2초 내 성공. 독립 background 프로세스로 원본을 제작하여 기존 사용자 Blender 세션에 영향을 주지 않는다.

## 실행 근거

- S0 Editor/Game: 엔진 번들 dotnet의 UnrealBuildTool 실행, `Result: Succeeded`, exit 0. `Saved/Automation/Implementation/S0/Build.log`.
- 최초 샌드박스 실행은 엔진 Source 디렉터리 접근 거부. 정상 권한 실행에서 성공하여 제품 코드 문제와 구별했다.
- S0 자동화: `S0/index.json` 2 성공, 0 실패, 0 경고. 6방향 PIE 이동 포함.
- S1 RED: `S1-Red/index.json` 기존 2개 성공, 인형 검사 1개 실패. `SK_Dummy` 누락을 올바르게 검출했다.
- S1 Blender: 원본 .blend/FBX, 실제 단면 치수 JSON, 정·측·후면 PNG 생성. 36,436 정점, 미할당 웨이트 0, 최대 영향 4. 원본 실루엣은 큰 머리·짧은 몸 조건에 부합하며, 표면/손/UV는 아직 블록아웃 품질이다. 최종 품질로 승인하지 않았다.
- S1 Import: `S1-Import.log` 15.0000cm, 20본, 19 충돌 바디. 명령 종료 0, 오류/경고 0. 충돌은 초기 자동 피팅이며 단계 2~3에서 수동 치수 검수가 필요하다.
- S1 GREEN: `S1-Green/index.json` 자동화 3/3 성공, 실패/경고 0. 본 스케일 1, 바닥 z=0, 유효 UV0, 정규화된 최대 4 영향 웨이트 및 혼합 웨이트 검증.
- S1 QA: UE 골격 애니메이션 및 렌더 검수 진행 중. 시각 품질과 관절 변형은 수치 검사만으로 통과시키지 않는다.
- S1 축 회귀: 첫 UE 렌더에서 +Y 전방을 발견. `S1-Axis-Red`에서 축 계약 실패를 재현했다. UE의 unattended FBX reimport는 저장된 import-data를 우선하므로 해당 데이터의 ForceFrontXAxis를 갱신하고 Interchange 자동 전환을 제작 명령 내에서 비활성화했다. `S1-Axis-Green2` Editor/Game 빌드와 전체 자동화 3/3 성공, 경고 0. UE의 upperarm_l 원점은 `(0,-1.48,9.15)`cm로 변환됐다.
- S1 렌더 도구: commandlet의 IsClient=false가 NULL scene을 생성한 것이 첫 crash 원인. 렌더용 client와 AllowCommandletRendering으로 해결. 반복 캡처에서는 게임/렌더 frame 번호 갱신이 필요했다. 첫 캡처의 셰이더 준비와 무릎 굽힘 방향을 보완 중.
- 렌더 로그의 GPU 경고: NVIDIA 드라이버 610 미만에서 TSR 16-bit VALU를 비활성화한다는 엔진 경고 1개. 프로젝트 자동화 경고와 구분한다.
- S1 최종 블록아웃 렌더: `ArtSource/Dummy/QA/S1_UE_*.png` 12개. 정지 검수에서는 temporal AA/motion blur를 꺼 잔상을 제거했다. 팔꿈치/무릎은 90° 회전, 무릎은 뒤쪽으로 굽힘. 파트 분리나 관절 붕괴는 관찰되지 않았다. 링 단면으로 인한 표면 굴곡, 손 형태, 최종 UV는 S3의 명시적인 남은 일이다.
- S2 RED: `S2-Red/index.json` 기존 3개 성공, `LaceLoft.Cloth.ProbeAssetContract`는 존재하지 않는 Cloth Asset을 검출해 실패. `ArtSource/Development/ClothProbe.json`에는 2개 몸판, 봉제 전 950 정점, stitch 36쌍이 있다.
- Ruling: S2는 최종 티셔츠 전체 아트 이전의 몸판 검증물로 만든다. 어깨 고정/몸통 충돌/동일 골격 pose/실제 크기 동작을 먼저 검증하며, 소매를 포함한 최종 의상은 S4 범위다. 이 probe 성공만으로 최종 의상 충돌을 통과시키지 않는다.
- S2 애셋/장면: `S2-Scene-Green/index.json` 5/5 성공, 경고 0. `CA_TShirtProbe`, `DF_TShirtProbe`, `L_ClothProbe` 저장. 914 용접 입자/1728 면, 같은 skeleton과 직접 부모 SkeletalMesh 부착 계약 확인.
- S2 실제 실행 RED: `S2-Runtime1/metrics.json`에서 855 동적 입자와 settling 관찰. 자동 피팅한 head 캡슐의 반경 2.536cm/축 길이 2.649cm가 실제 머리 아래로 돌출하여 관통 0.624cm 검출. 단위 cm/액터 배율 1을 유지하며 19개 해부학적 충돌체를 다시 피팅했다.
- S2 화면 도구: 물리 카메라의 기본 EV와 낮은 검수 조명의 불일치로 검은 이미지가 나왔다. 검수 카메라에서 물리 노출을 끄고, 실제 camera cache가 갱신되는 다음 프레임에 촬영하도록 보완. `S2-Runtime3`에서 몸/천이 보이는 정·측·후면 저장을 확인했다.
- S2-Runtime3: 중립 포즈 관통 0.032cm/안정화 최대 프레임 변위 0.015cm. 팔 올리기에서 clavicle 관통 0.154cm로 기준 0.12cm 실패. 팔에 영향을 받는 고정 어깨 정점을 목둘레/가슴 고정 방식으로 보완 중이며, 기준을 완화하지 않았다. 하복부의 작은 가시 관통도 충돌체 단면 보완 대상으로 확인했다.
- S2 렌더 재질: 기본 BasicShapeMaterial의 SkeletalMesh/Clothing 플래그 누락 경고를 발견하여 probe 전용 흰 재질과 천 재질을 명시적으로 저장 중. 기존 블록아웃 렌더를 최종 재질/표면 승인으로 취급하지 않는다.
- S2 GREEN: `S2-Runtime4/metrics.json` 두 포즈 892 동적 입자, 관통 0cm, 안정화 최대 프레임 변위 0.00950cm. 고정 맵은 목둘레를 chest에 부착하고 나머지 천은 실제 전이 웨이트/시뮬레이션을 사용한다. `S2-Collar-Green/index.json` 자동화 5/5, 경고 0.
- S2 패키지: `S2-Package.log` Win64 BuildCookRun 성공, exit 0. `Saved/ClothProbePackage/Windows/LaceLoft.exe` 실제 실행. `S2-Packaged2` 두 포즈 각각 599 samples, 892 동적 입자, 관통 0cm, 안정화 최대 변위 0.00352/0.00643cm. `image-validation.json` 6개 PNG의 실제 밝기·파란 천 픽셀 검증, 정·측·후면 시각 검수 완료. editor-only Dataflow 노드 누락 경고는 패키지에 없음.
- S2 패키지 검증 스크립트: GUI bootstrapper는 PowerShell에 먼저 제어를 반환하므로 `Start-Process -WindowStyle Hidden -Wait`로 자식 게임 종료까지 기다린다. 초기 `S2-Packaged1`은 스크립트가 일찍 판정한 실행이며 최종 근거로 사용하지 않는다.
- 잔여 개발 장면 경고: pawn 없는 probe GameMode의 SpawnActor 경고, 설치 GPU 드라이버의 TSR 경고. 검증한 시뮬레이션/이미지에는 영향이 없었으며 제품 자동화 5/5는 경고 0이다.

- S3 검사기: `ArtSource/Tools/test_uv_validation.py` 겹침/공유 경계/퇴화·범위/밀도 불균형/31px·15px 실패 표본 5/5 통과. 최초 모듈 부재 RED → 구현 GREEN. 삼각형 교집합 양의 면적과 실제 경계 segment 거리를 검사한다.
- S3 원본: cubic profile로 큰 loft 띠의 곡률을 개선, 36,816 정점/최대 4 웨이트. 뒤통수/목/사지 안쪽 및 몸통 측면의 명시적 seam 사용. 대칭면 좌표가 정확히 0인 경우 절개가 누락되던 문제를 UV 겹침으로 검출해 수정. 초기 pelvis checker 왜곡도 몸통 전·후면/목 분리로 보완했다.
- S3 source/UE UV GREEN: `ArtSource/Dummy/UVValidation.json`, `Saved/Automation/Implementation/S3-ImportedUV/UVValidation.json`. UE 73,628 삼각형, 16 UV 섬, 겹침 0, 밀도 약 105.747px/cm(모든 섬 ±10% 이내), 최소 섬 간격 77.350px/외곽 50.097px. UE 면적 가중 stretch p95 1.231/p99 1.331/최대 1.993; 구형 머리 뒤와 관절의 국소 변형은 checker로 별도 검수.
- S3 Surface RED: 재질 없는 상태에서 해당 검사 1 실패, 기존 5 성공. `S3-Surface-Green` 및 최종 `S3-Final-Green` 자동화 6/6 성공, 경고 0. 실제 `M_Dummy`, `MI_DummyWhite`, 7개 텍스처/재질 인스턴스를 저장했다.
- S3 렌더: Blender 도색 18개, UE 6 포즈×7 재질×3 방향=126개 `S3_UE_*.png`. `S3-UE-QA2.log` 종료 0, GPU 드라이버 경고만 1. 배경 capture의 낮은 mip 로드를 발견해 검수 실행에 `-NoTextureStreaming` 사용(샘플러 mip/filtering은 유지). 좌우 L/R·색 분리, 머리 F/B, 2K/1K/512 및 관절을 확인했다. 원본/FBX/UV/흰 PNG와 packed source 이미지를 보존했다.
- S3 치수: 실제 표면 단면에서 목 4.012/가슴 7.704/허리 6.609/힙 8.140cm. 초기 단순 x 제한으로 팔 단면이 섞이던 가슴 측정을 해부학적 face 영역으로 보완했다. `Measurements.json`에 단면 좌표·측정 정의·source hash·dummy-1 버전을 기록했다. S1 백업은 `ArtSource/Dummy/Versions/blockout-1/`.
- S3 Cloth 회귀: `S3-ClothRegression` 두 포즈 모두 관통 0cm, 892 동적 입자, 여섯 GPU 이미지 검증 통과. 현재 단계는 최종 티셔츠/쇼츠의 동시 착의 검증을 대신하지 않는다.

## 다음 작업

단계 0–9 구현 및 검증 완료. 최종 인계 근거와 한계는 아래 마지막 절을 기준으로 읽는다. 이전 절의 미완료 표현은 당시 실행 기록이다.

## S4–S5 진행 근거

- S4-Pattern-Red3: 기존 6 성공, 잘못된 패턴 7개를 검출하지 못해 새 검사 실패. S4-Pattern-Green: 7/7 성공. 앞선 Red/Red2는 테스트 표본에서 TArray 자신의 원소를 Add하여 엔진 assert가 발생한 실행이며, 표본 복사 수정 후 재현했다.
- `ArtSource/Patterns/TShirt.json`, `DolphinShorts.json`: mm 원본, 5/7조각, 별도 3mm cut allowance, 봉제선/접는 선/notch/결/입구, 실제 2D/3D 정점·삼각형·Edge 대응. 닫힌 선은 첫 점을 끝에 명시적으로 반복한다.
- `S4-Assets-Red`는 누락된 두 의상 애셋을 검출. `S4-Assets-Green`, `S4-GarmentProbe-Build`는 Editor/Game 빌드와 8/8 성공·경고 0. 실제 DA/CA/DF 각 두 개, 의상 재질 세 개 저장. MD5는 보안 서명이 아닌 원본 변경 감지용이다.
- 초기 source 렌더의 목/어깨/밑위 관통을 실제 더미 표면 기반으로 보정했다. 정점별 표면 투영만으로 어깨 삼각형 교차가 생겨 welded 이웃 완화와 국소 겨드랑이 보정, 시보리 띠 재구성을 추가했다. `S4-PatternFit9.log`: 티셔츠 1692 vertices/3280 triangles, 쇼츠 1797/3448, 각각 1 connected component, 입구 4/3, Euler -2/-1, 비인접 삼각형 교차 0. 임계값을 완화하지 않았다.
- `S4-TShirt-Runtime1`: 1612 동적 입자, 네 포즈 최대 안정화 변위 0.043cm. 팔 들기 상완 관통 0.210cm로 0.12cm 기준 실패. 그 외 포즈 최대 관통 0.056cm. 어깨 전체 chest 고정을 바깥 어깨의 전이 웨이트로 바꿨으며 재실행 전이다. 최종 의상 동작 통과로 보고하지 않는다.
- Ruling: S4 원본 보정과 독립적인 S5 순수 입력 모델은 확정된 경로 schema를 사용해 먼저 검증한다. UI 연결이나 S4 완료 승인을 앞당기지 않는다.
- `S5-Cutting-Red`: 새 입력 검사 16개 실패. `S5-Cutting-Green`: 해당 검사 성공, 기존 7개도 성공했으나 의상 원본 갱신에 따른 해시 2개 불일치로 전체 실행은 실패. 원본 수정 뒤 애셋 재생성 필요성을 올바르게 검출했다.

## S4–S6 후속 검증

- `S6-Sewing-Green`: 순수 재단·봉제 모델을 포함한 전체 10/10 성공, 경고 0. 실제 두 패턴의 모든 절단선/봉제 DAG를 포인터 샘플로 완료하고 Undo/Redo 자격을 검증했다.
- `S6-Session-Green2`: Editor/Game 빌드, 11/11 성공. 작업물 두 개·상하의 독립 착장·저장 후 Undo/Redo·버전 불일치 부분 복구·손상 원본 보존을 확인했다. 빈 저장 파일 읽기 경고 2개는 존재 검사 추가로 수정했으며 `S5-Workspace-Red4`의 기존 11검사에서 다시 검증한다.
- `S4-TShirt-Runtime3`: 1628 동적 입자, 네 포즈 캡슐 관통 최대 0.01208cm, 안정화 변위 0.05cm 이하. 캡슐 통과만으로 실제 몸 표면 통과를 주장하지 않는다.
- 실제 posed body CPU skinning과 cloth OBJ를 저장하고 정점/삼각형 내부 표본을 검사하는 `validate_cloth_surface.py`를 추가했다. 내부 정점 및 정점은 외부지만 면 내부가 관통하는 실패 표본으로 검사기를 검증했다. 허용 깊이 0.02cm.
- `S4-TShirt-Surface2`: 실제 1280×960 PNG 12개, 캡슐 검사 통과. 실제 표면은 중립 관통 0, 팔 올리기 0.067cm/앞으로 0.117cm/앉기 약 0.13cm로 미통과. 허리 옆 변형을 덮는 spine 충돌체 추가 보정 후 재검증 필요. 쇼츠·동시 착용도 아직 승인하지 않았다.
- `S5-Workspace-Red4`: 작업대 C++ 빌드 성공. 새 WBP 네 개가 없어 UI 계약 검사에서 의도한 실패를 확인했다. 기존 WBP 계층을 기록했고, 여섯 원본 애셋은 `Saved/PreservedOriginals/S5-UI/`에 보관했다.
- UI authoring은 에디터 모듈에서 실제 애셋에 계층을 저장한다. 런타임은 저장된 WBP 인스턴스·바인딩·데이터 선 표시·입력만 담당한다. 기존 제작 WBP의 Background/NavigationMenu와 세 이동 버튼을 보존하여 확장한다.
- `S5-Workspace-Green`: 실제 신규 WBP 네 개 저장, 기존 제작 화면 확장, 12/12 자동화·경고 0. `S5-UIInput-First`, `S5-Capture-Build`: 13/13·경고 0. 실제 SObjectWidget→UMG 포인터 이벤트로 두 패턴 전체 재단·봉제, 캡처 상실, DPI 1/1.5, UI Undo/Redo 및 두 프로젝트 복원을 검증했다. Slate hit-test/실제 사용자 조작 검증과는 구별한다.
- `S5-UI-1080/Creation.png`: 실제 1920×1080 제작 화면. 기존 제목/이동 버튼의 글꼴 애셋이 깨진 것을 발견, 해당 WBP 텍스트를 정상 composite font로 보완 중. 시각 승인 전이다.
- S4 표면 검사 정정: nearest face plane의 부호는 폐곡면 내부 판정이 아니며, posed TangentZ로 매 프레임 면 방향을 뒤집으면 연결 방향도 깨진다. 읽기 전용 독립 조사에서 7-ray parity와 generalized winding으로 큰 보고값이 오탐임을 확인했다. `S4-SurfaceParity-Red` 역방향 면 cube 실패 → `S4-SurfaceParity-Green` 두 검사 성공. 면 순서를 보존하고 3-ray/불일치 시 7-ray + 실제 nearest 거리로 변경했다.
- 정정된 실제 티셔츠 내부 깊이: Surface1 P0 .024980/P2 .027655/P3 .054581cm, Surface2 P0–P2 0/P3 .054476cm, Surface3 P0–P2 0/P3 .039237cm. 실제 Sitting 최악은 triangle1190 부근 `[-.536843,1.455567,3.595655]cm`. 허리 전체 확대 대신 Surface2 spine(.98cm)로 복원하고 lateral pelvis 충돌을 보완 중. 기존 큰 0.209cm 등을 실제 관통으로 사용하지 않는다.
- `S7-PoseAssets-Red2`: 기존 13검사 성공, 누락 ABP_Dummy/PA_DummyPoses를 새 검사에서 검출. 네이티브 포즈 프록시, DummyActor 두 Cloth 슬롯, 화면 수명의 약한 참조 FittingController와 실제 WBP 연결 코드를 작성 중이다. 아직 착의 기능 검증 완료가 아니다.
- `S7-Fitting-Green` 14/14, `S7-Fitting-Runtime2` 15/15, 경고 0. 실제 인형 본 이동으로 네 프리셋·수동 관절·초기화 확인, 두 Cloth의 같은 skeletal leader·독립 장착·Undo 시 상의만 제거 확인. 실제 ABP/4-pose PA와 기존 fitting map의 인형/조명, WBP_FittingRoom/WBP_PoseControls/WBP_GarmentEntry를 저장했다.
- `S7-Fitting-1080`: 동시 착용 허리 z-fighting과 너무 좁은 기본 카메라를 발견. `S7-Fitting-1080-Layer2`: 실제 1920×1080에서 인형 전체가 보이고 중립 허리 겹침은 보이지 않는다. 상의 collision thickness .15cm/하의 .05cm, 기본 카메라 거리42/FOV50. 나머지 포즈 및 표면/동시 착용 수치 검수 전이므로 아직 최종 승인 아님.
- 폰트 원인: FCoreStyle의 transient file-backed font를 WBP에 저장하면 폰트 애셋 참조가 유지되지 않는다. 기본 UTextBlock의 실제 UFont를 저장해 한국어/영문 정상 렌더 확인. 기존 이동 버튼 긴 문구는 작은 sidebar에서 줄바꿈이 넘쳐 추가 조정 필요.
- 단계8 코드/애셋: 타이틀에 옷본 선택·새 제작·이어하기, 모든 이동 직전 EndStroke/SaveSession(실패 시 이동 취소), 명시적 UI/Dummy/Garments cook 경로 추가. 전체 흐름 회귀는 다음 실행에서 확인한다.

## S8–S9 최종화 근거

- `S8-IntegratedFlow`, `S9-FinalAssets-Regression`: Editor/Game 빌드 및 자동화 16/16, 실패·경고 0. 여섯 방향 실제 PIE 이동에 열린 재단/봉제 스트로크의 디스크 복원, 완성→두 벌 착장→제작 복귀→상의 Undo→fresh session에서 상의 자격/장착 취소·하의 유지 검사를 추가했다.
- `S5-Routed-1080-Green`, `S5-Routed-720`: 실제 렌더링된 Slate hit-test grid에서 입력 전달을 확인했다. 이전 probe는 `bIsSynthetic=true`여서 캡처 중 MouseMove를 Slate가 생략한 검사 코드 오류였다. 제품의 WorkspaceFrame hit-test 설정과 함께 수정해 가시 선 5.021726mm 진행을 확인했다.
- `S9-Editor-Complete2`: 실제 hit-test/capture/bubble 경로의 포인터 입력으로 두 옷본 모든 재단선과 모든 봉제 작업 완료, 상·하의 자격/장착·저장 확인. 버튼은 먼저 MouseMove로 진입시킨 뒤 down/up한다. 앞선 Complete 실행은 버튼 hover 진입을 생략해 두 번째 작업을 만들지 못한 검사 실패이며 최종 근거가 아니다.
- `S9-Editor-Fitting1080`: 새 실행에서 완성한 두 작업을 복원, 실제 화면 hit-test로 카메라 회전/휠/초기화와 네 포즈 버튼(버튼 조작 중 카메라 정지) 통과. 검증 저장 슬롯은 `LaceLoft_UICapture_*`로 플레이어 파일과 분리한다.
- `S7-Both-Surface4`: body 표면 ray-parity는 네 포즈 모두 0이지만 의상끼리 교차 74/120/72/96쌍을 검출했다. 독립 segment–triangle 검사도 동일 교차를 확인했다. 주 위치는 top 하단과 waistband이며, 앉기에서 앞판·쇼츠 허벅지 일부도 겹쳤다.
- `S7-Both-Spacing30`: 상의 collision thickness 0.30cm/하의 0.05cm에서 네 포즈 몸 내부 표본 0, 상하의 삼각형 교차 0, 12개 정·측·후면 GPU 이미지 확인. 별도 Cloth Component 간 충돌을 가정하지 않고 검증된 간격을 사용한다. 천을 비활성화하지 않았다.
- 캡슐 수치의 고정점 정정: bottom particle1533(합계3225)은 waistband의 MaxDistance=0 정점이며, 확대된 Physics capsule 내부 .155cm에 위치하나 실제 몸 표면 밖이다. 고정점은 collision solver가 밀 수 없으므로 동적 입자 capsule 기준과 모든 입자의 실제 body 표면 검사를 분리했다. 실제 표면 허용 깊이 .02cm, 동적 capsule .12cm, 안정화 변위 .1cm 기준은 유지한다.
- `S9-Cloth-TShirt`: 네 포즈 1,628 동적 입자, capsule/실제 표면 내부 0, 안정화 최대 .037968cm. `S9-Cloth-DolphinShorts`: 실제 표면 내부 0이나 앉기 안정화 .101625cm로 .1cm 기준 실패. 기준을 완화하지 않고 shorts damping .25, iterations12/max16/substeps4로 보완 후 재검증한다.
- 독립 리뷰: 저장 가능 개수/용량과 load 한도 불일치, 관절 release 저장 실패 안내 누락, 완료 후 Undo 안내 잔류, Windows 저장 파일 truncate 경로를 발견했다. 128개/16MiB 사전 제한, 저장 결과 방송, 상태 기반 안내, 같은 디렉터리의 검증·flush된 임시 파일을 `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)`로 교체하는 방식으로 보완했다. 기존 파일을 먼저 삭제하는 `IFileManager::Move`는 쓰지 않는다. 임의 하드웨어 고장에 대한 보장으로 해석하지 않는다.
- `S9-Instructions-Red`: 15 성공, 실제 UI 안내 회귀 1 실패/3 assertions. 저장 실패 주입과 128작업 복원·16MB 거부 시 이전 bytes 동일 검사는 성공했다. 안내 수정 후 전체 회귀 필요.
- 관절 한도는 실제 `DA_DummyJointLimits`의 아홉 관절별 min/max로 이동했다. 기본 ±90°를 유지하고 UI/입력/저장 복원이 같은 값을 사용한다. `S9-JointLimits-Assets.log` compile/save 및 map check 오류·경고 0.
- `S9-Package.log`: 세 제품 레벨과 개발 검수 map을 포함한 첫 Win64 Development BuildCookRun 성공(exit0). 최종 리뷰 수정분을 포함한 재패키지와 실행 결과가 최종 인계 기준이다.

## 최종 인계 검증 (2026-10-16)

- `S9-Review-Green`, `S9-Final-Verification`: 저장 실패 주입·이전 파일 byte 보존, 128개 작업 복원, Undo 안내 갱신, 관절별 Data Asset 한도를 포함하여 16/16 성공·경고 0. 독립 읽기 전용 재리뷰에서 저장/안내 수정의 중대 문제 없음.
- `S9-DPIClosure-Red`: 720p 실제 전체 재단에서 닫힌 경로 마지막 0.85/0.92mm가 남는 문제를 회귀 검사 2 assertions로 재현했다. Slate float 좌표가 공유 끝점을 수십 nm 수준으로 벗어나면 시작 arc가 선택되는 원인이다. 투영 후보 간 0.001mm 이내 동률에서 이전 arc와 연속인 쪽을 선택한다. 재단 허용 .8mm, 점프 3mm, 완료 .02mm 기준은 유지했다. `S9-DPIClosure-Green`과 최종 저장 UI 이후 `S9-ReleaseVerification` 모두 16/16, 경고 0.
- `S9-FinalLayout-Assets`: 기존 제작 위젯을 보존하여 왼쪽 단계/가운데 작업대/오른쪽 안내/하단 도구로 재배치한 실제 WBP 저장. `S9-Release-Complete720`, `1080`, `1440`: Win64 패키지에서 두 옷본의 모든 재단·봉제·완성 자격·상하의 저장을 실제 렌더된 Slate hit-test/capture/bubble 포인터 경로로 완료. 각 실제 PNG 해상도와 화면 배치를 확인했다.
- `S9-Release-Fitting1080`, `720`, `1440`: 새 패키지 프로세스에서 완성 작업 복원, 실제 카메라 회전/휠/초기화, 네 포즈 버튼 및 버튼 조작 중 카메라 정지 통과. 완료 작업을 불러오는 캡처 설정은 자격 있는 두 의상을 자동 장착한다. 저장된 장착 플래그 자체의 엄격한 복원·Undo 회귀는 `Session.SaveRestoreUndo`/`Navigation.AllRoutesPIE`가 검증한다. 플레이어 슬롯과 분리된 `LaceLoft_UICapture_*`만 사용했다.
- 사람이 마우스로 전체 경로를 직접 그린 수동 플레이로 표현하지 않는다. 실제 렌더링·Slate 입력 자동화와 캡처 이미지 시각 검수로 검증했으며 NullRHI 계약 검사와 구별한다.
- `S9-Stable-DolphinShorts`: 수정된 damping .25/iterations12/max16/substeps4로 네 포즈 안정화 최대 .014642cm, 동적 capsule 깊이 .119937cm 이하, 실제 body 표면 내부 표본 0. `S9-Stable-Both`: 같은 최종 애셋으로 네 포즈 body 표면 내부 표본 0, 의상 간 삼각형 교차 0. `S9-Stable-OriginalRegression`: 원래 개발용 천 914개/892개 동적 입자의 두 포즈 회귀 통과.
- `S9-Release-ClothBoth`: Win64 패키지의 네 포즈에서 3,489개/3,105개 동적 입자, 안정화 변위 최대 .093435cm(<.1), 동적 capsule 깊이 최대 .119937cm(<.12), 비유한 값/폭발 없음. 정·측·후면 12장과 `Overview.png` 검수. 에디터 전용 CPU 표면 추출 결과를 패키지에서 실행한 것으로 혼동하지 않는다.
- 성능: `S9-Release-Fitting1080/performance.json`, Ryzen 9 7900 / RTX 4060 / Win64 Development / 실제1920×1080 / UMG와 두 Chaos Cloth 동시 실행. 워밍업 뒤 565프레임 평균 **6.2535ms**, p95 **12.4816ms**, 프로세스 peak resident **1175.13MiB**. 상의 iterations8/max12/substeps3, 하의12/16/4. 프레임 제한 없는 RenderOffscreen의 짧은 측정에서 16.67ms 목표를 충족했으며 모든 PC·장시간 실행·화면 표시 지연의 보장이 아니다.
- 타이틀 마지막 시각 검수에서 비어 있는 옷본 목록을 발견했다. `S9-TitleOptions-Red`는 저장 후 재로드된 WBP의 옵션/선택값 2 assertions 실패로 재현했다. UComboBoxString의 AddOption은 transient Options만 바꾸므로 Designer DefaultOptions를 저장하도록 보완했다. 첫 Green의 기본 선택 검사는 Slate RebuildWidget 전의 transient 선택값을 읽어 실패했으며, 실제 위젯 생성 후 확인하도록 검사 생명주기를 바로잡았다. `S9-TitleOptions-Green2`: Editor/Game 빌드 및 16/16, 경고 0. `S9-TitleOptions-Package.log`: 최종 BuildCookRun 성공(exit0), `S9-Release-TitleFixed`: 패키지의 한국어 옷본 선택 표시 확인. 마지막 변경은 타이틀 저작/애셋과 테스트뿐이며 제작·착의·물리 런타임과 해당 애셋은 앞선 패키지 검증 그대로다.

### 결과물과 남은 한계

- 실행: `Saved/LaceLoftPackage/Windows/LaceLoft.exe`. 실행 파일만 옮기지 말고 `Windows/` 전체를 함께 유지한다. UE 편집 원본: `LaceLoft.uproject`, `Source/`, `Content/`, `Config/`, `ArtSource/`.
- `FinalAssets.json`: 최종 원본/UE 애셋 66개의 경로·크기·SHA256. UV/도색 검수는 `ArtSource/Dummy/QA/`, 최신 의상 12뷰는 `Saved/Automation/Implementation/S9-Release-ClothBoth/`, 전체 검증 보고서는 같은 `Implementation/`의 위 실행 이름에 있다.
- 제공한 네 프리셋은 검증했다. 임의 관절 ±90° 조합 전부의 천 자기 충돌/의상 간 충돌을 보장하지 않는다. 별도 컴포넌트 의상 간 동적 충돌 대신 검증된 간격을 사용하며 극단적인 사용자 포즈에는 추가 검수가 필요하다.
- 사용자 범위의 미완료 기능이나 알려진 차단 오류는 없다. 시각 검수는 에이전트가 실제 렌더 이미지를 확인했고 인간의 수동 플레이 승인으로 표현하지 않는다. 출시용 Signing/Installer/다른 PC 성능 보장은 이번 범위에 포함되지 않는다.
- 독립 LaceGarment 플러그인 호스트·라이선스/이용조건·Git 최초 커밋은 이 게임 구현에서 변경하지 않았다.

## REQ-014 — 즉시 완성 치트 버튼

- `WBP_GarmentCreation`의 하단 `CreationToolbar`에서 보기 초기화 오른쪽에 `CompleteCheatButton`을 저장했다. 흰 글자/붉은 기본색, 더 밝은 hover/어두운 pressed 색으로 다른 도구와 구별한다. 레이아웃·색·툴팁은 Designer에서 편집 가능하다.
- 현재 작업의 모든 재단·봉제 경로를 채운 뒤 기존 모델로 완료 자격을 검증하고 하나의 Undo 명령으로 저장한다. 다른 작업과 조각 위치는 유지된다. Undo는 직전 진행을 복원하고 장착 자격을 취소하며 Redo도 지원한다. 작업이 없거나 이미 완성이면 비활성화된다.
- `CheatComplete-Red`: 기존 15 성공, 실제 UI 치트 버튼 누락 1 실패. `CheatComplete-Green`: UE 5.8 Editor/Game 빌드와 전체 16/16 성공·경고 0. 두 옷본 완성/다른 작업 보존/하나의 명령/저장 복원/Undo 시 착장 해제/Redo를 확인했다.
- 최초 애셋 저작 실행은 신규 필수 BindWidget을 추가하기 전 로드 단계 오류를 기록했지만 애셋 저장을 완료했다. `CheatComplete-AssetsReload`: 저장된 애셋 재로드·Compile/Save 오류 0/경고 0. `CheatComplete-UI720`: 실제1280×720에서 버튼 색상·위치·잘림 검수 및 기존 입력 회귀 통과.
- `CheatComplete-Package.log`: Win64 Development 재패키지 성공(exit0). `CheatComplete-PackageUI`: 실제1920×1080 패키지에서 새 버튼 표시·가독성 및 기존 Slate 입력 통과. 기존 배포 실행 경로에 반영했다.

## REQ-015 — 렌더 유지 와이어프레임 보기

- 입어보기 왼쪽 하단에 실제 `WBP_FittingRoom`의 `WireframeButton`과 상태 캡션을 저장했다. 기본 꺼짐이며 켜짐/꺼짐을 화면에서 전환한다.
- `M_WireframeOverlay`는 양면 translucent/unlit/wireframe 재질이며 깊이 검사를 유지한다. 인형과 두 Chaos Cloth Component의 OverlayMaterial에만 적용하므로 원래 재질/조명, 포즈와 천 시뮬레이션이 유지된다. 같은 변형된 렌더 정점으로 삼각형 선을 그린다. 외부 디버그 view mode에 의존하지 않는다.
- `Wireframe-Red`: 실제 토글 누락으로 15 성공/1 실패. `Wireframe-Green`, 최종 양면 재질 이후 `Wireframe-Final`: 전체16/16·경고0. 켜기/끄기, 세 메시 오버레이, 원래 재질과 천 시뮬레이션 유지 검사 포함. Editor/Game 빌드는 `Wireframe-TwoSided-Build2.log` 성공.
- 최초 저작 실행은 로드 시 신규 BindWidget 누락 오류 뒤 실제 위젯 저장을 완료했다. 재로드 저작은 성공했다. 첫 GPU 검수는 단면 오버레이여서 의상 앞면 선을 표시하지 못했다. 양면으로 수정하고 선 색/불투명도를 낮춘 `Wireframe-TwoSided1080`에서 몸/티셔츠/쇼츠의 삼각형 선과 원래 색·명암을 함께 확인했다. 에디터 저작 코드의 TObjectPtr range 타입 컴파일 오류 1회는 명시적 포인터 타입으로 수정 후 빌드 성공했다.
- 실제 Slate 입력 검수는 토글 켜기→끄기→켜기와 기존 카메라/네 포즈 버튼을 실행한다. `Run-UI-Capture.ps1 -Map L_FittingRoom -Wireframe`로 재현할 수 있다. 고밀도 인형은 멀리서 선이 촘촘하므로 기존 카메라 줌으로 살펴볼 수 있다. 오버레이는 추가 렌더 패스이므로 꺼진 상태와 GPU 비용이 같지는 않다.
- 최종 Win64 패키지: `Wireframe-Package2.log` 성공(exit0). 최초 실행은 다른 UAT 프로세스와 잠금 충돌하여 종료했으며 공식 `-WaitForUATMutex`로 대기 후 정상 완료했다. `Wireframe-PackageOn720`(켜기/끄기/켜기 실제 입력, 중립), `Wireframe-PackageOff1080`(기본 렌더, 앉기) 모두 통과하고 실제 PNG를 확인했다. 최종 원본/UE 애셋 manifest는 새 오버레이를 포함해 67개다.
