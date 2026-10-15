# LaceGarment 독립 의상 파일 계약

작성·개정일: 2026-10-16. 상태: **v1 설계안**. REQ-011의 UE 단독 저작을 반영합니다. 상위 설계: [플러그인 설계](LACE_GARMENT_PLUGIN_DESIGN.md). 아래 이름과 규칙은 구현할 계약이며 현재 파일 포맷이 구현되었다는 의미는 아닙니다.

**REQ-017에 따라 이 독립 파일의 저장·열기 구현은 이번 구현 대화의 완료 목표에서 제외합니다.** 후속 파일 계약으로 보존하며, 현재 진행하는 문서 모델·Cloth 생성 및 UE 애셋 저장 검증과 구분합니다.

## 1. 컨테이너

`.lgarment`는 UTF-8 JSON, glTF 2.0 GLB, PNG와 필요한 메타데이터를 담는 ZIP입니다. **UE 플러그인이 직접 저장·열기·재편집**합니다. 하나의 파일은 한 의상의 편집 원본과 기준 바디를 포함합니다. UE 배포 ZIP의 한 팩은 여러 의상 파일을 수용할 수 있습니다. GLB 입출력에 Blender를 호출하지 않습니다.

```text
manifest.json
authoring/document.json
authoring/corrections.json   # 수동 웨이트·천 속성 맵 보정 원본
pattern/pieces.json          # 평가된 경계, 파생 데이터
pattern/seams.json
mesh/garment.glb             # 검증 완료 상태의 생성 결과
mesh/simulation.json         # 2D rest / 초기 배치 / 착장 결과 구분
mesh/mapping.json            # 생성 결과의 대응
body/body.glb
body/profile.json
body/collision.json
cloth/properties.json
materials/materials.json
textures/*.png
preview/thumbnail.png
license.txt
readme.txt
```

라이선스·출처 정보는 작성자가 입력합니다. 출처 필드는 온라인 다운로드 의존성을 만들지 않습니다. 필요한 형상·텍스처는 파일 안에 모두 포함해야 합니다. `.uasset`, `.uproject`, 게임 DLL은 이 원본 컨테이너에 넣지 않습니다.

`state=draft`는 미완성 저작 문서이고 `state=validated`는 생성·검사를 통과한 문서입니다. 초안은 저작 문서·봉제 정의·기준 바디·사용 재질 등 편집에 필요한 원본을 포함하고, 아직 없는 생성 결과는 생략할 수 있습니다. 배포용 UE 애셋 생성과 준비된 의상 팩 내보내기는 검증 완료 상태만 허용합니다. 구조적으로 안전하지 않은 파일과 형식 오류는 초안에서도 허용하지 않습니다.

## 2. 식별과 manifest

| 필드 | 계약 |
| --- | --- |
| `format`, `schemaVersion` | `LaceGarment`, major/minor 형식 버전. 모르는 major는 거부 |
| `garmentId`, `garmentVersion` | 의상 UUID와 개정 버전. 표시 이름과 무관하게 유지 |
| `patternId`, `patternVersion` | 옷본 및 봉제 기준 데이터의 식별자와 버전 |
| `bodyProfileId`, `bodyProfileVersion`, `bodyProfileHash` | 골격·기준 포즈·치수·충돌 기준의 식별 |
| `authoringTool`, `generatorVersion` | LaceGarment 저작 도구 버전, UE 빌드, 메시·Cloth 생성기 버전 |
| `state`, `authoringHash`, `builtFromAuthoringHash` | 초안/검증 완료, 편집 원본 해시, 현재 생성 결과가 사용한 원본 해시 |
| `builtWith` | 결과 생성에 사용한 생성기·UE 엔진 빌드·solver 및 재질 프로필 버전 |
| `requiredFeatures` | 반드시 이해해야 하는 기능 ID. 모르는 필수 기능은 거부 |
| `coordinates`, `units` | 2D/3D 각각의 축·방향·단위 정의 |
| `files` | 정규화한 상대경로, 역할, 바이트 크기, SHA-256 해시. manifest 자체는 제외 |
| `payloadHash` | 아래 규칙으로 직렬화한 payload 파일 목록의 SHA-256 |
| `contentHash` | 단위·좌표·필수 기능 등 해석 정보도 포함한 manifest와 payload 목록의 SHA-256 |
| `author`, `license`, `provenance` | 작성자 표시·이용조건·원본 출처 |

