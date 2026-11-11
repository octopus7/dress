# GarmentLab

Unreal Engine 5.8 C++ 프로젝트. `GarmentLab.uproject`를 열면 더미 타이틀 레벨 `Content/Maps/MainMap.umap`이 열립니다.

- UI: `Content/UI/WBP_TitleScreen.uasset`의 UMG Designer에서 편집합니다.
- 부모 클래스: `UTitleScreenWidget`. C++은 종료 버튼 이벤트만 연결합니다.
- `ATitlePlayerController`가 저장된 WBP를 표시하고 UI 입력 모드를 설정합니다.
- `ATitleGameMode`는 더미 레벨에 플레이어 폰을 생성하지 않습니다.
- 런타임에서는 트리 노드를 구성하지 않습니다. 최초 구성에 사용한 임시 에디터 코드는 저장 후 삭제하며, 레이아웃·텍스트·색상은 편집 가능한 WBP에 저장합니다.
- `ExitButton`은 C++ 바인딩 이름입니다. 종료 버튼은 게임을 종료하며 PIE에서는 플레이를 종료합니다.

검증: UE 5.8 Editor 빌드, `GarmentLab` 자동화, WBP 저장/다시열기 및 Play 화면 확인.

자동화 실행: `Tools/Verify.ps1 -EngineRoot '<UE 5.8 설치 경로>'`.
검증 보고서는 `Saved/Verification/`에 생성됩니다.
