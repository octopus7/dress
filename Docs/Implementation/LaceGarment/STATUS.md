# LaceGarment 구현 상태

갱신: 2026-10-16. 담당: 별도 Astra / High 구현 대화 `01a14073-fb57-76d2-bf43-db08097d8770`.

작성자/이용조건: REQ-013에 따라 미정. 저장소는 추후 비공개 전환 예정이며 라이선스 파일·문구·배포 이용조건을 임의로 추가하거나 확정하지 않는다. 공개 설정 변경은 실행하지 않았다.

**현재: 사용자 요청으로 구현을 중단하고 중간 커밋으로 보존합니다. 패널/봉제/안정화/Cloth Collection 하위 경로는 구현했으나, Task 1~3 전체 완료가 아니며 실제 대표 의상 피팅·포즈 검증은 미완료입니다. 중단 요청 이후 추가 빌드·테스트는 실행하지 않았습니다.**

**이번 실행 제외:** Task 4 독립 원본 파일, Task 5 실제 저작 WBP/EUW, Task 6 콘텐츠 팩·배포·cook, Task 7 새 프로젝트 최종 인수. 아래 전체 제품 관문 A–F와 과거 검증 기록은 보존하지만 전체 통과를 이번 완료 조건으로 삼지 않습니다. 호스트 빌드·자동화·UE 애셋 저장/재열기·대표 의상 품질 검증은 유지합니다. 배포 가능한 제품이 완성되었다는 뜻은 아닙니다.

- 승인: REQ-012. 실행 계획: `Docs/Plans/2026-10-16-lacegarment-plugin.md`.
- 독립 경계: `PluginDev/LaceGarmentHost/`, `Tools/LaceGarment/`, `Dist/LaceGarment/`, 이 기록 폴더.
- 기존 루트 Source/Content/Config/.uproject와 게임 STATUS 변경 금지.
- 실제 엔진 재확인: `E:/WorkTemp/Epic Games/UE_5.8`, 5.8.3 / CL 58210709 / BuildId 55116800.
- 초기 `git status --short`: 기존 프로젝트 대부분 미추적. 보존하며 브랜치 전환/전체 스테이징/정리하지 않음.
- CIM 프로세스 명령줄 조회는 샌드박스에서 접근 불가. `Get-Process`의 UE/dotnet/MSBuild 조회는 현재 결과 없음. 빌드 시 UE의 mutex와 순차 실행을 사용.

| 관문 | 상태 | 근거 |
| --- | --- | --- |
| A UE 단독 저작 | 일부 하위 경로 확인, 미통과 | 목표 고정점·문서→생성·충돌체·수동 보정·굽힘 진단 제어 포함49개 자동화로 확장. 중단 전 마지막 BendingControl-Green에서 Editor 빌드 및49/49(오류/경고0) 통과. native panel은 ConvexVersion-Native-Save/Reopen에서 최신 원본 동일성·312particles/240frame 통과로 이전 불일치 해소. 실제 저작 UI 및 대표 의상의 착장·포즈 검증 미완료 |
| B 원본 왕복 | 미실행 | |
| C 천·포즈 | 미실행 | |
| D 배포 독립성 | 일부 하위 경로 확인, 미통과 | Checkpoint-4 개발용 BuildPlugin 선컴파일 산출물과 새 Blueprint/C++ 프로젝트에서 각각37개 자동화·저장 Cloth 재열기 확인. 최종 배포 팩·독립 원본·참조 격리 검사는 미구현 |
| E 다중 팩·갱신 | 미실행 | |
| F 새 프로젝트 인수 | 일부 하위 경로 확인, 미통과 | 새 빈 Blueprint/C++ 프로젝트에서 현재 하위 경로 확인. 컴파일러 없는 환경, cook/Development/Shipping 미실행 |

## 검증 기록

### 사용자 요청 중간 보존