해시 바이트 계약:

- 아카이브 경로는 소문자 ASCII 문자·숫자·`_`·`-`·`.`·`/`로 구성합니다. 파일명에는 사용자 표시 이름 대신 ID를 사용하고, 표시 이름은 JSON의 Unicode 문자열로 저장합니다. 구분자는 `/`이며 빈 경로 요소, `.`·`..` 요소, 선행 `/`, 역슬래시를 거부합니다.
- `files`는 `path`의 ASCII 바이트 순으로 정렬합니다. 파일 해시는 **압축 해제한 파일의 실제 바이트**에 대한 SHA-256이며 소문자 64자리 hex로 기록합니다.
- `payloadHash = SHA256(JCS(files))`로 정합니다. `contentHash = SHA256(JCS(manifest에서 payloadHash, contentHash, createdAt을 제외한 객체))`로 정합니다. `files`, 단위, 좌표, 버전, 필수 기능, 출처·이용조건 등 나머지 필드는 포함합니다.
- `JCS`는 RFC 8785의 canonical JSON UTF-8 바이트를 뜻합니다. 일반 JSON 출력이나 임의 줄바꿈·숫자 포맷을 해시 입력으로 사용하지 않습니다. 중복 키, 비정상 Unicode, NaN/무한대는 거부합니다. manifest의 들여쓰기와 ZIP 생성 시각은 해시에 영향을 주지 않습니다. payload JSON은 파일의 실제 바이트를 해시하므로 들여쓰기 변경도 파일 해시에 반영됩니다. [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785)

소스 전체 아카이브 해시는 UE 배포 manifest에서 별도로 기록합니다. 해시는 손상과 원본 대응을 검사하며 작성자의 신원을 보증하는 전자서명으로 취급하지 않습니다. `bodyProfileHash`는 `profile.json`의 프로필 객체와 `body.glb`, `collision.json`의 파일 해시를 담은 객체를 JCS로 직렬화하여 SHA-256으로 계산합니다. 프로필 객체 안에는 자신의 해시를 넣지 않습니다.

`files`에는 편집 원본(`source`)과 생성 결과(`derived`) 역할을 구분합니다. `authoringHash`는 `{schemaVersion, coordinates, units, requiredFeatures, sourceFiles}` 객체의 JCS 바이트를 SHA-256으로 계산합니다. `sourceFiles`는 `source` 역할 파일의 경로·역할·크기·해시를 경로순으로 정렬한 배열입니다. 생성 성공 시 그 값을 `builtFromAuthoringHash`에 기록합니다. 두 값이 다르면 결과는 낡은 것으로 처리하며 `validated`로 저장하지 않습니다. 옷본·봉제·바디·재질·천 속성·생성 설정은 모두 원본 해시 대상입니다.

원본이 같아도 `builtWith`의 생성기·엔진 빌드·프로필과 현재 대상 환경이 다르면 기존 검증 상태를 그대로 인정하지 않습니다. 명시한 호환성 시험을 통과하거나 재생성·재검사한 뒤 배포합니다. 템플릿 버전과 모든 생성 설정도 원본 문서에 포함하고, 버전 변경을 기록하지 않은 생성 알고리즘 변경은 허용하지 않습니다.

PackId는 UE에서 묶음을 만들 때 별도로 발급합니다. 옷의 GarmentId와 팩의 PackId를 혼용하지 않습니다. 같은 GarmentId에 다른 내용이 있으면 명시적인 새 버전이어야 합니다.

## 3. 단위·좌표와 변환

