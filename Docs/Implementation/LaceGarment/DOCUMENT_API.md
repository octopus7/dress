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

돌핀쇼츠 기본 템플릿은 `0.3.4-draft`, 기장28mm, 원통 초기 배치 반경2cm이다. 새 쇼츠 문서는 경계 간격을 최대1mm, 메시 변 길이를 최대2mm로 제한하고 더 조밀한 기존 설정은 유지한다. 같은 템플릿의 파라미터 갱신에서는 사용자가 선택한 메시 설정을 보존한다. 중앙 앞/뒤 허리와 양옆 허리에 목표 고정점이 있다. 진단 원본의 피팅 통과와 기본 템플릿의 실제 Cloth 저장·runtime 통과는 별도로 기록한다. 티셔츠는 아직 `0.3.3-draft`이며 검증되지 않은 후보 형상을 기본값에 적용하지 않았다.

생성기 `0.3.5-welded-junctions`는 닫힌 봉제 연결을 전이적인 정점 그룹으로 풀어 변 길이·중력·충돌 보정 후에도 같은 위치를 유지한다. 초기부터 일치하는 점은 즉시, 분리된 점은 봉제 닫힘 단계 이후 0.02cm 이내이고 모든 구성점의 충돌 sweep이 같은 위치에 도달할 때만 묶는다. 충돌체를 가로질러 봉제를 강제로 합치지 않는다. 같은 봉제 그룹에 서로 다른 고정 목표가 지정되면 생성 전에 오류를 반환한다. 원본의 2D 정점·UV 분리는 보존한다.

`Tools/LaceGarment/Run-Fit.ps1`의 `SourceDraft`는 진단 JSON 재실행 경로이며 `.lgarment` 파일 구현이 아니다. `GravityCm`, `BendingStrength`, `Iterations`로 중력·굽힘 보정·반복 횟수의 영향을 분리할 수 있다. 기본값은 각각−980cm/s², 초기 순차 반복당0.05,100회로 유지하며 실제 사용값을 fit.json에 기록한다. 생성기0.3.6에서 Chaos dihedral을 도입했고 현재는 같은 부호·각도 정의의 위치와 미분을 배정밀도로 계산한다. 작은 삼각형의 수치 미분·평행이동 불변성으로 검증하며 면 안의 반대 정점 간 거리를 강제하지 않는다. 굽힘 보정은 보정된 원단 물성이 아니고, 값을 바꾸어도 봉제·변형·관통·면 검사 기준을 완화하지 않는다. 실패 결과를 Cloth 애셋으로 저장하지 않는다.

봉제가 닫힌 이후 구조 변은 원본 길이의±5% 범위로 투영한다. 원본 rest와 최종10% 변형 거부 기준은 별도로 유지하며, 과도한 가이드 늘어남을 허용하거나 원본 길이를 결과에 맞춰 다시 쓰지 않는다.

생성기0.3.9-feasible-start는120단계 이후 모든 봉제가 실제로 묶였을 때 상대 변형률·접촉·dihedral 굽힘을 감쇠 Gauss–Newton으로 연립 보정한다. 기본 선형 풀이는 정점별3×3 블록 전처리 matrix-free PCG이다. `Run-Fit.ps1 -DirectSolve`를 명시하면 자유 정점 그룹4096개 이하에서 UE에 포함된 Eigen sparse LDLT를 비교할 수 있으며, 더 큰 입력이나 분해/잔차 확인 실패 시PCG로 돌아간다. 외부 프로그램이나 별도 설치는 필요하지 않다. 단계당 중력을 한 번 적용한 예측 위치는 목적함수의 고정 목표로 유지하되 비선형 탐색은 이전의 실현 가능한 위치에서 시작한다. 이 단계는 이전 속도를 누적하지 않는 과감쇠 안정화이며, 최종 속도는 실제 단계 간 변위에서 측정한다. `Iterations`는 초기 순차 보정 횟수, `LinearIterations`는 PCG 상한, `RefinementIterations`는 단계 내 비선형 보정 상한이다. 현재 기본값은100/200/32이며 진단 commandlet에서 독립적으로 비교할 수 있다. 최종 선형 잔차·비선형 보정량·직접/반복 풀이 횟수와 연립 보정·선형 풀이·충돌 sweep·에너지 평가의 소요 시간도 fit.json에 기록한다.

