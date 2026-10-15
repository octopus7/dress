# LaceGarment 구현 상태

갱신: 2026-10-16. 담당: 별도 Astra / High 구현 대화 `01a14073-fb57-76d2-bf43-db08097d8770`.

작성자/이용조건: REQ-013에 따라 미정. 저장소는 추후 비공개 전환 예정이며 라이선스 파일·문구·배포 이용조건을 임의로 추가하거나 확정하지 않는다. 공개 설정 변경은 실행하지 않았다.

**현재: 패널/봉제/안정화/Cloth Collection 하위 경로 구현·자동화 확인. REQ-017에 따라 이번 완료 목표는 Task 1~3으로 축소되었으며 실제 대표 의상 피팅·포즈 검증은 진행 중입니다.**

**이번 실행 제외:** Task 4 독립 원본 파일, Task 5 실제 저작 WBP/EUW, Task 6 콘텐츠 팩·배포·cook, Task 7 새 프로젝트 최종 인수. 아래 전체 제품 관문 A–F와 과거 검증 기록은 보존하지만 전체 통과를 이번 완료 조건으로 삼지 않습니다. 호스트 빌드·자동화·UE 애셋 저장/재열기·대표 의상 품질 검증은 유지합니다. 배포 가능한 제품이 완성되었다는 뜻은 아닙니다.

- 승인: REQ-012. 실행 계획: `Docs/Plans/2026-10-16-lacegarment-plugin.md`.
- 독립 경계: `PluginDev/LaceGarmentHost/`, `Tools/LaceGarment/`, `Dist/LaceGarment/`, 이 기록 폴더.
- 기존 루트 Source/Content/Config/.uproject와 게임 STATUS 변경 금지.
- 실제 엔진 재확인: `E:/WorkTemp/Epic Games/UE_5.8`, 5.8.3 / CL 58210709 / BuildId 55116800.
- 초기 `git status --short`: 기존 프로젝트 대부분 미추적. 보존하며 브랜치 전환/전체 스테이징/정리하지 않음.
- CIM 프로세스 명령줄 조회는 샌드박스에서 접근 불가. `Get-Process`의 UE/dotnet/MSBuild 조회는 현재 결과 없음. 빌드 시 UE의 mutex와 순차 실행을 사용.

| 관문 | 상태 | 근거 |
| --- | --- | --- |
| A UE 단독 저작 | 일부 하위 경로 확인, 미통과 | 목표 고정점·문서→생성·충돌체 중첩/이동 경로·목길이 하한 포함37개 자동화 통과. 패널 Cloth 저장/재열기/240 tick/렌더 확인. 실제 저작 UI 및 대표 의상의 착장·포즈 검증 미완료 |
| B 원본 왕복 | 미실행 | |
| C 천·포즈 | 미실행 | |
| D 배포 독립성 | 일부 하위 경로 확인, 미통과 | Checkpoint-4 개발용 BuildPlugin 선컴파일 산출물과 새 Blueprint/C++ 프로젝트에서 각각37개 자동화·저장 Cloth 재열기 확인. 최종 배포 팩·독립 원본·참조 격리 검사는 미구현 |
| E 다중 팩·갱신 | 미실행 | |
| F 새 프로젝트 인수 | 일부 하위 경로 확인, 미통과 | 새 빈 Blueprint/C++ 프로젝트에서 현재 하위 경로 확인. 컴파일러 없는 환경, cook/Development/Shipping 미실행 |

## 검증 기록

로그 루트: `PluginDev/LaceGarmentHost/Saved/Verification/`. 각 실행은 새 디렉터리와 새 index.json을 요구하며 프로세스 exit=0만으로 테스트 성공 처리하지 않는다.