- 옷본 좌표는 mm, 조각별 2D 로컬 좌표와 방향을 기록합니다. UE의 옷본 표시 계산은 `cm = mm / 10`을 사용합니다.
- GLB 형상은 glTF 2.0의 미터·오른손 좌표계·Y-up 규칙을 따릅니다. 우리 바디의 정면은 GLB +Z, 인형 자신의 오른쪽은 -X로 지정합니다. 시뮬레이션 3D 좌표와 바디 충돌 형상도 같은 기준을 씁니다.
- 변환은 `UE_cm = 100 × (GLB_z, -GLB_x, GLB_y)`로 고정합니다. UE에서 +X 정면/+Y 오른쪽/+Z 위가 됩니다. 이 변환의 4×4 동차 행렬을 `C`로 부릅니다. 반사에 따른 삼각형 winding과 탄젠트 부호를 한 번만 보정하고 노멀은 역전치 행렬로 변환합니다. 엔진 임포터의 자동 축 변환을 다시 중첩하지 않습니다.
- 파일의 행렬은 column-major 16개 값, 열벡터 기준이며 `world = parentWorld × local`, 로컬 TRS는 `T × R × S`입니다. quaternion은 정규화한 `[x,y,z,w]`입니다. UE 메모리 표현과 별개인 파일 계약이며, 행렬 변환은 `C × M × inverse(C)`로 계산하고 회전은 그 결과에서 복원합니다.
- UE 작업 공간은 cm, 파일의 옷본은 mm, GLB는 m입니다. 저장할 때 위 변환의 역변환을 한 번 적용하고 열 때 정방향 변환을 한 번 적용합니다. 저작 문서의 배치·곡률·단위도 파일 기준으로 정규화합니다.
- 대칭, 경계 샘플링, 삼각분할과 topology 변경을 평가한 **후** 생성 메시의 안정 ID와 대응을 생성합니다. 옷본 조각·제어점·봉제 ID는 이보다 상위의 저작 ID로 계속 유지합니다. 이후 형상이 바뀌면 메시 대응을 다시 생성합니다.
- 알려진 치수의 축 표시 테스트 메시와 인형의 키·좌우·전방으로 변환을 검증합니다. 임포트 후 component scale은 `(1,1,1)`을 기준으로 합니다.

GLB의 기하·skin·PBR 표현은 공식 규격을 따릅니다. 옷본과 봉제는 별도 계약입니다. [Khronos glTF 2.0 규격](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)

## 4. 옷본과 봉제

`authoring/document.json`은 재편집의 기준입니다. 조각·제어점 ID, 직선/곡선 유형과 제어점, 치수·템플릿 파라미터, 대칭 관계, 시접·결 방향·notch, 조각별 초기 3D 배치와 휘어짐 가이드, UV 패킹 설정, 메시 생성 설정, 고정 가이드를 저장합니다. 기록된 템플릿을 외부에서 찾아야만 열 수 있어서는 안 됩니다. 편집 가능한 모든 조각 정의를 파일에 포함하고 템플릿 이름/버전은 출처로 기록합니다.

`pattern/pieces.json`은 이 원본을 평가한 선분열과 경계이며 `derived`로 분류합니다. `pattern/seams.json`은 저작 EdgeId와 곡선 구간 파라미터를 참조하는 봉제 원본입니다. 이 둘의 내용이 충돌하면 제어점 원본을 기준으로 재생성·검사합니다. Undo/Redo는 저작 도구의 명령 기록이고, 게임의 작업 진행 세이브와 혼용하지 않습니다.

`authoring/corrections.json`은 UE에서 수동 수정한 skin 웨이트와 천 속성 맵 보정을 `source`로 저장합니다. 각 보정에는 적용 도메인, 안정 ID, 기준 topologyHash, JointId별 웨이트 또는 속성값을 기록합니다. 같은 topology로 재생성할 때 자동 결과 위에 재적용합니다. topology가 바뀌면 보정을 조용히 삭제하거나 가까운 정점에 붙이지 않고 미해결 보정으로 표시하여 사용자가 다시 지정하거나 명시적으로 폐기하도록 합니다. 해결 전에는 초안 저장만 허용합니다.

