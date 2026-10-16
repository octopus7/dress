# GarmentFitLab 구현 기록

근거: REQ-021~023, REQ-025, [설계](../../Design/GARMENT_FIT_LAB_DESIGN.md), [계획](../../Plans/2026-10-17-garment-fit-lab.md).

## 현재 — REQ-025 소매 흔적을 정리한 마네킹 입력 (2026-10-17)

`/Game/Maps/L_GarmentFitLab`의 입력을 **SKM_LunaMk2_Mannequin**으로 교체했다. 기존 Mk2 메시·스켈레톤·무광 재질과 외부 제공 원본은 보존했다. 단계 1의 입력 개선만 수행했으며 단계 2는 시작하지 않았다.

- 별도로 연결된 옷깃·패드·가슴 장식 8개 표면과 원본의 미사용 정점을 제거했다. 몸과 연결된 소매는 단순 삭제하지 않고 어깨–팔꿈치 뼈 위치를 기준으로 팽창된 표면과 소매 끝단을 정리했다.
- 연결된 마네킹 표면을 만들고 어깨·팔·몸통의 경계와 표면을 매끈하게 다듬었다. 원본 가짜 머리, 기준 포즈, 손·다리 및 발의 기본 형상은 유지했다. 의상 아래 실제 피부를 복원했다고 가정하지 않는 저작 파생본이다.
- 출력: **37,507정점 / 75,010삼각형**, 열린/비다양체 모서리 0, 퇴화 면 0, 골격 가중치 누락 정점 0. 소매 흔적을 제거하기 위한 표면 재구성으로 원본 토폴로지와 UV는 유지하지 않는다.
- 실제 UE 크기: 높이 **135.635cm**, 좌우 폭 **57.254cm**, 앞뒤 깊이 **22.106cm**. 기존 스켈레톤을 공유하며 131개 뼈의 이름·부모·기준 포즈가 최초 입력과 일치한다. 애니메이션 변형 품질은 이번 범위에 포함하지 않는다.
- 기존 `M_LunaMk2_Matte`를 그대로 사용했다. 텍스처는 없다. 파생 `.blend`에도 메시 1개와 armature 1개만 저장했으며 image/material 데이터블록은 0개다. UE 임포트에서 UV가 없어 기본 UV를 생성했다는 경고 1건이 있었으며 단색 재질 표시에 영향은 없다.
- 편집 원본: `Content/GarmentFitLab/Source/SKM_LunaMk2_Mannequin.blend`, 교환 원본: 같은 폴더의 `.fbx`. UE 애셋: `Content/GarmentFitLab/Characters/LunaMk2/SKM_LunaMk2_Mannequin.uasset`.
- 재현 스크립트: `Plugins/GarmentFitLab/Content/Python/create_luna_mannequin.py` (제공 원본을 별도 Blender 프로세스로 열어 파생본 저작), `import_luna_mannequin.py` (신규 애셋 임포트와 전용 레벨 지정). 이미 존재하는 파생 애셋은 자동 덮어쓰지 않는다.
- UE 5.8.3 Editor 빌드 성공: `Saved/GarmentFitLab/build-mannequin.log`.
- **GarmentFitLab.Input 4개 + LaceLoft 16개 = 20/20 성공**. `failed=0`, `notRun=0`, 예상 누락 경로 경고 1건. 결과: `Saved/GarmentFitLab/mannequin-report/index.json`. 기존 입력 보존, 파생본 재열기, 골격·기준 포즈 유지, 재질, 실제 렌더 표면과 레벨 지정 검증을 포함한다.
- Blender 저장본 재열기에서 모서리·면·가중치·텍스처 부재를 재확인했다: `Saved/GarmentFitLab/mannequin-source-reopen.log`. 정면·측면·후면 저작 검토 이미지: `mannequin-derived-{front,side,back}.png`.
- 실제 UE Editor에서 저장된 레벨을 재열어 정면·측면·후면의 소매 제거와 어깨–팔 연결을 확인했다: `Saved/GarmentFitLab/mannequin-{front,side,back}.png`.