- 추가 구현·검토를 중단하고 현재 플러그인 소스·원본 애셋·도구·문서를 커밋한다. 아래 검증은 중단 전에 실행한 기록이며 이번 마무리를 위한 재실행은 하지 않는다.
- `DolphinShorts-LongerRiseWideHem-1`: 360단계, 최대 상대 변 길이 오차0.4437638, 속도0.23068cm/s, 면 관통0.013501cm로 실패. 기본 템플릿에 적용하지 않은 진단 비교다.
- `TShirt-DeeperNeck-1`: 이미 시작된 실행의 종료 결과만 회수했다. 360단계, 최대 상대 변 길이 오차1.0842024, 속도1.29718cm/s, 봉제 간격6.78e-8cm, 정점 관통0cm, fitted=false/엔진 exit7. 기본 템플릿에 적용하지 않았다.
- 실제 티셔츠·쇼츠의 수렴, Cloth 저장/재열기·런타임·포즈 인수는 미완료다. 단순 패널 프로브의 통과를 실제 의상의 통과로 대체하지 않는다.
- 진단 draft·실행 로그·렌더는 로컬 `Saved/Verification/`에 보존하며 생성물로서 커밋에서 제외한다. 제안 단계의 봉제 정점 그룹 솔버 변경은 구현하지 않았다.

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
| ConvexCollision-Green / Green-2 / Green-3 | factory friend의 DLL linkage 오류, ChaosCore 직접 링크 누락, GeometryCore face 추출용 neighbor 저장 누락을 순서대로 확인·수정. Green-3은 빌드 성공했으나 hull 생성 ensure로37/40. 통과로 취급하지 않음 |
| ConvexCollision-Green-4 | Editor 빌드 및40/40 통과. 불변 native FConvex와 병합 polygon face, Chaos 거리·둥근 두께 sweep, capsule/convex 합집합 투영, 초기/최종 정점·면 관통 검사 구현. 실제 바디 애셋 연동은 아직 없음 |
| ConvexNearSurface-Red / ConvexGrazingSurface-Red / ConvexNearSurface-Green | near-corner 외부 시작점에서 GJK의 보수적인 음수 진입값을 버려 반대편 외부로 관통하는 결함을 실제 UE에서 재현(각39/40). 해당 구간이 현재 이동을 포함하고 안쪽으로 이동하면 현재점 접촉으로 처리하여 Editor 빌드 및40/40 통과 |
| ConvexPrecision-Red / ConvexPrecision-Green | 중심Z=1000cm·반두께.00001cm의 double hull이 native float에서 평면으로 붕괴해도 승인되는 회귀39/40. float 양자화 좌표에서 hull을 먼저 계산하여 거부·이전 결과 보존 확인. 최신 Editor 빌드 및40/40(오류/경고0) 통과 |
| ConvexOutward-Red / ConvexOutward-Green / ConvexOutward-Green-2 | 보수적 음수 진입 구간에서 먼 진입면 법선을 사용해 바깥 이동을 막는 회귀39/40. 현재점의 거리 법선으로 수정. 첫 Green은 실제 접선 이동이 Y=0을 넘는 것을 관통으로 잘못 판정한 시험1개 실패. 양의 X면 유지·실제 외부 거리 기준으로 수정하고 결과좌표를 기록한 Green-2는 최신 Editor 빌드·40/40(오류/경고0) 통과 |
| CommitCheckpoint-Native-Reopen | 기존 CA_NativePanelProbe의 새 프로세스 재열기 실패(exit9).312 particles/240frame runtime의 변위·관통은 허용 범위지만 savedGeometryMatchesSource=false여서 전체 실패. 해당 애셋을 최신 검증 완료 대상으로 커밋하지 않으며 삭제하지 않음 |
| CookedConvex-Red / Green / Green-2 | 첫2회는 ReferenceSkeleton 헤더 경로 오류로 빌드 실패. 경로 수정 후 Editor 빌드 및41/41 통과. 실제 cooked convex의 회전/이동 본 계층, rounded corner 거리, 이동 차단, 잘못된 element transform·누락 cooked shape 거부와 결과 보존 확인. 첫 Red에서 동작 실패를 확인하지 못한 점을 구분 기록 |
| ShoulderConvex-Red / Red-2 | 첫 실행은 TObjectPtr 반복 타입 빌드 오류. 수정 후 Editor 빌드 및40/41: 기존22capsule/sphere에서 새6convex 부재와 실제Z8.0cm 겨드랑이 틈 손실·clavicle 표면 포함 부족을 재현 |
| ShoulderConvex-Import-1 | 실패(exit5), 저장 전 거부. 원본 표면의 조밀한 hull을 UE 기본 cooking이 병합하면서 polygon 정점과 plane이 불일치. 허용치를 완화하지 않음 |
| ShoulderDOP-Build / ShoulderDOP-Import-1 / ShoulderDOP-Reopen | Editor 빌드 성공 후26방향 지지 평면으로 원본을 포함하는 다면체 생성. clavicle 각1개·위팔 각2개, native cooking 후 면/정점·폐곡면·원본 포함 검사를 통과하여 저장.19bodies/16capsule+sphere/6convex. 새 프로세스 전체41/41(오류/경고0) 통과: 실제Z7.8/8.0cm 겨드랑이 공기점·팔/쇄골/spine 원본 정점 포함·템플릿 고정점 이동 경로 확인. 대표 의상 및 변형 포즈는 별도 |
| TShirt-ShoulderConvex-1 | 1200단계 완료 후 실패(exit7). 종료를 시도하기 전에 이미 정상 종료되어 실제로 중단한 프로세스는 없음. 봉제 간격.000059cm·정점 관통0이나 속도1.617cm/s·변 길이 오차283.9%·면 관통.04089cm·면적비.526~6.359. 실제 렌더 확인. 새 어깨 충돌체만으로 대표 의상 문제가 해결되지는 않음 |
| CorrectionMap-Red | Editor 빌드 및41/42. native convex의 안전한 평면 구간 조기 제외/평면 접촉 경로 최적화 후 기존41개 통과. 새 수동 보정 시험은 generated correspondence 미구현에서 예상 실패 |
| CorrectionMap-Green / Green-2 | 첫 빌드는 성공했으나 UE Windows의 미구현 GetSHA256Signature에서 assert/exit3, 보고서 생성 실패. 엔진 내장 OpenSSL의 SHA256으로 교체 후 Editor 빌드 및42/42(오류/경고0). 동일 원본의 stable generated/welded/joint ID, 실제 Cloth MaxDistance·skin 적용 및 재생성 재적용, topology 변경 시 원본 보존·미해결·출력 불변 확인. 이 내부 대응 해시는 아직 파일 계약의 canonical archive hash가 아님 |
| CorrectionFailures-Red / Green | Editor 빌드42/43: 양수 double 웨이트가 native float에서0이 되어도 적용되던 회귀 실패. 표현 불가능한 웨이트를 거부하여43/43(오류/경고0). 존재하지 않는 정점/본·미지원 맵·중복 보정·고정점 해제·낡은 봉제·불완전 Cloth 맵도 원본/출력을 보존하며 거부 |
| TShirt-ShoulderConvex-1/collision-section-audit.json | 실제 UE 표면과 capsule+convex 합집합의 독립 단면 비교. Z8.0cm의 틈은 복원되었지만 Z8.2cm에서는 실제0.160cm 틈을 아직 proxy가 메움. 두께 없는 단면 검사이며 전체 표면/움직임의 보장은 아님 |
| DolphinShorts-Fit-1 | 실제 첫1200단계 피팅 실패. 속도0/정점 관통0이나 가랑이 봉제 간격1.30151cm·최대 변 길이 오차354.6%. 렌더 직접 확인. 원본 단면은 높이4.6/5.0/5.2cm에서 다리 사이 틈이 있으나 좌우 허벅지 capsule이 중앙을 .135~.198cm 관통하는 독립 진단 확인 |
| TShirt-ConvexFast-1 | 최적화 후1200단계도 실패. 최대 변 길이 오차283.926%·봉제 간격.00000565cm·속도3.593cm/s. 동일한 겨드랑이 최악 변이 남음. native GJK 시작 구간 변경으로 수치 궤적까지 동일하다고 주장하지 않음 |
| ShoulderDOP-Import-1/uv-gap-audit.json | UE 원본38174정점/73628면/16UV섬의 양의 면적 겹침0·퇴화0, 2K 외곽 최소50.097px·섬 경계 최소77.351px, 밀도 최대/최소1.00000488. 정확한 선분 거리의 교차/평행/일직선 경우도 별도 확인. 페인트 seam 시각 검증은 별도 |
| ThighConvex-Red / Build / Import-1 / Reopen | Editor 빌드42/43: 기존 허벅지 capsule의 실제 틈 차단·원본 표면 누락 재현. 허벅지 각2개26방향 지지 convex를 native cooking·원본 포함 검사 후 저장.19bodies/14capsule+sphere/10convex, collisionFit0.3.4. 새 프로세스43/43(오류/경고0): 높이4.6/5.0/5.2cm 중앙에 최소.03cm 여유와 모든 refit dominant 표면 정점 포함. 생성기0.3.4-cooked-convex/플러그인0.3.4-dev |
| DolphinShorts-ThighConvex-1 | 600단계 실패. 봉제 간격.44343cm·최대 변 길이 오차511.7%. 다리 틈 복원만으로 옷본의 불가능 길이는 해결되지 않음. 렌더 확인 |
| ShortsAnchorLength-Red / PelvisConvex-Red | 실제 삼각분할 변 경로의 길이 상한(10%변형+.02cm봉제 허용)2.0801cm가 허리 고정점 간격3.6cm보다 작아43/44 실패. 골반 캡슐의 허리 공기점 차단·원본 표면 누락 회귀를 추가한 실행은42/44 |
| PelvisAndShorts-Build / Build-2 / PelvisConvex-Import-1 / PelvisAndShorts-Green | 경로 시험 지역 변수 이름 충돌로 첫 빌드 실패, 수정 후 빌드 성공. 골반 각2구간 convex를 저장하여19bodies/13capsule+sphere/12convex(collisionFit0.3.5). 쇼츠 가랑이 깊이15mm(짧은 길이는 밑단과3mm 이상 분리), 허리 목표±1.1cm/Z6.4cm를 적용한 draft0.3.3. 새 프로세스44/44(오류/경고0), 고정점 이동 전 구간·길이 필요조건 통과. 실제 착장 성공과 구분 |
| DolphinShorts-PelvisGuides-1 | 360단계 실패. 가랑이 봉제 간격.0000014cm·정점 관통0이나 최대 변 길이 오차142.6%·속도.1189cm/s. 렌더에서 봉제 개선 확인. 최악 변은 밑단 안쪽. 독립 단면에서 Z4.2cm 실제 내측Y.26565cm를 현재 proxy가Y.031cm까지 메움 |
| ConvexVersion-Native-Save / Reopen | 오래된 CA_NativePanelProbe를 최신 생성기로 다시 저장하고 새 프로세스 재열기 통과. 원본 형상/UV/skin/map 동일성,312particles/240frame,최종 변위.0411868cm·정점 관통0. 이전 CommitCheckpoint의 source 불일치 해소. 두 패널 하위 경로이며 대표 의상 인수가 아님 |
| ThighResolution-Red / Build / Import-1 / Green | Editor43/44로 밑단 높이의 과대 근사 재현. 허벅지 각4구간으로 저장하고 새 프로세스44/44 통과.19bodies/13capsule+sphere/16convex,collisionFit0.3.6. cooked capsule/convex도 body.json에 직접 내보냄 |
| ThighResolution-Import-1/collision-surface-audit.json / BodyFaceCoverage-Red | 독립 검사에서 의상 접촉 영역29454면 중28894면만 단일 collider에 완전히 포함됨. 나머지560면의427개 추가 표면 샘플에서 최대.031723cm 외부 거리 하한 확인. 전체 미포함1167정점은 주로 이번 의상 접촉 영역 밖의 종아리/발이며 가슴4개도 발견. 실제 UE 회귀에서 동일560면 미포함 재현(43/45). 정점별 포함을 면 전체 포함으로 잘못 확대하지 않음 |