| 데이터 | 필수 내용 |
| --- | --- |
| 조각 | PieceId, 이름, 좌우/수량, 실제 크기의 2D 외곽·내부 루프, 결 방향 |
| 경계 | EdgeId, 2D 선분열, 진행 방향, 재단/봉제/접힘/열린 경계 구분 |
| 시접 | 재단 경계와 실제 봉제 경계의 구분, 구간별 시접 폭 또는 명시적 시접 형상 |
| 정렬 표식 | notch의 EdgeId와 구간 위치, 좌우 대응 |
| 봉제 | SeamId, 조각·경계·구간 쌍, 방향, 길이 차이 허용값, EaseRatio, 유형 |
| 작업 순서 | 재단 및 봉제 선행 관계. 순환 의존은 오류 |

길이 차이는 무조건 오류로 처리하지 않습니다. 명시된 여유·늘림 범위 안의 차이는 허용하고, 범위 밖이면 조각과 경계를 지정해 표시합니다. 자기 교차·닫히지 않은 필수 루프 등 편집 도중의 형상 문제는 초안에 경고로 보존할 수 있지만 메시 생성·검증 완료 저장은 차단합니다. 존재하지 않는 ID 참조 등 형식 오류는 저장 전에 해결해야 합니다.

일반 연결, 접어박기, 바인딩은 서로 다른 유형입니다. 접어박기는 한 경계와 접힘 지시로 표현할 수 있으며 항상 두 경계가 있다고 가정하지 않습니다. 바인딩은 별도 띠 조각 또는 규격이 있는 처리 방식으로 표현합니다. v1 변환기가 지원하는 유형만 `requiredFeatures`에 선언하여 사용합니다.

게임에서 그리는 지정 재단선과 봉제 안내는 이 데이터로 만들 수 있습니다. 사용자의 절단/바느질 진행과 Undo 기록은 포함하지 않습니다.

## 5. 렌더·시뮬레이션 메시와 대응

`garment.glb`는 렌더 삼각형, 노멀, UV, 재질 슬롯, 골격과 스킨을 담습니다. `simulation.json`은 시뮬레이션 정점·삼각형, 조각 ID, 봉제 대응과 함께 **2D rest 옷본**, **시뮬레이션 전 초기 3D 배치**, **안정화 후 착장 3D 위치**를 별도 필드로 담습니다. 초기 배치는 저작 문서에서 재생성하며, 안정화된 결과로 초기 배치를 덮어쓰지 않습니다. 단위는 manifest의 2D mm/3D m 규칙을 따릅니다.

v1은 각 GLB에 활성 scene 하나, 정규화한 skinned mesh node 하나, mesh 하나, skin 하나를 허용합니다. 재질별 primitive는 여러 개일 수 있습니다. 오브젝트의 배치·scale은 형상과 rig에 반영하고 mesh node와 최상위 장면 노드는 identity로 저장합니다. 임의 scene instancing과 morph target은 v1 필수 기능에 포함하지 않습니다.

`mapping.json`은 다음 대응을 명시합니다.

- 옷본 PieceId/EdgeId/구간 ↔ 시뮬레이션 경계·정점 ID.
- 렌더 primitive 및 정점/삼각형 코너 ID ↔ 시뮬레이션 삼각형 ID와 무게중심 좌표.
- `(파일 경로, nodeIndex, meshIndex, primitiveIndex, POSITION accessorIndex, vertexElementIndex)` ↔ 안정 정점 ID. 삼각형은 해당 primitive의 indices accessor와 삼각형 순번으로 식별합니다. 이 표는 **실제 내보내기 결과**를 기준으로 생성합니다.
- UV 경계·하드 노멀·재질 경계에서 여러 렌더 정점이 하나의 원본 표면 지점을 나타낼 수 있는 다대일 관계.