| 실행 | 실제 결과 |
| --- | --- |
| Geometry-Red | 샌드박스 UBT 엔진 작업 디렉터리 접근 거부. 호스트 권한 실행으로 해결 |
| Geometry-Red-Host | UE 5.8에 없는 GeometryCollectionCore 모듈 의존성 오류. 로컬 모듈 소스 확인 후 제거 |
| Geometry-Red-2 | Editor 빌드 성공. 미구현 패널 생성 양성 테스트 2개가 예상대로 실패 |
| Geometry-Green | Editor 빌드 성공, Geometry 3/3 통과 |
| Pipeline-Red | Editor 빌드 성공. 미구현 봉제/Collection 양성 테스트 2개가 예상대로 실패 |
| Pipeline-Green | Editor 빌드 성공, 전체 6/6 통과. float 리터럴 경고는 후속 수정 |
| Capsule-Initial | 6/7 통과, 몸통 시험 실패. 초기 패널 축 방향 오류 수정 |
| Capsule-Upright | 6/7 통과, 고정점 근처 변 길이 오차 338.7%로 수렴 거부. 봉제 간격·정점 관통만으로 통과하지 않음 |
| Capsule-SmallScaleStep | Editor 빌드 성공, 전체 7/7 통과. 1/480초·100회 반복·246단계, 최대 속도 0.01607cm/s, 봉제 간격 0cm, 정점 관통 0cm, 변 길이 오차 9.314% |
| Probe-Build / Probe-Build-2 | UE 5.8 헤더·접근 지정자 오류. 로컬 소스의 공개 simulation proxy API에 맞춰 수정 |
| Probe-Build-3 / Probe-WorldFix | 각 Editor 빌드 및 7/7 통과 |
| Native-Probe-1 | 3개 애셋 저장 후 검증 월드 중복 초기화 assertion. CreateWorld의 초기화 인수로 수정 |
| Native-Probe-Reopen-1 | 새 프로세스에서 저장된 Cloth/재질/Physics 로드 성공. 240 tick 후 816 particles, 최종 최대 변위 0.07024543cm, 최종 정점 관통 0cm. 전체 프레임·저장 데이터 동일성 검사는 이 실행에 없음 |
| Native-Probe-2 | 기존 material package 재저장 시 partially loaded 오류. FullyLoad 및 기존 UObject 재사용으로 수정 |
| Probe-RegenerationFix / Probe-RenderBuild | 각 Editor 빌드 및 7/7 통과 |
| Native-Probe-3 | 렌더 과정에서 skeletal scene updater assertion. commandlet IsClient=false가 NULL Scene을 생성하는 원인 확인. 소스 authoring.json 생성, PNG/metrics 미생성 |
| Review-Regression-Red | Editor 빌드 성공. 초대형 좌표·삼각형 예산 회귀 2개 실패. 빈 패널 결과의 Collection 변환에서 배열 assertion 재현; 정상 종료/통과 아님 |
| Review-Regression-Green | Editor 빌드 성공, 10/10 통과(오류/경고 0). 배치·출력 좌표 ±10000cm, 세분화 전 32768 face 예산, Collection의 offsets/triangle/stitch/pin/UV 검증 추가 |
| Native-Probe-4 | 렌더 3장 생성 및 수치 검사 true였지만 속성 CopyTo의 비어 있지 않은 group ensure로 exit=1. 전체 실패로 기록. 이미지에서 봉제 주변 과도한 면 접힘 발견 |
| Probe-Integrity-Build / Probe-CanonicalConfig | 각 Editor 빌드 및 10/10 통과. 재열기 동일성/전 프레임 검사 추가, canonical runtime 속성을 생성 단계 한 곳에서 설정하도록 수정 |
| Native-Probe-Reopen-Integrity | 새 프로세스에서 rest/3D/UV/face/stitch/skin/고정점이 원본 재생성과 동일. 240/240 frame particle 확인, 최고 변위 0.08652435cm, 최고 정점 관통 0cm, 고정점 변위 0cm |
| Probe-Fold-Red | 9/10 통과. capsule 내부 인접면 접힘 179.592도로 새 품질 회귀 실패 |
| Probe-Bend-Regularization | Editor 빌드 및 10/10 통과. 패널 내부 opposite-vertex 굽힘 spring 추가. 최대 내부 접힘 52.520도, 198단계, 변 길이 오차 9.943% |
| Native-Probe-5 | 재생성·저장·runtime·렌더 exit=0, passed=true. 원본 데이터 동일, 816 particles, 최종 변위 0.1488592cm, 정점 관통 0cm. 3장 이미지 직접 확인: 기존 미세한 반전 접힘 개선, 양옆 접힘은 남아 있음. 완성 의상 판정 아님 |
| Probe-FaceCollision-Red | 10/11 통과. 모든 정점이 몸 밖에 있어도 면이 몸을 가로지르는 배치를 기존 검사가 허용함을 재현 |
| Probe-SurfaceValidation | Editor 빌드 및 11/11 통과. 초기/결과 삼각형 면과 capsule의 정확한 거리 검사, 내부 인접면 접힘 120도 및 면적비 0.5~1.5 제한 추가 |
| Native-Probe-Surface / Native-Probe-Surface-Reopen | 생성·렌더·새 프로세스 재열기 모두 exit=0/passed=true. 정지 결과 면 관통0cm, 내부 접힘52.520도, 면적비0.965~1.219, runtime 전 프레임 최고 변위0.1904002cm, 정점 관통0cm, 고정점 변위0cm |
| Body-Red | 11/12 통과. 독립 바디 애셋 누락을 예상대로 검출 |
| Body-Import-Build / Body-Import-1 | Editor 빌드 후 고정된 독립 FBX/PNG에서 플러그인 바디·골격·재질·Physics 생성. 15cm,20bones,19collision bodies. 자동 Physics fit의 착장 적합성은 별도 검증 대상 |
| Body-Green / Body-Render-Build | 각 12/12 통과. 유효한 골격 참조, 정점당1~4개·합65535의 skin, UV margin, 절대 import 경로 제거 확인 |
| Body-Import-1/uv-audit.json | UE LOD0 추출 좌표로 독립 검사. 38174verts/73628tris/16charts,197795 후보쌍의 정확한 UV clipping에서 면적 겹침0·퇴화0. 외곽50.097px/2K, 최대/최소 섬 밀도비1.00000488. 섬 사이 거리·도색 경계 시각 검수는 아직 별도 |
| Body-Render-1 | 자체 바디를 UE renderer에서 3방향 캡처. 정면/사선 이미지 직접 확인. 실제 의상 착장·관절 포즈 검수는 아님 |
| Body-Skin-Red | 미구현 전송 테스트 실패(12/13) |
| Body-Skin-Green / Body-Skin-Chest | 구현 후12/13. 엄격한 삼각형 보간이 일부 지점에서5영향을 만들어 계약에 따라 거부. 자동으로 잘라내지 않음 |
| Body-Skin-Modes | Editor 빌드 및13/13 통과. 명시적 SurfaceVertex 모드는 가까운 표면 삼각형 정점의 전체 원래 웨이트를 복사하고 BarycentricStrict는4초과 시 실패. 봉제 그룹 동일 웨이트·render/sim 보존 및 실패 시 기존 데이터 보존 확인 |
| Body-Skin-ProbeBuild | Editor 빌드 및13/13 통과 |
| BodySkin-Probe-1 / BodySkin-Probe-Reopen-1 | 별도 CA_BodySkinProbe 생성·렌더·새 프로세스 재열기 모두 exit0/passed. reference skeleton20bones, 원본의 sim/render skin 동일,816 particles, 최종 변위0.1488565cm, 정점 관통0. 좌표 검사는 simulation reference bone transform을 component space로 적용함 |
| Document-Red / Document-Seams-Red | 미구현 source model 테스트가 예상대로 실패(각13/16,13/17). 곡선·보정·Undo/Redo·봉제 구간/순환 포함 |
| Document-Green | Editor 빌드 및17/17 통과. 안정 ID 참조/수치 범위/봉제 순환 검사, cubic 평가, 실제 UE transaction Undo/Redo, topology가 달라진 보정의 미해결 보존. UI/archive 연결 및 생성 bridge는 아직 미구현 |
| AdaptiveMesh-Red / AdaptiveMesh-Green | 각17/19,18/19. 전체 균등 세분화의 과다 곡선 face와 봉제 경계 샘플 불일치를 재현. 필요한 변만 분할하여 경계 대응은 해결했지만 곡선 face 예산 회귀는 남음 |
| BoundarySamples-Regression | 18/20. 일직선상 다수의 경계 점에서 기존 ear clipping이 퇴화 삼각형을 만드는 문제 추가 재현 |
| ConstrainedMesh-Green | Editor 빌드 및20/20 통과. 경계를 제약으로 보존하는 Delaunay·내부 격자·선택적 변 분할 적용. 조밀한 곡선·공선 경계·경계 독립 샘플링 확인 |
| Constrained-Probe-1 / Constrained-Probe-Reopen-1 | 생성기0.2.0-adaptive-probe로 새 저장·렌더·새 프로세스 재열기 통과. 176단계, 내부 접힘33.996도, 면적비0.980~1.034, 변 길이 오차2.989%, 면 관통0.000881cm(0.005cm 허용 기준 이내). 312particles,240frame,최고 runtime변위0.059982cm,정점 관통0,고정점 변위0. 렌더 사선 이미지 직접 확인 |
| Constrained-BodySkin-1 / Constrained-BodySkin-Reopen-1 | 새 생성기의 바디 skin Cloth 저장·렌더·재열기 exit0/passed.312particles,20bone reference,원본 sim/render skin·형상 동일성 통과. 최종 runtime변위0.041187cm,정점 관통0 |
| DocumentSampling-Red | Editor 빌드 성공,20/23. 곡선 경계·초안 생성 차단·부분 봉제/ease의 새 테스트가 미구현 stub에서 예상대로 실패 |
| DocumentSampling-Green / DocumentSampling-Integration | Editor 빌드 및각23/23,25/25 통과. cubic arclength 경계·notch 파라미터·부분 봉제·ease·원본 고정점 대응. 길이가5% 다른 경계의 실제 드레이핑 수렴, 반대 방향 notch의 명시적 stitch, 교차 notch/중복 봉제 거부 확인 |
| InnerLoops-Red | 26/27. 안쪽 루프의 실제 면적 제외가 미구현 상태에서 실패 |
| SourceCurveReview-Red / SourceCurveReview-Green | 각26/29,27/29. 검토가 지적한 성긴 샘플의 원곡선 자기교차 누락 및 curve arc/chord 봉제 허용치 차이를 재현. 첫 수정은 유효한 접선 연결까지 근접 교차로 거부하여 미통과 |
| CurveReview-And-InnerLoops | Editor 빌드 및29/29 통과. 출력 간격과 별도인 정밀 source polyline 검사·동시 봉제 재샘플링·할당 전 누적 샘플 예산 추가. 안쪽 루프 OddWinding 삼각분할, 면적 제외, 방향 독립성, 외부/접촉/중첩 구멍 거부 확인. source curve 검사는 수치 근사이며 해석적 곡선 교차 인증은 아님 |
| DocumentCutout-Red / DocumentCutout-Green | 각29/30 예상 실패 후 Editor 빌드 및30/30 통과. 문서 inner loop가 실제 rest 메시에서25mm²를 제외하고 native drape에서도 유지됨. 각 loop의 source curve 교차 및 포함 관계를 검사 |
| FreshBlueprint-Checkpoint-1 | BuildPlugin 실패. 배포 플러그인의 UHT 규칙에서 편집 노출 property의 Category 누락 검출. 모든 문서 property에 명시적 Category 추가 |
| FreshBlueprint-Checkpoint-2 | Win64 BuildPlugin 선컴파일 성공. 산출물을 새 빈 Blueprint 프로젝트에 복사해30/30 자동화와 BodySkin Cloth 새 프로세스 재열기·240frame 검사 통과. 개발 체크포인트이며 완성품 배포 아님. 이 PC에는 컴파일러가 설치되어 있으므로 컴파일러 없는 환경 검증을 주장하지 않음 |
| BodyCollision-Red / BodyCollision-Green / BodyCollision-Green-2 | 미구현 추출 회귀30/31 실패 → TObjectPtr 배열의 auto* 타입 추론 빌드 오류 → 수정 후 Editor 빌드 및31/31. 기준 포즈의19개 capsule을 component cm 공간으로 변환, 머리/발 높이·양팔 위치·실패 시 기존 값 보존 확인 |
| Templates-Red / Templates-Green | 미구현 회귀31/33 실패 후 Editor 빌드 및33/33. 티셔츠/돌핀쇼츠의 원본 조각·곡선·봉제·앵커를 포함한 draft 템플릿. 소매/기장 편집 시 deterministic source ID와 수동 보정 보존, 삼각분할 확인. 실제 착장 성공 판정은 아님 |
| FitProbe-Build / FitProbe-Build-2 | JSON TSharedRef 타입 사용 빌드 오류 수정 후 Editor 빌드 및33/33. 실제 바디와 초기/결과 의상 표면을 함께 렌더하는 진단 commandlet 추가 |
| TShirt-Fit-1 | 실패. 실제19개 body proxy 사용 시 초기 정점 관통0.171976cm로0단계에서 차단. 렌더 확인, 자동 head capsule이 실제 머리보다 아래/위로 크게 확장됨을 검출 |
| BodyHeadProxy-Red / HeadProxy-Build / HeadProxy-Refit-1 / HeadProxy-Green | 범위 회귀32/33 실패 → 코드 빌드만 실행 → 실제 head-dominant 정점의 결정적 높이 minimax sphere로 Physics 갱신 →33/33 통과. 중심z12.430774cm/r2.629073cm,아래9.801701/위15.059847cm. 실제 머리 정점 포함 검사를 추가함 |
| TShirt-Fit-2 | 실패. 초기 관통0이나4800단계 후 변 길이 오차543.3%·관통0.14719cm. 봉제 간격0만으로 성공 처리하지 않음. 렌더에서 목·어깨의 큰 변형 확인. 초기 평면상 목 고정점 간격3.6cm가 옷본 목둘레와 양립하지 않는 원인 추적 |
| AnchorTargets-Red | 새 목표 고정점이 생성기에 반영되지 않는 회귀33/34 실패. 목표 위치와 초기 배치를 분리하고 목표의 body 관통을 거부하도록 구현 중 |
| AnchorTargets-Green | Editor 빌드 및34/34 통과. 목표 위치로60단계 이동, 목표의 body 관통·중복 충돌 거부. 기존 초기 배치와 source ID 보존 |
| TShirt-Fit-3 | 실패.4800단계 후 속도0/봉제 간격0이지만 변 길이 오차554.5%·관통0.17676cm. 당시 면 측정0은 미수렴 조기 반환에 따른 미측정값이며 정상 판정 근거가 아님 |
| SleeveWidth-Build / TShirt-Fit-4 | Editor 빌드 및34/34 후 소매 cuff 폭24mm(앞뒤 둘레48mm)로 진단.1200단계,1374정점,변 길이 오차704.5%·관통0.23195cm로 실패. geometry.json 추가로 위팔 내부/아래팔 표면에 멈춘 정점1281 확인. 렌더에서 큰 변형·몸 관통 확인 |
| CollisionUnion-Red | Editor 빌드 성공,34/35. 단순 구 두 개·평행 capsule·실제 Fit-4 팔 좌표에서 순차 충돌 투영이 관통을 남기는 회귀 실패. 충돌체 합집합 출구 투영 수정 검증 중 |
| CollisionUnion-Green | Editor 빌드 및35/35 통과. 순차 투영 뒤 남은 관통에 한해 유한 방향들의 정확한 line/capsule 구간 합집합 출구 후보를 검사. 전역 최근접점 보장은 아니며 모든 collider의 최종 외부 여부를 재검사. 실패 시에도 면 관통/접힘/면적을 측정하고 surfaceMeasured 기록 |
| TShirt-Fit-5 | 실패.1200단계, 정점 관통1.33e-15cm/면 관통0이나 속도18.80cm/s, 변 길이 오차106.1%, 내부 접힘179.37도, 면적비0.288~2.185. 중첩 관통 개선은 확인했으나 착장 성공 아님 |
| NeckEase-Build | Editor 빌드 및35/35 통과. 목둘레 source 폭32mm와 어깨 끝20mm, 충돌 후보 단위벡터 허용치 일치 및 AABB 조기 제외. 실제 착장 진단은 별도 실행 |
| TShirt-Fit-6 | 실패.1200단계, 정점 관통은 수치 오차 수준이나 속도95.62cm/s·변 길이 오차1215.3%. 밑단 인접 정점이 몸 앞/뒤로 갈라짐을 저장 좌표에서 확인. 도착 위치만 검사하는 투영의 몸 통과 문제를 분리 |
| CollisionSweep-Red | Editor 빌드 성공,35/36. 외부→외부의 몸 통과와 외부→내부의 반대편 투영 회귀2개 실패. 최초 접촉 전 정지·접선 잔여 이동 구현 검증 중 |
| CollisionSweep-Green | Editor 빌드 및36/36 통과. 반복마다 마지막 충돌 처리 위치를 보존, 정확한 line/capsule 최초 진입 전 정지 및 접선 잔여 이동. 최대6회 접촉 후 잔여 이동을 버리는 보수적 처리. 별도 읽기 전용 검토에서 현재 인형 크기 capsule의 차단급 문제 없음 |
| TShirt-Fit-7 | 실패.1200단계, 속도0.447cm/s, 봉제 간격1.222cm, 변 길이 오차361.2%. 정점 관통은 수치 오차 수준. 렌더는 티셔츠 형태에 가까워졌으나 겨드랑이·어깨 미봉제와 큰 변형이 남아 성공 아님 |
| ShoulderGuides-Build | Editor 빌드 및36/36. 어깨의 명시적 목표 고정점 추가, 생성기0.3.0-contact-sweep/플러그인0.3.0-dev/템플릿0.3.0-draft로 갱신. 빌드 중 소스 좌우 부호를 재확인해 수정했으므로 이 실행을 최신 소스 확인 근거로 쓰지 않음. 전체 이동 경로 회귀를 추가해 별도 재빌드 |
| ShoulderGuidePaths-Build / TShirt-Fit-8 | 최신 소스 Editor 빌드 및36/36. 모든 템플릿 가이드의60단계 이동 경로가 바디 밖임을 확인. 실제 착장1200단계는 속도0.0046cm/s지만 봉제 간격1.279cm·변 길이 오차340.1%로 실패 |
| ArmProxy-Red | Editor 빌드 성공,35/36. 실제 메시 X=0,Z=7.8cm 단면에서 몸통끝 Y=1.249cm/팔시작 Y=2.010cm인데 기존 Physics가 사이를 메움. 양쪽 Y=±1.65cm 공기 지점이 충돌체 내부인 회귀2개 재현. 바디별 dominant surface와 기준 bone 위치 진단 추가 |
| ArmSurface-Diagnostic-1 / ArmProxy-Build / ArmProxy-Refit-1 | 실제 bone별 dominant surface와 기준 위치 추출. 단일 enclosing capsule은 어깨의 넓은 단면을 팔 전체에 적용하여 틈을 막는 것을 확인. 위팔2구간/아래팔1구간에 bone 축·측정 반경·최소 체적 반경 탐색을 적용해21개 shape/19개 body로 저장. 빌드만 성공한 단계와 바디 생성 단계이며 전체 검증은 별도 |
| ArmProxy-Green | Editor 빌드 및36/36.19개 body/21개 shape, 양쪽 실제 공기 지점의 비관통과 모든 upperarm/forearm-dominant 원본 표면 정점의 충돌체 포함 확인. 머리 표면 포함·템플릿 가이드 경로 검사도 통과. 삼각형 전체 피복이나 실제 의상 착장 성공을 뜻하지 않음 |
| TShirt-Fit-9 | 실패. 새 팔 proxy에서도1200단계 후 봉제 간격1.237cm·변 길이 오차385.5%, 최악점은 초기 겨드랑이(높이8.4cm). 실제 틈이 있는7.8cm보다 위에서 봉제선이 닫히려 하여 앞/뒤 표면에 걸리는 배치 문제를 계속 추적. 전체 착장 미완료 |
| ContactSweep-BodySkin-1 | 생성기0.3.0-contact-sweep으로 Cloth 재생성·저장·skin/원본 동일성·240frame runtime·렌더 통과.312particles,176단계,변 길이 오차2.989%,면 관통0.000881cm,내부 접힘33.996도,최종 runtime변위0.041188cm,정점 관통0. 두 패널 프로브이며 실제 티셔츠 인수 아님. 렌더 직접 확인 |
| ArmholeDepth-Build | Editor 빌드 및36/36. source armholeDepth17mm(초기 겨드랑이높이7.8cm),템플릿0.3.1-draft. 소매/기장 조정 및 가이드 경로 회귀 포함 |
| TShirt-Fit-10 / GravityDiagnostic-Build / TShirt-ZeroGravity-1 | 암홀 조정 후1200단계도 봉제 간격1.068cm·변 길이 오차237.3%로 실패. 중력 비교 인수 추가 및36/36 후 중력0으로600단계 실행: 봉제 간격1.063cm·변 길이 오차262.8%로 같은 실패. 중력만으로 설명되지 않음을 확인했으며 제품 중력 설정을 임의 변경하지 않음 |
| TorsoProxy-Red / TorsoProxy-Build | 실제 몸통끝1.249cm보다 바깥인 Y=±1.45cm도 기존 spine capsule에 포함되어35/36 실패. spine-dominant 표면을 높이2구간으로 나누고 각 구간의 좌우 capsule을 맞추도록 수정 후 Editor 빌드만 성공. 실제 저장/검증은 별도 |
| TorsoProxy-Refit-1 / TorsoProxy-Green | body collision fit0.3.2-measured-body,19body/22shape 저장 후 동일 빌드에서36/36 자동화 통과. 팔·spine 원본 정점6634개 포함과 양측Y=1.45/1.65cm의 실제 공기점 비관통 확인. 실제 의상 및 변형 포즈 검증은 별도 |
| TShirt-Fit-11 | 실패.1200단계,속도0.00029cm/s, 봉제 간격0.69749cm, 변 길이 오차408.8%. 정점 관통은 수치 오차 수준이나 면 관통0.00891cm·내부 접힘166.33도·면적비0.214~9.493으로 의상 봉제·변형은 미통과. 실제 렌더 확인 |
| FreshProjects-Checkpoint-3 | 최신0.3.0-dev의 Win64 BuildPlugin 선컴파일 및 새 C++ 호스트 Editor 빌드 성공. 새 빈 Blueprint/C++ 프로젝트에서 각각36/36 자동화와 저장 BodySkin Cloth 재열기·312particles/240frame 검사 통과. 개발 체크포인트로 A–F 전체 통과나 컴파일러 없는 환경·cook·게임 패키지 검증을 의미하지 않음 |
| TShirt-Fit-11/collision-section-audit.json | UE 원본 삼각형과 충돌체의 X=0 단면 독립 비교. 높이7.8cm의 몸통/팔 틈은 복원되었지만8.0~8.2cm에는 실제 틈을 충돌체가 메움. 옷의 경로·길이 제약을 함께 조사 중이며 추가 임의 치수 조정으로 성공 처리하지 않음 |
| NeckLength-Red / NeckLength-Green | Editor 빌드 후36/37 예상 실패: 기존 뒤목 반쪽1.626187cm는10%변형과 면 관통.005cm를 허용해도 단일 충돌 구 우회 하한1.865905cm에 못 미침. 독립 검토 확인. 원본 cubic의 control polygon 길이 상한과 실제 Physics 끝 구의 최단 외부 경로 하한으로 필요조건 회귀 추가. 뒤목 깊이1.5→4mm만 바꾼 템플릿0.3.2-draft는 Editor 빌드 및37/37 통과. 전체 충돌체 합집합이나 착장 성공을 보장하는 검사는 아님 |
| TShirt-Fit-12 | 뒤목 수정만 적용한1200단계 비교. 봉제 간격2.22e-16cm·정점/면 관통0·내부 접힘70.93도로 개선. 하지만 최대 변 길이 오차342.1%·면적비0.289~8.972로 실패. 전체 구조 변의약29%가10%변형을 초과하며 겨드랑이와 목 고정점 주변에 집중됨. 렌더 직접 확인, 완성품으로 저장/배포하지 않음 |
| FreshProjects-Checkpoint-4 | 템플릿0.3.2-draft와 목길이 회귀를 포함한 Win64 BuildPlugin 및 새 C++ Editor 빌드 성공. 새 Blueprint/C++에서 각각37/37와 저장 Cloth 재열기/312particles 통과. 뒤따른 진단 문서 재실행 경로는 이 체크포인트에 포함하지 않음 |
| DraftReplay-Build / DraftReplay-Build-2 | 개발용 source-draft.json 재실행 옵션 추가. 처음 사용한 JSON 변환 overload가 UE5.8에 없어 빌드 실패. 실제 엔진 헤더의 클래스 지원 template overload로 수정 후 Editor 빌드와37/37 통과. versioned .lgarment 읽기/쓰기 구현이나 관문 B 통과가 아님 |
| TShirt-Guide970-1 | 진단 원본에서 어깨 가이드4개의 높이만9.75→9.70cm 변경. UE 재직렬화 JSON과 입력 문서가 완전히 같은지 별도 확인.1200단계 후 봉제 간격은 수치 오차 수준이나 변 길이 오차340.7%로 실패. 기본 템플릿의 가이드 높이는 변경하지 않음 |
| TShirt-Armhole25-1 | 겨드랑이 경계점4개의 rest 높이만8mm 내린 사용자 편집 진단 원본.1200단계 후 봉제/관통0, 최대 변 길이 오차185.9%·면적비0.379~4.025로 실패. 실제 렌더 확인. 기본 템플릿의 암홀 치수는 유지함 |
| FreshProjects-Checkpoint-5 | REQ-017 전달 전에 시작한 마지막 반복 인수. 최신 진단 재실행 코드를 포함한 Win64 선컴파일·새 C++ Editor 빌드·각37/37·저장 Cloth 재열기 통과. 이후 이 검증을 이번 범위의 반복 필수조건으로 사용하지 않음 |
| ConvexCollision-Red | UE5.8 native Cloth의 convex 지원을 로컬 엔진에서 확인하고 생성/연속접촉/초기 면관통 회귀 추가. Editor 빌드 성공, 기존37개 통과·미구현3개 예상 실패(37/40). 실제 바디 Physics는 아직22개 기존 capsule/sphere로 보존됨 |