| BodyFaceCoverage-Build / Import-1 / Green | 전체 접촉 삼각형 포함을 검증하는0.3.7 저장.29454/29454면의 포함은 증명했으나 자동화44/45: 겨드랑이/다리 공기 틈 과대 근사와 잘못된 primitive 기대 개수 실패. 실제 잔여 capsule/sphere는 head/neck/forearm/hand/calf/foot의10개이며 기존8개 표기는 오류 |
| BodySupportResolution-Build / Import-1 / BodySupportCookDiagnostic-Build / Import-1 | 290방향 팔 polytope와 polar dual 교차 계산의 Editor 빌드 성공. 기본 native cook의 polygon-plane 오차.001193cm로 저장 전 거부. 허용치2e-4cm 유지 |
| BodySupportSparse-Build / Import-1 | 50방향도 기본 cook 실패. 단계 진단에서 Build max오차5.386e-6cm→MergeFaces8.735e-4cm→MergeColinearEdges동일. 단순 정점 계산 오차와 면 병합 문제를 분리 |
| BodySupportInflation-Build / Import-1 / Green / BodyPartitionDiagnostic-Build / Import-1 | 일정 면 오프셋이 만드는 작은 bevel 대신 내부 중심 기준 균일 확대를 적용해 저장 성공. 접촉면 전체 포함. 자동화44/45이며 구간 겹침 수정이 같은 코드의 capsule 위치에 적용된 오류를 좌표 대조로 발견. 이후 capsule 코드를 복원하고 convex 구간만 수정 |
| BodyPartitionRanges-Build / Import-1 / BodyCanonicalHull-Build / Import-1 | 올바른 convex 구간 수정 및10개 primitive 기대값 정정. thigh_l 기본 cook의 면 오차.000218465cm로 저장 전 거부. 정점 공간 정렬만으로는 동일 실패. 기준 완화 없음 |
| BodyCookPreflight-Build / Import-1 | 균일 확대와 일정 면 오프셋의 두 보존적 형상만 기본 native constructor로 사전 검사. thigh_l의 한 구간만 후자를 사용. 사전 객체를 저장에 재사용하지 않고 실제 PhysicsAsset 기본 cooking 후 표면 포함을 다시 검증. 다리 틈 복원됐지만 팔의 얕은 접평면 표현은 추가 개선 필요 |
| BodyAngularSupport-Build / Import-1 / collision-surface-audit.json | 팔의 좌표 평면 방향을15도 간격으로 세분화한74방향, 나머지26방향. Editor 빌드 및 실제 저장 통과. collisionFit0.3.8,19bodies/10capsule+sphere/19convex. 독립 접촉면29454/29454포함, 미증명0. 전역 미포함1069정점은 종아리/발이며 이번 접촉 영역 밖. 전체 자동화 및 대표 의상 검증은 별도 |

