# LaceLoft

Unreal Engine **5.8** C++ 프로젝트입니다. 반팔 티셔츠와 돌핀쇼츠의 지정선을 재단·봉제하고, 15cm 흰 인형에 입혀 포즈를 조절합니다. 타이틀·의상 제작·입어보기의 기존 세 레벨과 실제 UMG 애셋을 확장했습니다.

## 열기

검증된 Win64 Development 실행본은 `Saved/LaceLoftPackage/Windows/LaceLoft.exe`입니다. 이동할 때는 `Windows/` 폴더 전체를 유지합니다.

1. UE 5.8과 Visual Studio 2022의 Unreal C++ 빌드 도구를 설치합니다.
2. `LaceLoft.uproject`를 엽니다. 빌드 안내가 나오면 프로젝트 모듈을 빌드합니다.
3. 기본 레벨 `L_Title`에서 **Play**를 누릅니다.
4. 타이틀에서 옷본을 선택하고 **새로 제작**을 누릅니다. 기존 작업은 **이어 하기**로 엽니다.
5. 제작 화면에서 조각을 선택하고 마우스를 누른 채 외곽선을 따라 재단합니다. 녹색이 완료 구간입니다. 휠은 확대/축소, 오른쪽/가운데 버튼 드래그는 작업대 이동입니다. 재단을 마친 조각은 내부를 드래그해 정리할 수 있습니다.
6. 가능한 봉제 단계를 선택하고 주황 선을 화살표 방향으로 따라갑니다. 파란 짝 선과 반투명 목표 윤곽이 조립 위치를 안내합니다. 경로 밖 이동이나 큰 커서 점프는 완료량에 더하지 않습니다.
7. **입어보기**의 완성 의상 목록에서 상·하의를 각각 장착합니다. 가운데 영역 드래그/휠로 카메라를 조작하고, 네 포즈 버튼과 관절 슬라이더를 사용합니다.

입어보기 왼쪽 아래의 **와이어프레임: 꺼짐/켜짐** 버튼은 재질과 조명을 유지하면서 인형·착용 의상의 렌더 메시 선을 겹쳐 표시합니다. 포즈와 천 변형을 그대로 따라가며, 다시 누르면 기본 렌더만 보입니다. 선 색과 불투명도는 `Content/Characters/Dummy/Materials/M_WireframeOverlay`에서 편집합니다.

스트로크·조각 이동이 끝날 때와 화면 이동 전에 자동 저장합니다. **되돌리기/다시 하기** 기록도 복원됩니다. 완성 후 되돌리면 해당 의상의 착장 자격이 취소되고 장착도 해제됩니다. 다른 작업은 유지됩니다. 새 작업 버튼은 기존 작업을 삭제하지 않습니다.

제작 화면 하단 도구줄 오른쪽의 붉은 **치트: 즉시 완성** 버튼은 현재 작업의 재단·봉제를 한 번에 완료하고 저장합니다. 한 번의 **되돌리기**로 치트 직전 상태를 복원할 수 있습니다. 이미 완성했거나 선택한 작업이 없으면 버튼은 비활성화됩니다.

Windows 저장 파일은 실행 환경의 `Saved/SaveGames/LaceLoftWardrobe_v1.sav`입니다. 최대 128개 작업/16MB이며 저장 실패는 화면에 표시됩니다. 임시 파일 검증·flush 후 동일 폴더 내 원자적 교체로 이전 정상 파일을 보호합니다. 손상/버전 불일치 파일은 `_recovery_...`로 보존하고 가능한 작업을 복구합니다. 원본 옷본을 수정해 hash가 달라지면 구버전 작업의 완료 자격은 유지되지 않습니다.

C++ 편집은 `LaceLoft.uproject`의 **Generate Visual Studio project files**로 솔루션을 생성한 뒤 `LaceLoft.sln`을 열고 **Development Editor / Win64**를 빌드합니다.

## 레벨과 실제 UMG 애셋

| 화면 | 레벨 | Widget Blueprint |
| --- | --- | --- |
| 타이틀 | `Content/Maps/L_Title.umap` | `Content/UI/WBP_Title.uasset` |
| 의상 제작 | `Content/Maps/L_GarmentCreation.umap` | `Content/UI/WBP_GarmentCreation.uasset` |
| 의상 입어보기(포즈 취하기) | `Content/Maps/L_FittingRoom.umap` | `Content/UI/WBP_FittingRoom.uasset` |

Content Browser에서 WBP를 열고 **Designer** 탭에서 계층, 배치, 텍스트, 스타일을 수정합니다. 세 WBP의 부모는 `LaceLoftTitleWidget`, `LaceLoftCreationWidget`, `LaceLoftFittingWidget`이며 모두 공통 `LaceLoftNavigationWidget`을 상속합니다. 필수 이동 버튼 이름은 `TitleButton`, `CreationButton`, `FittingButton`입니다.

`LaceLoftPlayerController`는 저장된 화면 애셋을 표시하고 입력을 UI로 전달합니다. 런타임 C++에 UI 계층 생성 경로는 없습니다. 옷본 뷰는 저장된 WBP 안에서 데이터 기반 선과 진행 상태만 그립니다. `WBP_PatternWorkspace`는 Designer 예제 옷본과 표시 색상/배율을 제공합니다.