## 단계 1 최초 Mk2 입력 — 이전 확인 결과 (2026-10-17)

`/Game/Maps/L_GarmentFitLab`을 열고 Play하면 사용자가 제공한 **SKM_LunaMk2**가 기준 포즈로 표시된다. 입력 메시, 골격, 새 재질은 `/Game/GarmentFitLab/`에만 저장했다. 단계 2는 시작하지 않는다.

### 제공 입력과 재질

- 원본: `D:/github/extraction_shooter/Blender/SKM_LunaMk2.blend`, Blender 4.5.12 LTS. SHA-256 `9DA0A1A696D69CD649CD4E87ACFF2A1EDBCAB497D478ECA49CE9FC68F1AC0640`.
- `SKM_LunaMk2` 메시와 연결된 `SK_LunaMk2` 골격만 선택하여 내보냈다. 별도 Eye/Head/SideTail/Face 메시는 제외하고 Mk2 내부의 가짜 머리는 유지했다. 원본 blend는 저장하거나 수정하지 않았다.
- 원본 3,808정점/6,077면 중 표면 사용 정점은 3,115개, 면에 사용되지 않는 정점은 693개다. 연결된 표면은 9개이며 큰 몸 표면은 2,760정점이다. 이 정보는 원본 점검 결과이며 메시를 정리·재모델링한 것이 아니다.
- **몸과 옷이 완전히 분리된 순수 바디 입력은 아니다.** Mk2 자체에 어깨·상의·장식·신발 형태가 포함되어 있다. 이를 임의로 제거하지 않았다. 향후 체형 분석에서 이 표면을 맨몸 치수로 바로 해석할 수 있는지는 별도 판단이 필요하다.
- 원본 m 단위를 UE cm로 변환했다. 실제 렌더 표면 크기: **높이 135.696cm, 좌우 폭 57.315cm, 앞뒤 깊이 22.893cm**. 원본 Z-up/앞 -Y → UE Z-up/앞 +X이며 액터 스케일은 1이다. 발 최저점은 Z=-0.376cm이다.
- 팔을 아래로 벌린 원본 참조 포즈를 표시한다. 원본 130개 뼈와 FBX armature 변환 루트 1개로 UE에는 131개 뼈가 있다. 애니메이션·MorphTarget·PhysicsAsset은 가져오지 않았다.
- 새 `M_LunaMk2_Matte`: Base Color 선형 RGB `(0.50, 0.58, 0.57)` (대략 sRGB `#BCC8C7`), Metallic `0`, Roughness `0.9`, Specular `0.25`. 모든 메시 재질 슬롯에 적용했다. 원본 텍스처/재질은 가져오지 않았으며 새 재질의 텍스처 사용 수는 0이다.
- 신규 UE 애셋은 메시·스켈레톤·재질 3개다. 선택 내보낸 FBX는 `Content/GarmentFitLab/Source/SKM_LunaMk2.fbx`에 보존한다. 실제 UI WBP와 런타임 C++는 이번 입력 작업에서 변경하지 않았다.

### 검증 결과