구현: 단일 루프 검증, 제약 Delaunay 삼각분할 및 선택적 공유 경계 세분화, 원본 edge 대응, mm→cm 단일 변환, 문서의 cubic arclength·notch·부분 봉제·ease 대응, 네이티브 constrained relaxation, capsule 충돌, 명시적 고정점, 수렴 실패 차단, rest/initial/settled 분리, 2K UV atlas(섬간 32px·외곽 16px·동일 rest 밀도), Cloth Collection 생성. EaseRatio는 LengthA = EaseRatio × LengthB이며 LengthToleranceMm으로 검증한다.

현재 제한: 대칭 평가/hem·binding은 미구현이며 문서 생성 시 명시적으로 거부한다. 천 자체충돌은 미구현이다. 봉제 순서는 목록에 보존하지만 물리적으로 단계별 수행하지는 않는다. 굽힘은 단순 패널 내부 spring regularization이며 실제 원단의 보정된 물성이 아니다. 면-캡슐 거리·내부 접힘·면적 검사는 있으나 전역적인 자체 교차 검증은 없다. SurfaceVertex skin은 명시적 정점 샘플링으로 불연속 가능성이 있으며 실제 의상 포즈 검증을 대체하지 않는다. 엄격한 보간의4초과는 아직 수동 재분배 UI가 없어 작업을 중단한다. 수렴 플래그는 `.lgarment`의 validated 상태가 아니다.

