# UMG 제작 지침

## 사용자 원칙

> 내가 지시하지 않는다면 클래스 코드 내부에서 위젯 트리를 구성하지 않는다 항상 위젯 에디터에서 보면서 수정 가능한 애셋 상태여야한다

## 적용 규칙

- 모든 화면은 실제 `Widget Blueprint` (`WBP_*.uasset`)로 저장합니다.
- 패널, 버튼, 텍스트, 배치, 스타일, 기본 표시 상태는 UMG Designer에서 확인하고 수정할 수 있어야 합니다.
- C++ 생성자, `RebuildWidget`, `NativeConstruct`, `NativePreConstruct` 등에서 `WidgetTree`, `ConstructWidget`, `NewObject` 또는 Slate로 UI 계층을 만들거나 교체하지 않습니다.
- C++은 저장된 WBP 클래스의 인스턴스 생성/표시, `BindWidget` 참조, 버튼 이벤트, 입력 모드, 레벨 전환을 담당할 수 있습니다.
- 의상 제작 확장에서는 저장된 WBP 안의 옷본 뷰가 데이터에 따라 도형/진행 상태를 표시할 수 있습니다. 이는 위젯 계층 생성 허용을 뜻하지 않습니다. 뷰의 위치·크기·스타일은 Designer에서 편집하고 예제 옷본도 Designer에서 미리 볼 수 있어야 합니다.
- 애셋이 없을 때 코드로 UI를 만드는 대체 경로를 넣지 않습니다. 누락을 명확하게 보고하고 애셋을 복구합니다.
- 사용자가 명시적으로 요청하지 않는 한 위 규칙의 예외를 추가하지 않습니다.

## 현재 애셋과 바인딩

| 레벨 | UMG 애셋 |
| --- | --- |
| `/Game/Maps/L_Title` | `/Game/UI/WBP_Title` |
| `/Game/Maps/L_GarmentCreation` | `/Game/UI/WBP_GarmentCreation` |
| `/Game/Maps/L_FittingRoom` | `/Game/UI/WBP_FittingRoom` |

세 WBP는 각각 `LaceLoftTitleWidget`, `LaceLoftCreationWidget`, `LaceLoftFittingWidget`을 부모로 하며 공통 `LaceLoftNavigationWidget`을 상속합니다. `TitleButton`, `CreationButton`, `FittingButton`은 필수 `BindWidget` 이름이며 모두 `Button` 타입입니다. 이름이나 타입을 바꿀 때는 C++ 바인딩과 검증도 함께 수정합니다. 현재 레벨에 해당하는 버튼은 Designer 기본값으로 비활성화되어 있습니다.

## 확인 사항

1. WBP를 열어 Designer의 Hierarchy와 실제 화면을 확인합니다.
2. Blueprint Compile/Save가 성공하고 필수 바인딩 오류가 없는지 확인합니다.
3. Play에서 세 레벨 간 이동, 돌아오기, 반복 이동을 확인합니다.
4. C++ 클래스에 UI 계층 생성 코드가 추가되지 않았는지 확인합니다.
