# LaceLoft 작업 지침

- Unreal Engine **5.8**, C++ 프로젝트입니다.
- 저장소는 추후 비공개로 전환할 예정입니다. 작성자 표시와 이용조건은 현재 미정이며, 사용자의 명시적인 결정 없이 라이선스 파일·문구·배포 이용조건을 임의로 추가하거나 확정하지 않습니다.
- UMG 관련 작업 전 반드시 [UMG 제작 지침](Docs/UMG_GUIDELINES.md)을 읽고 따릅니다.
- 사용자 요청을 받으면 [요청 기록 제외 기준](Docs/REQUEST_LOG_EXCLUSIONS.md)을 먼저 확인하고, 제외 대상이 아닌 요청은 [요청 문서](Docs/REQUESTS.md)에 기록합니다. 기능, 구조, 작업 지침의 추가·변경 요청과 중요한 수정 요청을 누락하지 않습니다.
- 사소한 수치 조정과 레벨 내 사소한 배치 조정은 요청 문서에 기록하지 않습니다. 단, 사용자가 명시적으로 기록을 요청하면 기록합니다.
- 기본 세 레벨과 이동 UI는 구현되어 있습니다. 다음 확장 범위는 [전체 설계](Docs/Design/LACELOFT_DESIGN.md)와 [구현 계획](Docs/Plans/2026-10-15-laceloft-garment-workflow.md)을 따릅니다. 요청하지 않은 기능을 추가하지 않습니다.
- 설계·관리 대화는 계획, 요청 기록, 작업 검토만 담당합니다. 실제 코드·모델·애셋 구현은 별도 구현 대화에서 **Astra (`gpt-6-astra`) / High**로 진행합니다. 구현 대화에서도 이 문서와 연결된 UMG 지침을 준수합니다.
- UE 5.8 내부에서 독립 저작·저장·배포하는 의상 플러그인은 [별도 설계](Docs/Design/LACE_GARMENT_PLUGIN_DESIGN.md)와 [파일 계약](Docs/Design/LACE_GARMENT_PACKAGE_FORMAT.md)을 따릅니다. **Blender와 외부 저작툴을 필수 의존성으로 두지 않습니다.** REQ-012에 따라 기존 게임 구현 대화와 별개의 **Astra / High** 대화에서 구현을 진행합니다. 기존 LaceLoft 게임 구현의 범위를 자동으로 확장하지 않습니다.
- **REQ-017의 이번 플러그인 완료 범위는 Task 1~3(독립 호스트·패널, 봉제·Cloth, 바디·대표 의상·문서 모델)입니다.** Task 4 독립 원본 파일 저장·열기, Task 5 실제 저작 WBP/EUW, Task 6 콘텐츠 팩·배포·cook, Task 7 새 프로젝트 최종 인수는 후속 범위이며 이번에는 진행하거나 완료 조건으로 요구하지 않습니다. 기존 작업물은 보존합니다. UI 제외가 클래스 코드 내부 위젯 트리 생성을 허용한다는 뜻은 아닙니다.
- 플러그인 개발은 `PluginDev/LaceGarmentHost/`의 독립 호스트와 그 아래 플러그인에서 진행하고, 진행 기록은 `Docs/Implementation/LaceGarment/`에 둡니다. 기존 게임 대화가 작업 중인 루트 프로젝트의 `Source/`, `Content/`, `Config/`, `.uproject`와 `Docs/Implementation/STATUS.md`는 수정하지 않습니다. 명시적인 후속 통합 요청 전까지 독립 개발 경계를 유지합니다.
- UI는 `Content/UI/`의 실제 Widget Blueprint 애셋에서 편집합니다. C++은 입력 이벤트와 화면/레벨 전환을 담당합니다.
- `Content/`의 `.uasset`, `.umap`은 프로젝트 원본입니다. 빌드 생성물로 취급하거나 삭제하지 않습니다.
- 게임 C++ 또는 애셋 변경 후 UE 5.8 Editor 빌드와 `LaceLoft` 자동화 검증을 실행합니다. 독립 플러그인 변경은 해당 호스트의 Editor 빌드·`LaceGarment` 자동화·변경에 필요한 UE 애셋 저장/재열기 및 착장·포즈 검증을 실행합니다. REQ-017에 따라 새 프로젝트 최종 인수와 배포·cook 검증은 이번 필수 검증에서 제외합니다. 실행하지 않은 검증을 통과했다고 보고하지 않습니다.