몸 안쪽으로 향하는 보정은 접촉면의 접선공간에서 다시 풀고, 매 비선형 반복마다 접촉을 다시 판단해 바깥쪽 이동을 허용한다. 이동 경로마다 기존 충돌 sweep을 수행하며 비선형 오차가 감소하는 이동만 채택한다. Newton 방향이 접촉 처리 후 감소 방향이 아니면 대각 전처리한 음의 기울기로 다시 시도한다. 원단 물성에 대한 정량 모델이나 모든 옷본의 수렴 보장을 뜻하지 않는다. 실제 대표 의상의 검증 상태는 STATUS를 따른다.

생성기0.3.13-surface-contact는 구조 변의 중점과 각 삼각형 중심에서도 capsule/convex 접촉을 계산하고 barycentric 비율로 원래 정점의 연립 풀이에 반영한다. 이 표본은 생성 topology나 원본 대응을 추가·변경하지 않는다. 표본 충돌만으로 성공 처리하지 않으며, 정착 후보와 최종 결과는 기존 연속 삼각형 관통 검사도 통과해야 한다. 표본 사이의 임의 접촉을 모두 해소하거나 수렴을 보장하는 방식은 아니다. 이 수정은 저작 피팅 경로에 적용되며 저장된 엔진 Chaos Cloth의 runtime 충돌 알고리즘을 바꾸지 않는다.

생성기0.3.14-surface-sweep는 초기 봉제 단계에서도 같은 중점/면 중심의 이동 경로를 검사한다. 보정은 barycentric 계수와 봉제된 그룹의 역질량으로 원래 정점에 분배하고 고정점은 유지한다. 매 반복의 이전 위치에는 봉제된 모든 원본 정점을 포함한다. 초기120단계의 면 관통 회귀를 재현 후 수정했으며 전체57개 자동화를 통과했다. 표본 사이까지 포함하는 완전한 연속 삼각형 충돌 검사는 아니므로 최종 면 관통 판정은 계속 별도로 수행한다.

비선형 반복은 중력 예측으로 밀어낸 위치가 아니라 이전 단계의 유효 위치에서 시작한다. 예측 위치는 근접 항의 목표에 계속 포함되므로 중력은 유지된다. 이렇게 해야 반복이 일찍 끝나더라도 line search 이전의 투영 이동이 변형 에너지를 증가시키는 것을 막을 수 있다. 진단 geometry.json의 `lastStepDisplacementCm`와 fit.json의 `timeStepSeconds`, `lastNonlinearIterations`로 마지막 단계의 이동·실제 보정 횟수를 확인할 수 있다.

`Run-Probe.ps1 -Template TShirt|DolphinShorts [-SourceDraft ...]`는 대표 의상의 생성·skin·수동 보정 후 Cloth 저장과 기준 포즈 runtime을 검사한다. `-VerifyExisting`은 새 프로세스에서 재생성한 원본과 저장된 rest/3D/UV/봉제/skin/천 맵을 비교한다. 대표 의상에서는 기준 바디의 전체 PhysicsAsset을 사용한다. 이 경로의 구현과 실제 의상 인수 통과는 구분하며, 아직 실패하는 피팅은 애셋으로 저장하지 않는다. 명시적 component-space 본 포즈를 받는 `ReadBodyCollision` overload는 포즈 검사의 충돌체를 제공한다. 본 개수와 변환을 검사하고 실패 시 기존 출력을 보존한다.

`-Poses`는 `-BodySkin` 또는 `-Template`과 함께 사용한다.720프레임 동안 기준 자세, 양팔25도와 루트5도 회전, 복귀, 왼쪽 허벅지−25도·종아리45도 굽힘, 복귀를 실행한다. 실제 component-space 포즈와 skin으로 계산한 고정점 목표를 비교하고, 고정점 자체도 움직였는지 검사한다. 정점 관통·고정점 오차는 매 프레임, 면 관통·변형률·접힘·면적비는 다섯 단계 끝에서 검사한다. `-Render`는 각 단계의 이미지를 저장한다. `-BodySkin`만 사용한 두 패널 프로브는 실제 바디 skin을 쓰지만 충돌은 단일 캡슐이다. 따라서 이 프로브의 성공은 전체 바디에 대한 대표 의상 착장 성공을 뜻하지 않는다.

