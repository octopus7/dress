# LaceGarment 문서/API 경로

REQ-017의 Task 1~3 개발 경로이다. 실제 저작 UI와 `.lgarment` 파일 저장은 후속 범위이며, 아래 호출은 독립 호스트의 Editor 모듈에서 사용한다.

## 생성 순서

1. `ULaceGarmentDocument`의 조각·직선/cubic·봉제·고정점을 편집한다. `ApplyTemplate`은 같은 문서의 안정 ID와 수동 보정 원본을 유지한다. 점 이동은 `MovePoint`를 사용하고, 다른 원본 필드의 직접 편집은 UE transaction의 `Modify()` 후 `InvalidateDerived()`를 호출한다.
2. `SampleDocument`로 mm 옷본과 경계 원본 대응을 만들고, `ReadBodyCollision`의 capsule+convex overload로 실제 저장 바디의 component-space cm 충돌체를 읽는다.
3. `BuildGarment`로 안정화한다. 실패 결과는 진단용이며 Cloth 저장 대상으로 사용하지 않는다.
4. `TransferBodySkin`으로 자동 웨이트를 생성한다. `SurfaceVertex`는 선택된 실제 표면 정점의 전체 웨이트를 복사한다. `BarycentricStrict`는 보간 결과가 4개 영향을 넘으면 거부한다. 둘을 자동으로 전환하거나 상위 4개로 잘라내지 않는다.
5. `MakeGeneratedCorrespondence`로 생성 정점·봉제된 정점 그룹·본의 안정 ID와 topology 해시를 얻는다. `FLGCorrection.Target`에는 해당 simulation ID를, `TopologyHash`에는 이 해시를 저장한다. 정점 배열의 순번이나 가장 가까운 정점을 수동 보정 대상 ID로 사용하지 않는다.
6. 자동 생성/skin 직후 `ApplyDocumentCorrections`를 호출한다. 성공 결과를 `MakeClothCollection` 또는 `SaveClothAsset`에 전달한다. 같은 원본을 재생성할 때도 자동 결과부터 만들고 수동 원본을 다시 적용한다.

## 수동 보정

- 천 맵: `Domain="simulationVertex"`, `Property="maxDistance"`, `Value`는 **cm**이다. 예를 들어 `.42`는 4.2mm이다. 현재 지원 범위는 0~100cm이며, 원본 고정점은 0을 유지한다. 다른 맵 이름은 미지원 상태로 보존하고 거부한다.
- Skin: `Property`를 비우고 `Skin`에 본의 안정 ID와 정규화된 1~4개 양수 웨이트를 넣는다. native float에서 0이 되는 값은 적용하지 않는다.
- 같은 봉제 그룹의 모든 source 정점에 동일한 맵/skin을 적용하며, 실제 Cloth의 weld lookup을 통해 simulation weight map에 반영한다.
- 같은 대상·같은 속성의 중복 보정은 마지막 값으로 덮지 않는다. 존재하지 않는 대상/본, topology 불일치, 미지원 속성, 고정점 해제도 거부한다. 실패 시 생성 결과는 변경하지 않고 수동 원본을 유지하며 미해결 상태를 표시한다.
- `ApplyGeneratedTopology`는 문서 수준의 해시 대응 상태 갱신이다. 실제 보정 적용과 대상 검증은 `ApplyDocumentCorrections`가 담당한다. 해시 상태만으로 Cloth 저장을 승인하지 않는다.

현재 내부 topology 해시는 옷본 삼각분할·봉제·고정점·본 대응을 검증하는 개발용 SHA-256이다. 후속 파일 계약의 canonical archive 해시를 대신하지 않는다. 실제 티셔츠/쇼츠 피팅·포즈 검증은 구현 상태 기록을 따른다.

## 개발용 피팅 비교

`Tools/LaceGarment/Run-Fit.ps1`의 `SourceDraft`는 진단 JSON 재실행 경로이며 `.lgarment` 파일 구현이 아니다. `GravityCm`, `BendingStrength`, `Iterations`로 중력·opposite-vertex 굽힘 보정·반복 횟수의 영향을 분리할 수 있다. 기본값은 각각−980cm/s², 반복당0.05,100회로 유지하며 실제 사용값을 fit.json에 기록한다. 굽힘 보정은 보정된 원단 물성이 아니고, 값을 바꾸어도 봉제·변형·관통·면 검사 기준을 완화하지 않는다. 실패 결과를 Cloth 애셋으로 저장하지 않는다.