| BodyAngularSupport-Green | 새 UE 프로세스45/45(오류/경고0). 원본 접촉면 포함, Z7.8/8.0cm 겨드랑이 틈과 다리/허리/밑단 공기점 기준 통과. Z8.2cm 추가 공간·대표 의상 피팅 및 포즈는 아직 별도 문제로 남음 |
| DolphinShorts-CompleteSurface-1 | 360단계 실패. 최대 변 길이 오차88.54%·속도4.736cm/s·면 관통.03049cm. 최악 변은 앞 허리 중앙 고정점 인접. 렌더 확인 |
| DolphinShorts-WaistSides-1 / WiderHem-1 | 진단 문서만으로 양옆 허리 가이드 및 밑단 폭을 각각 비교. 각240단계 실패: 최대 변 길이 오차101.36% /137.64%, 속도12.418 /5.575cm/s. 두 렌더 확인. 실패한 비교본을 기본 템플릿에 적용하지 않음 |
| ArmNegativeSpace-Red | 45/46. 실제 삼각형까지 .17282/.04840cm 떨어진 겨드랑이 지점에 반경.1/.03cm 공간을 요구하면 기존 cooked convex가 각각 .01376/−.19237cm밖에 확보하지 못하는 회귀 재현 |
| ArmNegativeSpace-Build / Import-1 / ArmNegativeSpaceSimplified-Build / Import-1 | 전체 삼각형을 보호 공간별 지지 반공간으로 나누고 안전한 그룹을 병합. 두 빌드 성공, 기본 native cook의 float 면 오차로 저장 전 거부. 7개/팔에서4개/팔로 병합하고 불필요한 지원 방향을 줄여도 오른팔 한 영역의 .0002205cm 오차가2e-4cm 기준을 초과 |
| ArmNegativeSpaceSupports-Build / Import-1 / Green | Editor 빌드·저장·새 프로세스46/46(오류/경고0). 오른팔 한 영역만6축+필수 공간 평면의 보존적 대안 사용, 나머지26방향+필수 평면. 실제 기본 native cooking 후 원본 포함·보호 공간·면 정밀도 재검사. collisionFit0.3.9,19bodies/10capsule+sphere/23convex. 독립 audit29454/29454접촉면 포함, 미증명0. 전역 미포함1069종아리/발정점은 이번 접촉 영역 밖 |
| DolphinShorts-LongerRise-1 / LongerRise-NoGravity-1 | 진단 문서에서 상단을6mm 올리고 초기 배치를6mm 내려 초기 허리 높이를 유지. 중앙 밑위는21mm. 중력 포함360단계96.43%, 중력0인240단계90.17% 최대 변 길이 오차로 모두 실패. 최악 변은 동일한 옆 허리 인접45–46. 중력 포함 렌더 확인. 무중력은 초기 배치부터 별도 실행했으며 저장 snapshot의 정적 재실행은 아님 |
| TShirt-ProtectedArmSpace-1 | 최신 겨드랑이 충돌체로240단계 실패. 최대 변 길이 오차229.28%·속도.5438cm/s·봉제 간격.0000062cm·정점 관통0. 렌더 확인. 최악 변384–57은 여전히 겨드랑이 |
| ClearFitMotion-Diagnostic / Diagnostic-2 | 최초 진단 테스트의 TPair 초기화 컴파일 오류 수정 후 Editor 빌드 및47/47. 티셔츠/쇼츠 최악 변을 rest 길이 쪽으로 이동하는 두 경로가 각 convex의 동일 분리 평면과 capsule 선분 거리로 두께까지 확보됨을 증명하고 실제 SweepBodyCollision의 목표점 도달 확인. 직접 CCD 정지라는 가설을 배제했으며 전체 cloth 제약 수렴의 증명은 아님 |
| ThighTaperSpace-Red | Editor 빌드47/48, 새8개 공기점 실패. Z3.8cm 실제 다리 둘레3.573cm에 비해 proxy4.776cm이며, 실제 표면 밖 .03cm 공기 구를 기존 thigh convex가 침범. 원본을 포함하는 동시에 과대 테이퍼 근사를 줄이는 수정 진행 중 |
| ThighTaperSpace-Build / Import-1 / Green | 허벅지8구간 및15도 간격74방향으로 얕은 테이퍼 표현. Editor 빌드·저장·48/48 및 독립접촉면29454/29454포함. Z3.8cm 한쪽 proxy 단면 둘레3.872cm로 감소(두께 제외). 상부3영역이6축 fallback을 사용하여 추가 개선 대상 |
| ThighTaperSupports-Build / Import-1 / Green | 허벅지의74방향과6축 사이에26방향 보존적 대안을 추가. 상부3영역이26방향을 사용하고 허벅지6축 사용0개. Editor 빌드·실제 저장·새 프로세스48/48(오류/경고0), 독립29454/29454접촉면 포함. collisionFit0.3.10,19bodies/10capsule+sphere/31convex. 같은 LongerRise 원본으로 실제 피팅 재실행 중 |
| DolphinShorts-ThighTaper-1 | 같은 LongerRise 원본으로480단계 실패. 최대 변 길이 오차62.43%로 감소했으나10% 기준 초과, 속도.2018cm/s·봉제 간격.00000018cm·정점 관통0. 최악 변은 동일45–46. 실제 렌더 확인 |
| BendingControl-Red / Green | 굽힘 정규화 입력의 유한성/범위 거부와 실제 효과를 검사한48/49 예상 실패 후 구현하여 Editor 빌드49/49(오류/경고0). 기본 반복당.05는 유지. 개발용 Run-Fit에서 굽힘0 비교와 반복 횟수 진단 가능하며 사용값을 보고서에 명시. 수렴/품질 기준 변경 없음 |
| DolphinShorts-NoBending-1 | 굽힘0만 바꾼240단계 비교도 실패. 최대 변 길이 오차58.55%·속도.3216cm/s, 최악 변45–46 유지. 실제 렌더 확인. 굽힘 보정만으로 남은 옷본/충돌 조합을 설명할 수 없음 |
| TShirt-DeeperArmhole-1 | 겨드랑이 깊이17→22mm만 바꾼360단계 비교 실패. 최대 변 길이 오차132.29%로 감소했지만 최악 변은 목둘레의 어깨 고정점 인접143–144로 이동. 실제 렌더 확인. 해당 반쪽 목 경계 원본1.73930cm/정착1.99439cm로14.67% 길이 차이가 있어 목 깊이4→6mm 비교를 준비 |