저작 제어점 ID, 생성 메시 정점 번호, GLB 정점 번호, UE 애셋 내부 정점 번호가 같다고 가정하지 않습니다. UE 생성기·직렬화·임포트 과정이 재배열·병합·분할하는 경우 원본 대응을 검증 가능한 경로로 전달하거나 실제 출력에 맞게 재구축합니다. 이를 지원하지 않는 경로는 선택하지 않습니다. 좌표가 가까운 정점을 임의로 매칭하여 봉제 연결을 추측하지 않습니다.

대응 파일은 topology 해시와 연결합니다. 재메시·삼각분할 변경 등으로 해시가 달라졌으면 결과를 무효화하고 초안으로 저장합니다. 재생성 전에는 해당 결과를 검증 완료 상태로 내보내거나 배포할 수 없습니다. UV·skin·천 속성 맵도 적용 topology를 확인하여 재생성 또는 사용자 보정을 요구합니다. 의도하지 않은 구멍·찌그러진 면·유효하지 않은 인덱스·NaN·무한대·잘못된 skin 참조도 검사합니다.

## 6. 바디·골격·충돌

독립적인 전체 미리보기를 위해 배포 가능한 기준 바디를 필수로 포함합니다. 충돌 프록시만으로 흰 인형의 미리보기를 대체하지 않습니다.

- JointId, 이름, 부모, rest transform, inverse bind matrix와 기준 착장 변환.
- `(파일 경로, skinIndex, jointIndex, nodeIndex)`와 JointId의 일대일 표를 저장합니다. `jointIndex`는 skin의 joints 배열 위치이며 node 번호와 다릅니다. 두 GLB는 JointId로 연결하고 이름·배열 순서만으로 연결하지 않습니다.
- JointId별 rest world transform은 두 GLB에서 같아야 합니다. identity mesh node의 bind pose에서 inverse bind matrix는 joint rest world matrix의 역행렬이어야 합니다. 해당 GLB의 실제 accessor 값과 프로필의 값을 교차 검사합니다.
- 키·목·가슴·허리·엉덩이·팔다리 등 바디 치수와 측정 단위.
- 정규화된 skin 웨이트. v1 기본은 정점당 최대 4개 영향이며, 초과를 조용히 잘라내지 않고 제작자가 재분배하도록 표시합니다.
- bone별 capsule/sphere 등 지원하는 충돌 프록시와 단위. 엔진별 불투명한 충돌 설정 전체를 독립 파일의 표준으로 간주하지 않습니다.
- 배포 미리보기용 기본 포즈. 바디와 의상의 기준 포즈·골격·크기가 맞는지 검사합니다.

같은 BodyProfileId만으로 착장을 허용하지 않습니다. 버전·해시·골격·기준 포즈·치수를 함께 확인합니다. 호환되지 않는 바디는 이유를 표시하고 재피팅 대상으로 분류합니다.

OB11 흰 더미는 고유 UV0와 균일한 텍셀 밀도를 유지합니다. 자세한 기준은 [기존 바디 설계](LACELOFT_DESIGN.md)를 따릅니다. 의상은 페인트 가능한 고유 UV 모드를 기본으로 하고, 반복 원단용 UV 겹침을 사용하면 그 모드를 명시합니다. 바디와 의상의 UV 겹침 규칙을 혼용하지 않습니다.

## 7. 재질·천 속성

재질은 고정된 PBR 부분집합을 지원합니다: BaseColor, Metallic, Roughness, Tangent-space Normal, Opacity/AlphaCutoff, 양면 여부. 텍스처 파일·슬롯·UV 채널·색 공간을 명시합니다. BaseColor는 sRGB, 나머지 데이터 맵은 선형값을 기준으로 합니다. 노멀 맵의 Y 방향은 GLB 기준으로 고정하고 UE 변환기가 한 번만 변환합니다.