`Content/Characters/Dummy/Animation/DA_DummyJointLimits`에서 관절별 회전 범위를 편집합니다. 현재 아홉 관절의 기본 범위는 각 축 ±90°입니다. 화면 슬라이더와 저장 검증이 같은 데이터를 사용합니다. 인형/Cloth/Dataflow/패턴 DA는 실제 저장된 애셋이며, 제품 실행에서 재생성하지 않습니다.

원본은 `ArtSource/Dummy/`의 Blender·FBX·치수·UV/텍스처·도색 검수 자료, `ArtSource/Patterns/`의 mm JSON, `ArtSource/Garments/`의 Blender 및 UE 정점 대응 자료입니다. 최종 인형은 `dummy-1`, 20본/19 충돌 바디/고유 UV0 16개 섬입니다.

## 검증

Editor의 **Session Frontend → Automation**에서 `LaceLoft`를 검색해 실행합니다.

- `LaceLoft.Content.AuthoredAssets`: 실제 WBP/레벨 로딩, Designer 계층, 필수 버튼 바인딩/기본 상태, Blueprint 컴파일.
- `LaceLoft.Navigation.AllRoutesPIE`: 여섯 방향 이동과 재방문, 재단·봉제 중 저장, 완성·착용·복귀·Undo 후 새 세션 복원.
- `LaceLoft.Patterns/Cutting/Sewing/Session/UI/Fitting/Dummy/Cloth`: 패턴 대응, 연속 구간·DPI·캡처 상실, 봉제 선행 조건, 손상 복구·저장 실패 보존, 실제 WBP 입력, 포즈·장착, UV·애셋 계약.

`./Tools/Verify-LaceLoft.ps1 -RunName <고유이름>`은 Editor/Game 빌드와 전체 자동화를 실행하고 `Saved/Automation/Implementation/`에 보고서를 남깁니다. `Run-ClothProbe.ps1 -Garment Both`는 별도 개발 장면에서 네 포즈를 GPU 실행하며, `Run-UI-Capture.ps1 -Complete`는 실제 화면의 Slate hit-test 경로로 두 의상을 재단·봉제합니다. 이 검증은 플레이어 저장과 다른 슬롯을 사용합니다.

몸/의상 표면 검사는 `ArtSource/Tools/validate_cloth_surface.py`, 의상 간 교차 검사는 `validate_cloth_layers.py`를 Blender background로 실행합니다. NullRHI 자동화와 실제 렌더링 검증은 별개입니다. 실행 결과·이미지·성능 수치는 [구현 상태](Docs/Implementation/STATUS.md)에 기록합니다.

명령줄 검증 예시(PowerShell, `$UE`는 설치 경로):

```powershell
$UE = (Get-ItemProperty 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8').InstalledDirectory
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\LaceLoft.uproject" -unattended -nosplash -nullrhi -nosound '-ExecCmds=Automation RunTests LaceLoft' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD\Saved\Automation\Manual"
```

프로세스 종료 코드 외에 생성된 `index.json`의 테스트 결과도 확인합니다.

2026-10-16 최종 검증: UE 5.8.3 Editor/Game 빌드, 자동화 16/16·경고 0, Win64 패키지, 720p/1080p/1440p의 실제 Slate 입력 전체 제작 및 저장 복원, 네 포즈의 천 동작을 확인했습니다. Ryzen 9 7900/RTX 4060의 1080p 착의 화면은 평균 6.25ms·p95 12.48ms였습니다. 짧은 RenderOffscreen 측정이며 일반적인 성능 보장은 아닙니다. 원본/UE 애셋 해시는 `Docs/Implementation/FinalAssets.json`, 상세 실행 근거와 검증 한계는 구현 상태 문서에 있습니다.

## 작업 문서

- [AGENTS.md](AGENTS.md)
- [UMG 제작 지침](Docs/UMG_GUIDELINES.md)
- [사용자 요청 문서](Docs/REQUESTS.md)
- [요청 기록 제외 기준](Docs/REQUEST_LOG_EXCLUSIONS.md)
- [전체 설계](Docs/Design/LACELOFT_DESIGN.md)
- [의상 제작·인형·UV 구현 계획](Docs/Plans/2026-10-15-laceloft-garment-workflow.md)

완료 범위와 실제 검증 근거는 [구현 상태](Docs/Implementation/STATUS.md)를 기준으로 확인합니다. 개발용 cloth probe는 기본 타이틀 흐름에 표시하지 않습니다. 별도 Cloth Component 사이 접촉을 자동 보장하지 않으므로, 제공한 네 프리셋 이외의 극단적인 사용자 관절 조합은 추가 충돌 검수가 필요합니다.

별도 Astra / High 대화에서 구현할 UE 5.8 독립 의상 저작·배포 플러그인(Blender 설치 불필요):

- [LaceGarment 플러그인 설계](Docs/Design/LACE_GARMENT_PLUGIN_DESIGN.md)
- [프로젝트와 무관한 의상 파일 계약](Docs/Design/LACE_GARMENT_PACKAGE_FORMAT.md)

REQ-012에 따라 이 설계를 별도 구현 대화에 전달합니다. 구현 완료나 설치 완료를 뜻하지 않으며, 기존 게임 확장과 분리한 `PluginDev/LaceGarmentHost/`에서 개발합니다.