`Run-Probe.ps1 -Template DolphinShorts -RuntimeOnly -Poses`는 저장 Cloth의2D rest와 현재 시뮬레이션을 비교하는 읽기 전용 진단이다. 긴 피팅을 생략하므로 `sourceIdentityChecked=false`, `diagnosticRuntimeOnly=true`이며 원본 재생성·저장/다시열기 인수의 대체물이 아니다. `-GenerateTethers`는 이 모드에서만 허용하며 메모리의 임시 복사본에 engine geodesic tether를 생성해 한 조건을 비교한다. 저장 애셋은 수정하지 않는다. 일반 생성 경로에서도 weld와 최종 MaxDistance 보정 이후 tether를 만들며, 정식 다시열기 비교는 tether 연결과 길이까지 포함한다.

생성기0.3.10-native-tethers는 위0.3.9 피팅 경로를 유지하면서 native tether 데이터를 추가한다. RuntimeOnly의 `-RuntimeIterations 1..400`, `-RuntimeGravityScale 0..2`, `-RuntimeBendingStrength 0..1`, `-HingeBending`도 임시 복사본 전용 비교 옵션이다. 설정값과 엔진에서 실제 사용한 반복/하위 단계 수를 함께 기록한다. 포즈별 최악 변의 직접 복원 경로에 대한 collision sweep 결과는 국소 진단이며, 전체 원단이 모든 제약을 동시에 만족한다는 증거가 아니다. 진단에서 중력을 제거하거나 반복 횟수·굽힘 모델을 바꾼 결과는 제품 기본 설정의 인수 결과로 취급하지 않는다.

`FLGPiece.InitialWrapRadiusCm`은0이면 평면 배치, 양수이면 조각 경계의 X 중심을 기준으로 로컬 Y축 주위에 감는 초기 배치다. mm 원본과 삼각분할·UV는 바꾸지 않고, 로컬 X의 호 길이를 보존하는 원통 표면으로 이동한 뒤 `InitialPlacementCm`을 적용한다. 반 원통 이상 감겨 접히는 반경은 거부한다. 몸 밖의 초기 정점·면 검사와 봉제 수렴 검사는 그대로 적용한다. 사용자용 UI는 이 API에 포함하지 않는다.

`-RuntimeFriction 0..1`도 RuntimeOnly의 임시 복사본 전용 진단이다. 실제 적용 마찰을 보고서에 남기고, 최악 변의 두 정점에 대해 스킨 위치까지의 거리·MaxDistance·충돌면 거리·tether 직접 거리/기준 길이 비율을 기록한다. tether 비율은 개별 제약의 최종 잔차를 살피는 자료이며 포즈 인수 기준을 대체하지 않는다.

`-RuntimeWaistband`는 `-RuntimeOnly -Template DolphinShorts`에서만 허용한다. 저장 원본을 수정하지 않고 두 옷본의 가장 위쪽 경계 정점에 MaxDistance0·pelvis 스킨을 적용하고 tether를 다시 만든다. 네 개의 피팅 가이드만 고정된 경우와 허리선 전체에 하중이 분산된 경우를 비교하기 위한 진단이며, 결과를 확인하기 전에는 기본 원본이나 제품 설정에 적용하지 않는다.

`-RuntimeSkinnedBody`는 `-RuntimeOnly -Poses -Template ...`의 비교 진단이다. 엔진 `FPhysicsAssetUtils::GenerateSkinnedTriangleMesh`로 기준 바디 LOD0의 위치·삼각형·전체 웨이트를 사용한 충돌체를 생성하고, 임시 PhysicsAsset과 Cloth 복사본에만 적용한다. UV 경계 중복 정점 병합과 본 인덱스 매핑은 엔진 생성 함수가 담당한다. 저장 바디/Physics/Cloth는 변경하지 않는다. 현재 관통 수치는 여전히 원래 rigid proxy를 대상으로 하므로 실제 스킨 표면의 관통 인수를 대신하지 않으며, 이 모드는 항상 `passed=false`로 남긴다.720프레임 실행 여부는 `nativeMeshRuntimeCompleted`, 모델 크기는 `diagnosticBodyVertices/Triangles`로 따로 기록한다. 엔진 verbose 로그에서 실제 skinned collision 추출도 확인한다. 이 비교가 유효해도 제품 채택에는 직접 표면 검사와 저장/다시열기 검증이 필요하다.