독립 피팅 진단: 저장된 쇼츠의 pin/structural/seam만5,000회 반복하면 기본/양옆 가이드의 최대 변 길이 오차가4.24%/2.88%까지 내려가지만 바디를 .3249/.4062cm 관통한다. 앞 허리 pin→공유 가랑이의 rest 최단 경로1.52924cm에 비해 현재 배치의 직선거리부터1.81264cm여서 해당 배치에서는10% 기준을 충족할 수 없다. 모든3D 배치의 불가능성 증명은 아니며, 다음 비교는 밑단 폭보다 중앙 밑위 길이이다.

커밋 조율: `Saved/CommitCoordination/safe-body-snapshot-20261016.json`의15개 파일을 게임 대화가 `a9fe0ac`로 커밋했고 동결 해제를 전달했다. 커밋 내용을 읽어 확인했다. 이 대화에서는 stage/commit하지 않았다. 조율 종료 후 구현을 이어가지 못한 중단을 사용자 지적 후 인정하고 Task 1~3 작업을 재개했다. 이 manifest는 해당 커밋 시점의 스냅샷이며 이후 바디 수정의 최신 해시로 사용하지 않는다.

구현: 단일 루프 검증, 제약 Delaunay 삼각분할 및 선택적 공유 경계 세분화, 원본 edge 대응, mm→cm 단일 변환, 문서의 cubic arclength·notch·부분 봉제·ease 대응, 네이티브 constrained relaxation, capsule 충돌, 명시적 고정점, 수렴 실패 차단, rest/initial/settled 분리, 2K UV atlas(섬간 32px·외곽 16px·동일 rest 밀도), Cloth Collection 생성. EaseRatio는 LengthA = EaseRatio × LengthB이며 LengthToleranceMm으로 검증한다.