- UE 5.8.3 `LaceLoftEditor Win64 Development` 빌드 성공: `Saved/GarmentFitLab/build-mk2.log`.
- **GarmentFitLab.Input 3개 + LaceLoft 16개 = 19/19 성공**. JSON `failed=0`, `notRun=0`, 모든 state `Success`. 누락 경로 검사에서 예상 로드 경고 1건. 결과: `Saved/GarmentFitLab/mk2-verified-report/index.json`, `mk2-verified-tests.log`.
- 실제 디스크 애셋 재로드로 Mk2 참조·표면 크기·독립 골격·새 재질의 모든 슬롯 지정·텍스처 미사용을 검증했다. 새 입력 폴더에 SkeletalMesh 1개, Skeleton 1개, Material 1개만 있으며 Texture 애셋은 없다.
- 기준 포즈의 root/pelvis/head/hand_l/ball_l 위치를 원본 좌표 변환과 비교해 최대 약 **0.000022cm** 오차를 확인했다. 원본 앞/위 방향과 표시 크기가 일치한다. 결과: `Saved/GarmentFitLab/mk2-reopen.json`.
- FBX 최초 가져오기에서 bind pose에 모든 deformer가 포함되지 않았다는 경고 1건이 있었다. 참조 포즈의 주요 뼈 위치·렌더 표면 범위·실제 화면은 확인했으나 다른 애니메이션에서의 변형 품질까지 검증한 것은 아니다.
- UE 에디터 Play에서 실제 WBP의 준비 상태/입력 경로와 캐릭터 표시 확인. 정면·측면·후면 확인. 화면: `Saved/GarmentFitLab/mk2-{play,front,side,back}.png`. 정면을 비추도록 기존 테스트 조명과 시작 시점만 조정했다.
- 검증 중 발견한 저장 문제를 수정했다. Python 구조체 배열 항목은 수정 후 배열에 재할당하며, 메시 패키지는 `only_if_is_dirty=False`로 명시 저장한다. 화면상 적용만으로 완료 판정하지 않고 저장본 자동화를 통과시켰다.
- 별도 읽기 전용 검토에서 입력 선택·원본 불변·텍스처 제외·독립 경계를 점검했다. 검토 당시 재질 저장 미확인 지적은 위 저장 수정과 최종 자동화로 해소했다.
- 변경 범위는 이번 GarmentFitLab 입력/전용 레벨/저작 스크립트/자동화/요청 및 구현 기록이다. 기존 게임/LaceGarment/다른 실험 파일은 수정·커밋하지 않는다.

### 재현

`Plugins/GarmentFitLab/Content/Python/export_luna_mk2.py`를 원본 blend가 로드된 별도 Blender background 프로세스에서 실행하고, `import_luna_mk2.py`를 UE editor Python으로 실행한다. 열린 UE와 별도 commandlet에서 같은 애셋을 동시에 저장하지 않는다. 이후 아래의 자동화 재현 명령을 사용한다. Blender는 이 제공 파일의 일회성 입력 변환 도구이며 플러그인 실행 의존성이 아니다.

---

아래는 실제 입력 제공 전 기반 커밋 `96b1f47` 시점의 기록이다. 당시 미검증 항목은 위의 현재 결과로 갱신되었다.

## 단계 1 — 검증된 입력 기반, 실제 캐릭터 대기 (2026-10-17)

- 사용자 캐릭터 메시가 아직 제공되지 않았다. 실제 캐릭터 표시·단위·방향·기준 포즈·골격·몸 표면 분리는 미검증이다.
- 이번 작업은 새 플러그인, 입력 검증, 테스트 레벨과 실제 WBP만 다룬다. 체형 분석·옷본·봉제·Cloth는 구현하지 않는다.
- 기존 게임과 LaceGarment의 코드·애셋·작업 중 변경을 보존한다.
- REQ-022에 따른 기반 중간 커밋 뒤 입력과 사용자 결과 확인을 기다린다. 단계 2로 자동 진행하지 않는다.

## 구현된 기반

