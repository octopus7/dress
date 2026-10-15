# LaceLoft 구현 계획

**목표:** UE 5.8 C++ 프로젝트에 세 빈 레벨과 실제 편집 가능한 UMG 이동 UI를 구성합니다.

**구성:** 타이틀, 의상 제작, 의상 입어보기(포즈)마다 저장된 WBP를 사용합니다. C++ PlayerController가 해당 WBP를 표시하고 공통 UserWidget 부모가 버튼 클릭을 레벨 이동으로 연결합니다. 게임 콘텐츠는 추가하지 않습니다.

- [x] C++ 프로젝트, 게임 모드, 컨트롤러, 이동 이벤트와 시작/쿠킹 설정 작성
- [x] 실제 `.umap` 3개와 Designer에서 편집 가능한 `.uasset` 3개 저장
- [x] UMG 원칙을 별도 문서에 기록하고 AGENTS.md에서 연결
- [x] 사용자 요청 문서와 기록 제외 기준 작성 및 AGENTS.md 연결
- [x] Editor 빌드, 애셋 검증, Play 레벨 전환 확인

검증은 실제 WBP 바인딩, 레벨 로딩, 이동 후 UI 복원을 확인합니다. 최초 애셋 제작에만 사용하는 임시 에디터 제작 코드는 결과물에서 제거하고, 사용자 편집 내용을 코드로 재생성하거나 덮어쓰는 경로를 남기지 않습니다.

## 검증 결과 (2026-10-15)

- UE 5.8.3 / Win64 Development: `LaceLoftEditor`, `LaceLoft` 빌드 성공.
- `LaceLoft.Content.AuthoredAssets`: 성공, 오류 0 / 경고 0.
- `LaceLoft.Navigation.AllRoutesPIE`: 성공, 오류 0 / 경고 0. 여섯 방향 이동과 재방문, UI 개수, 키보드 포커스, 마우스 커서 확인.
- 최종 보고서: `Saved/Automation/Final/index.json`.
- 임시 애셋 제작 코드 제거 완료. 최종 C++에는 위젯 계층 생성 코드가 없으며, 실제 애셋 6개를 `Content/`에 저장했습니다.
- 패키징/배포는 실행하지 않았습니다.