실제 애셋은 플러그인 `Content/Development/`의 CA_NativePanelProbe, CA_BodySkinProbe, M_NativeCloth, PA_NativeProbe에 저장되어 있다. runtime 240 tick의 전체 프레임 변위·정점 관통·고정점 검사와 재열기 원본 동일성 비교를 실행했다. 바뀐 최종 생성 결과로 다시 검증하며, 이전 결과를 최신 출력의 통과 근거로 혼용하지 않는다. 새 빈 Blueprint 프로젝트의 현재 하위 경로는 확인했으며 Designer, 새 C++ 프로젝트, cook/Development/Shipping은 아직 실행하지 않음.

검토: 별도 읽기 전용 Astra/High 검토에서 좌표 double→float 범위와 세분화 예산 P2를 발견했고 재현 후 수정했다. 가변 FBuildResult의 구조 방어와 저장 후 동일성·전체 runtime frame 검증 공백도 추적한다. 검토자가 빌드/UE 실행을 수행한 것은 아니다.

바디 준비: 기존 절차형 더미의 FBX/흰 텍스처/치수/UV 자료를 `Resources/ReferenceBody/`에 고정 복사. 원본 전후 및 사본 SHA-256 일치. provenance.json에 출처와 아직 제공되지 않은 배포 라이선스 검토 상태를 기록했다. 이후 UE 독립 import/UV/skin 검증을 위 실행에서 수행했다. 복사된 원본 UV 보고서와 독립 검사 결과를 구분한다. Blender 실행이나 제품의 외부 저작툴 의존성 추가 없음. Python UV audit는 개발용 검증 도구이며 제품 저작 경로에 포함되지 않는다.