- `Plugins/GarmentFitLab/`에 Runtime/Editor 모듈을 새로 만들었다. 게임 모듈이나 기존 LaceGarment 모듈에 의존하지 않는다.
- `/Game/Maps/L_GarmentFitLab`에는 미리보기 액터 하나, 기본 엔진 바닥, 동적 조명, 관찰 시작 위치만 저장했다. 캐릭터 대체물을 넣지 않았다.
- `GarmentFitPreview`의 **Input Mesh**에 StaticMesh 또는 SkeletalMesh 애셋을 지정한다. 입력은 레벨에 저장되며 플러그인 코드에 특정 프로젝트 메시 경로를 고정하지 않는다.
- 입력 없음, 경로 로드 실패, 잘못된 타입, 표시할 표면 없음, 표시 준비됨을 구분한다. 입력 교체/제거/실패 시 이전 메시를 두 컴포넌트에서 모두 지운다.
- SkeletalMesh는 후처리 Anim Blueprint를 끄고 단일 애니메이션을 비워 참조 포즈 표시를 준비한다. 실제 사용자 골격·표면·포즈는 아직 검증하지 않았다.
- `/GarmentFitLab/UI/WBP_GarmentFitLab`은 실제 저장된 Widget Blueprint다. Canvas → Border → VerticalBox → TextBlock 계층, 크기·여백·색상·기본 안내 문구를 Designer에서 편집할 수 있다. C++은 `InputStatusText`와 `InputAssetText` 바인딩, 저장 WBP 인스턴스 표시만 담당한다.
- 새 레벨에만 `GarmentFitLabGameMode`를 지정했다. `.uproject`에는 플러그인 활성화 항목만 추가했다.

## 사용자가 확인하는 방법

1. UE 5.8에서 `Content/Maps/L_GarmentFitLab`을 연다.
2. Play하면 **캐릭터 메시 대기 중 / 입력 애셋: 없음** 안내와 빈 미리보기 공간이 나온다.
3. 제공받은 메시를 임포트할 때는 새 `Content/GarmentFitLab/` 아래에 둔다. 기존 메시를 이번 입력으로 가정하지 않는다.
4. Play 종료 후 Outliner의 `GarmentFitPreview`를 선택하고 Details에서 **Input Mesh**를 검색해 애셋을 지정한다. **Refresh Input**으로 다시 불러올 수도 있다.
5. 플러그인 콘텐츠 표시를 켜고 `Garment Fit Lab Content/UI/WBP_GarmentFitLab`을 열면 화면을 Designer에서 편집할 수 있다.

캐릭터가 제공되면 실제 크기(cm)·앞/위 방향·기준 포즈·골격 유무·몸 표면 분리를 먼저 확인한다. 입력이 준비되었다는 표시는 체형 자동 분석 성공을 뜻하지 않는다.

## 검증 결과와 증거

- 설치 엔진: `E:/WorkTemp/Epic Games/UE_5.8`, **5.8.3 / CL 58210709**.
- `LaceLoftEditor Win64 Development` 빌드 성공. 로그: `Saved/GarmentFitLab/build-final.log`.
- 자동화 필터: `GarmentFitLab.Input+LaceLoft`. 새 입력 검사 2개와 기존 게임 검사 16개, 총 18개 성공. 의도적으로 없는 애셋 경로를 읽는 검사에서 예상 로드 경고 1건, 오류 0건. 결과: `Saved/GarmentFitLab/final-report/index.json`, `final-tests.log`.
- `-DisablePlugins=GarmentFitLab`으로 실행한 기존 `LaceLoft` 자동화도 16/16 성공, 경고·오류 0건. 결과: `Saved/GarmentFitLab/plugin-off-report/index.json`.
- `ValidationAndReplacement`: 누락·잘못된 타입·빈 static/skeletal·뼈대/소스 LOD만 있는 빈 표면·정상 엔진 primitive 입력·잘못된 입력으로 교체·입력 제거·깨진 경로·후처리 애니메이션 차단 검사. 엔진 Cube는 메모리 내 자동화 입력에만 사용했으며 레벨에 저장하지 않았다.
- `SavedAssets`: 레벨/WBP 재로드, 컴파일된 상태, Designer 계층, 실제 WBP 인스턴스의 상태/경로 바인딩, 유일한 미리보기 액터, 빈 입력, 독립 GameMode와 WBP 참조 검사.
- 실제 UE Editor에서 WBP 재열기·Compile/Save 성공, Hierarchy와 `InputStatusText`의 편집 가능한 Details를 확인했다. Play에서 누락 안내와 관찰 시점 확인, 종료 후 재실행을 확인했다. 화면 기록: `Saved/GarmentFitLab/designer.png`, `play.png`.
- 에디터 Python으로 새 레벨을 재열어 저장하고 패키지 참조를 점검했다. 새 레벨에는 엔진과 GarmentFitLab 참조만 있으며 기존 게임/LaceGarment 애셋 참조가 없다. 결과: `Saved/GarmentFitLab/asset-dependencies.json`.
- 새 소스/애셋 외 기존 `Source/`, `Config/`, 기존 `Content` 및 기존 구현 문서에는 이 작업의 변경이 없다. 다른 대화의 미커밋 파일은 커밋에 포함하지 않는다.
- 실행하지 않은 것: 실제 사용자 메시 표시/정면·측면·후면 검사, 체형 분석, 봉제·Cloth, 배포/cook. 단계 1 전체 완료로 처리하지 않는다.