포즈 단계별 보고서에는 직전 프레임과의 최대 입자 속도(cm/s), 스킨 대비 이동 거리/MaxDistance의 최대 비율과99% 이상인 정점 수, 저장된 native3D rest 대비 변 길이 오차도 포함한다. 속도는 고정1/60초 tick의 유한 차분이며 solver 내부 속도 버퍼와는 구분한다. 원본2D 옷본 대비 변형률과 native3D rest 대비 오차를 함께 기록해 초기 피팅 변형과 runtime 잔차를 구분한다. 이 추가 진단 수치는 기존 인수 기준을 완화하지 않는다.

## 바디 UV 시각 검사

`Import-Body.ps1 -RepairSkin`은 독립 기준 바디의 명시적 스킨 보정 경로이다. 원본 영향을 UE 스킨 프로필 `LaceGarmentImportedSkin`에 보존하고, 골반 위 몸통의 다리 영향과 어깨 안쪽의 낮은 몸통에 전달된 팔 영향을 완만하게 줄여 기존 몸통 본에 재분배한다. 새 영향 본을 추가하지 않는다. 매번 보존된 원본에서 계산하므로 반복 실행 시 보정이 누적되지 않으며, 바뀐 웨이트가 없으면 저장하지 않는다. 이 경로는 `SK_OB11`만 저장하며 PhysicsAsset을 재생성하지 않는다. 읽기 전용 Inspection 옵션과 함께 실행할 수 없다. `skin-repair.json`의 저장 성공은 포즈 인수 통과를 뜻하지 않는다.

`Import-Body.ps1 -InspectionPose Reference|ArmsRaised|ArmsForward|Elbows90|Knees90|Sitting|HeadTurn`은 같은 읽기 전용 경로에서 저장 바디를 임시 `PoseableMeshComponent`로 검사한다. `ArmsRaised`는 양팔을 옆으로 수평까지 들어 올리며, `ArmsForward`는 앞쪽 수평이다. 팔꿈치/무릎90은 관절점 사이의 실제 각도를 기준으로 하고, 앉기는 허벅지 앞쪽 수평·종아리 수직, 고개 회전은45도이다. `-InspectionTexture`와 함께 사용할 수 있다. 실제 component-space 본 원점·회전과 전체 원래 스킨 영향을 적용한 정점 위치를 `inspection.json`에 기록한다.

`Tools/LaceGarment/Audit-BodyPoses.py <UE body.json> <inspection.json> [...]`는 본 방향/각도와 유한 정점·붕괴 면을 검사하고 면적비 분포를 기록한다. 최신 inspection의 정점 위치·UV0·삼각형 순서 해시를 기준 body.json과 독립적으로 비교하여 스킨 변경 후 형상 대응을 확인한다. commandlet은 모든 정점과 각 면의 중심·변 중점을 포즈가 적용된 충돌체와 비교하며, audit은 누락 표본이 있으면 수치 검사를 실패 처리한다. 최대 누락 거리·위치와 근접3정점의 실제 웨이트도 기록한다. 이 표본 검사는 연속 표면 전체의 포함 증명이나 관절의 시각 품질·자기 교차 검사를 대신하지 않는다. 이 검수 도구는 제품 런타임 의존성이 아니다.

`Tools/LaceGarment/Make-BodyUVInspection.py <UE body.json> <새 출력 폴더>`는 실제 UE UV0를 기준으로 체크무늬와 음수Y 빨강/양수Y 파랑 도색 입력을2K·1K·512px로 만든다. 도색 영역 바깥에2K 기준16px를 확장하고, 그 밖은 잘못된 샘플링을 찾기 위해 자홍색으로 둔다. 원본 body.json의 SHA-256과 생성 조건은 inputs.json에 기록한다. 개발용 Python 도구이며 제품의 저작 의존성이 아니다.

`Import-Body.ps1 -RunName <새 이름> -InspectionTexture <PNG>`는 저장된 바디를 읽고 임시 텍스처·재질 인스턴스로 정면/측면/후면을 렌더한다. 이 옵션은 import·충돌 생성·애셋 저장 경로를 실행하지 않는다. 결과 inspection.json의 rendered는 이미지 생성 여부이며, 시각 품질 통과를 뜻하지 않는다. 지정 해상도에서 bilinear 필터링을 확인하는 경로로, 자동 mip chain 검증과 구분한다.
