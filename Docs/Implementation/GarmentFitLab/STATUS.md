# GarmentFitLab 구현 기록

근거: REQ-021~022, [설계](../../Design/GARMENT_FIT_LAB_DESIGN.md), [계획](../../Plans/2026-10-17-garment-fit-lab.md).

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