검증 로그와 화면 기록은 로컬 `Saved/` 생성물이므로 Git에 포함하지 않는다. 실제 `.uasset`/`.umap`은 원본으로 커밋한다.

## 재현 명령

```powershell
& 'E:\WorkTemp\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' LaceLoftEditor Win64 Development '-Project=D:\github\dress\LaceLoft.uproject' -WaitMutex -NoHotReloadFromIDE
& 'E:\WorkTemp\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\github\dress\LaceLoft.uproject' -unattended -nop4 -nosound -NullRHI '-ExecCmds=Automation RunTests GarmentFitLab.Input+LaceLoft' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/github/dress/Saved/GarmentFitLab/recheck-report'
```

UE의 종료 코드만으로 테스트 성공을 판정하지 않는다. 내보낸 JSON에서 `failed == 0`, `notRun == 0`, 각 테스트의 `state == Success`를 확인한다.

`Plugins/GarmentFitLab/Content/Python/create_input_assets.py`는 최초 에디터 저작 스크립트이며 기존 레벨/WBP를 덮어쓰지 않는다. 런타임 UI 대체 경로가 아니며 이후 화면 편집은 저장 WBP에서 한다.

## 작업 판단 기록

- Ruling: 지정 작업 디렉터리 `D:/github/dress`와 공유 브랜치를 유지한다. 사용자 지시에 따라 브랜치 전환·별도 worktree·전체 스테이징 없이 명시적 파일만 커밋한다.
- Ruling: 입력 형식 확정 전에는 StaticMesh와 SkeletalMesh의 기본 표시만 지원한다. SkeletalMesh는 기준 참조 포즈로 표시하며 자동 체형 분석 지원을 의미하지 않는다.
- Ruling: WBP는 에디터 저작 스크립트로 저장하고 Designer에서 검증한다. C++ 클래스는 저장된 위젯 바인딩과 표시만 담당하며 위젯 계층을 생성하지 않는다.
- Pre-flight: 이번 활성 범위는 단계 1 하나다. 후속 단계의 데이터 모델은 이번에 만들지 않는다.
- Review: 별도 읽기 전용 Astra/High 검토에서 지적한 skeletal 표면 검사와 참조 포즈 후처리 문제를 실제 실패 테스트로 재현하고 수정했다. `Saved/GarmentFitLab/review-red-report/`에 실패 증거를 보존했다.
- Ruling: 실제 메시 품질, 런타임 파일 선택기, 다중 미리보기/멀티플레이, Cloth·분석·배포는 이번 기반 검토 범위 밖이다. 실제 입력 확인은 단계 1의 남은 일로 유지한다.
- Visual fix: Python Rotator 위치 인자의 순서로 시작 시점이 틀어진 것을 Play에서 발견했다. 명명된 인자와 미리보기 지점을 향하는 LookAt 회전으로 고친 뒤 재저장·재실행했다.