UE 저작 도구의 재질은 포함된 표준 Master Material과 이 PBR 속성으로 구성합니다. 원단 색·질감·밀도는 도구에서 설정하고, 이미지 파일을 추가할 수 있지만 외부 프로그램은 필수가 아닙니다. 임의 Material Graph의 완전 교환은 지원하지 않습니다. 지원 속성으로 표현할 수 없는 그래프를 허용 경로인 것처럼 저장하지 않습니다. v1은 불투명/마스크를 우선 지원하고 반투명 재질은 지원 목록에 없으면 거부합니다.

천의 물성은 두께(m), 면밀도(kg/m²), 마찰 등 단위와 의미를 갖는 값으로 저장합니다. 고정 영역·가동 영역·최대 이동량·anim-drive 등 속성 맵은 적용 도메인(시뮬레이션 정점/삼각형 등)과 범위를 기록합니다.

각 solver의 stiffness 숫자를 같은 물성으로 간주하지 않습니다. 중립 물성과 UE용 조정 프로필을 구분하고, UE 프로필에는 엔진·변환기 버전과 값의 의미를 기록합니다. Cloth와 Dataflow 애셋은 UE Editor에서 생성하는 결과물입니다. 원본 파일에 UE 내부 바이너리 구조를 저장하지 않습니다.

## 8. 읽기·재저장·오류 계약

- 읽기는 임시 영역에서 검사한 뒤 확정합니다. 실패한 파일이 기존 의상이나 팩을 덮어쓰지 않습니다.
- 허용된 파일 유형만 읽고, ZIP의 절대경로·`..` 탈출·심볼릭 링크·대소문자 정규화 후 중복 이름을 거부합니다.
- 파일 수·압축 해제 크기·압축률·개별 메시/텍스처 크기 제한을 두고 초과 시 중단합니다. 구체적인 기본 한도는 샘플 크기 측정 후 검사 설정으로 고정합니다.
- 모든 내부 참조는 정규화된 상대경로입니다. 외부 URL·로컬 절대경로를 필수 리소스로 사용하지 않습니다.
- GLB 외부 buffer/image URI를 허용하지 않습니다. manifest에 포함되지 않은 payload, 누락된 파일, 해시 불일치도 거부합니다.
- 원본의 Python·실행 파일·자동 실행 명령을 가져오기 과정에서 실행하지 않습니다. `.blend`나 DCC 실행 파일은 형식의 필수·선택 payload에 포함하지 않습니다.
- minor 버전의 모르는 선택 메타데이터는 보존하고, 이해하지 못한 필수 기능은 거부합니다. 스키마 변환은 원본을 보존한 새 파일로 출력합니다.
- 수신 파일 무결성은 위의 정확한 해시 계약으로 검사합니다. 편집·재내보내기 왕복 검증은 ID와 치수·UV·웨이트·봉제 의미를 비교합니다. exporter가 GLB 표현을 바꾸면 파일 해시는 달라질 수 있으므로 왕복 의미 보존을 해시 동일성과 혼동하지 않습니다.

## 9. UE 결과물과 출처 연결

UE 의상 정의에는 GarmentId/Version, PatternId/Version, BodyProfileId/Version/Hash, 원본 contentHash, 변환기 버전, 엔진 빌드, 생성 애셋 목록과 의상 기능을 기록합니다. UE 참조는 생성된 팩과 공통 플러그인 안에서만 만듭니다.

재가져오기는 임시 결과를 생성하고 검사한 후 기존 결과와 교체합니다. 공개된 애셋 경로는 유지합니다. 옷본·골격·topology가 바뀌면 Cloth와 대응을 재생성하고, 게임 연결부가 이전 작업 진행·착장 자격을 무조건 유지하지 않도록 버전 변경을 노출합니다.

이 계약은 **UE 저작 문서 → 독립 파일 → 다른 UE 프로젝트에서 재편집·재생성**을 대상으로 합니다. 플러그인 밖에서 임의로 수정한 최종 Skeletal Mesh·Cloth·Material Graph로부터 원본 옷본과 봉제 의도를 자동 역추론하는 기능은 v1 범위에 포함하지 않습니다. 저작 도구는 원본과 결과물의 불일치를 검사하고 알려야 합니다.