현재 제한: 대칭 평가/hem·binding은 미구현이며 문서 생성 시 명시적으로 거부한다. 천 자체충돌은 미구현이다. 봉제 순서는 목록에 보존하지만 물리적으로 단계별 수행하지는 않는다. 굽힘은 단순 패널 내부 spring regularization이며 실제 원단의 보정된 물성이 아니다. 면-캡슐 거리·내부 접힘·면적 검사는 있으나 전역적인 자체 교차 검증은 없다. SurfaceVertex skin은 명시적 정점 샘플링으로 불연속 가능성이 있으며 실제 의상 포즈 검증을 대체하지 않는다. 엄격한 보간의4초과는 아직 수동 재분배 UI가 없어 작업을 중단한다. 수렴 플래그는 `.lgarment`의 validated 상태가 아니다.

실제 애셋은 플러그인 `Content/Development/`의 CA_NativePanelProbe, CA_BodySkinProbe, M_NativeCloth, PA_NativeProbe에 저장되어 있다. runtime 240 tick의 전체 프레임 변위·정점 관통·고정점 검사와 재열기 원본 동일성 비교를 실행했다. 바뀐 최종 생성 결과로 다시 검증하며, 이전 결과를 최신 출력의 통과 근거로 혼용하지 않는다. 새 빈 Blueprint 프로젝트의 현재 하위 경로는 확인했으며 Designer, 새 C++ 프로젝트, cook/Development/Shipping은 아직 실행하지 않음.

검토: 별도 읽기 전용 Astra/High 검토에서 좌표 double→float 범위와 세분화 예산 P2를 발견했고 재현 후 수정했다. 가변 FBuildResult의 구조 방어와 저장 후 동일성·전체 runtime frame 검증 공백도 추적한다. 검토자가 빌드/UE 실행을 수행한 것은 아니다.

바디 준비: 기존 절차형 더미의 FBX/흰 텍스처/치수/UV 자료를 `Resources/ReferenceBody/`에 고정 복사. 원본 전후 및 사본 SHA-256 일치. provenance.json에 출처와 아직 제공되지 않은 배포 라이선스 검토 상태를 기록했다. 이후 UE 독립 import/UV/skin 검증을 위 실행에서 수행했다. 복사된 원본 UV 보고서와 독립 검사 결과를 구분한다. Blender 실행이나 제품의 외부 저작툴 의존성 추가 없음. Python UV audit는 개발용 검증 도구이며 제품 저작 경로에 포함되지 않는다.
