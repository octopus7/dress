# LaceGarment 구현 상태

갱신: 2026-10-17. 담당: 별도 Astra / High 구현 대화 `01a14073-fb57-76d2-bf43-db08097d8770`.

작성자/이용조건: REQ-013에 따라 미정. 저장소는 추후 비공개 전환 예정이며 라이선스 파일·문구·배포 이용조건을 임의로 추가하거나 확정하지 않는다. 공개 설정 변경은 실행하지 않았다.

**현재: 2026-10-17 사용자의 “중단하고 중간 커밋” 요청으로 구현과 검증을 중단했습니다. 생성기0.3.14의 마지막 완료 검증은 Editor 빌드·57/57(오류/경고0), 작은 두 Cloth 프로브의 저장·새 프로세스 다시열기와 단일 캡슐 포즈 검사 통과입니다. 티셔츠는 변형22.2837%·면 관통0.013921cm로 실패했고, 실제 쇼츠·전체 바디 포즈 인수도 미완료입니다. 중간 커밋을 위해 추가 빌드·테스트를 실행하지 않았습니다.**

**이번 실행 제외:** Task 4 독립 원본 파일, Task 5 실제 저작 WBP/EUW, Task 6 콘텐츠 팩·배포·cook, Task 7 새 프로젝트 최종 인수. 아래 전체 제품 관문 A–F와 과거 검증 기록은 보존하지만 전체 통과를 이번 완료 조건으로 삼지 않습니다. 호스트 빌드·자동화·UE 애셋 저장/다시열기·대표 의상 품질 검증은 유지합니다. 배포 가능한 제품이 완성되었다는 뜻은 아닙니다.

- 승인: REQ-012. 실행 계획: `Docs/Plans/2026-10-16-lacegarment-plugin.md`.
- 독립 경계: `PluginDev/LaceGarmentHost/`, `Tools/LaceGarment/`, `Dist/LaceGarment/`, 이 기록 폴더.
- 기존 루트 Source/Content/Config/.uproject와 게임 STATUS 변경 금지.
- 실제 엔진 재확인: `E:/WorkTemp/Epic Games/UE_5.8`, 5.8.3 / CL 58210709 / BuildId 55116800.
- 초기 `git status --short`: 기존 프로젝트 대부분 미추적. 보존하며 브랜치 전환/전체 스테이징/정리하지 않음.
- CIM 프로세스 명령줄 조회는 샌드박스에서 접근 불가. `Get-Process`의 UE/dotnet/MSBuild 조회는 현재 결과 없음. 빌드 시 UE의 mutex와 순차 실행을 사용.

| 관문 | 상태 | 근거 |
| --- | --- | --- |
| A UE 단독 저작 | 일부 하위 경로 확인, 미통과 | 생성기0.3.14 Editor 빌드·57/57 통과 및 작은 프로브 실제 저장/새 프로세스 다시열기 확인. 기본 쇼츠는 이전 정적 피팅293단계·변 길이 오차6.03%·정점/면 관통0 및 Cloth 저장을 통과했으나 실제 runtime 포즈 실패. 대표 의상 인수와 UI는 별도 |
| B 원본 왕복 | 미실행 | |
| C 천·포즈 | 일부 하위 경로 확인, 미통과 | 생성기0.3.14의 두 패널·단일 캡슐 프로브는 저장/다시열기 및 팔 올리기·루트 회전·무릎 굽힘720프레임 통과. 면 검사는5개 단계 끝에서 수행하며 매 프레임 검사는 정점·고정점 대상. 실제 티셔츠/쇼츠의 전체 바디 착장·포즈 인수는 미완료 |
| D 배포 독립성 | 일부 하위 경로 확인, 미통과 | Checkpoint-4 개발용 BuildPlugin 선컴파일 산출물과 새 Blueprint/C++ 프로젝트에서 각각37개 자동화·저장 Cloth 다시열기 확인. 최종 배포 팩·독립 원본·참조 격리 검사는 미구현 |
| E 다중 팩·갱신 | 미실행 | |
| F 새 프로젝트 인수 | 일부 하위 경로 확인, 미통과 | 새 빈 Blueprint/C++ 프로젝트에서 현재 하위 경로 확인. 컴파일러 없는 환경, cook/Development/Shipping 미실행 |

## 검증 기록

### 기본 포즈 검수 확대 — 2026-10-17

- `SurfaceSewing-BodySkinSave/BodySkinReopen`:0.3.14에서 바디 스킨 프로브를 저장/새 프로세스 다시열기하고720frames의5단계를 모두 통과했다. 원본 대응 일치,312particles/620tethers, 움직이는 고정점 오차3.69e-7cm. 작은 단일 캡슐 프로브의 성공이며 대표 의상/전체 바디 인수와 구분한다.
- `ShortsLength24-SurfaceSewing-1`: 사용자 중단 요청에 따라 실행을 취소했다. 마지막 출력120단계·속도0.46084cm/s·변형42.889%·봉제 간격0이며 최종 결과/Cloth 저장/포즈 검증에 도달하지 않았다. 기존 CA_DolphinShorts 해시20A0A3…F88CB 유지. 허리·가랑이의 초기 높이와21mm 밑위는 유지하며 밑단 높이를4mm 올렸다. 이전 저장본은 오래된 생성기·스킨을 사용하므로 길이 변경만의 인과 비교로 해석하지 않는다. 기본 템플릿은 변경하지 않았다.
- `SurfaceSewing-NativeSave/NativeReopen`:0.3.14에서 두 패널 프로브를797단계로 재생성하여 실제 저장/새 프로세스 다시열기 통과. 원본 geometry/UV/skin/맵 대응 일치,312particles/620tethers/240frames. 정적 변형5.2511%·면 관통0, runtime 변형8.3257%·면 관통0.003848cm·접힘28.04도. 이전 프로브3개/재질/Physics의 백업과 해시는 `SurfaceSewing-BeforeAssets`에 보존했다.
- `TShirt-SurfaceSewing-1`: 같은 FreeUnderarm 원본240단계, 변형22.2837%·면 관통0.013921cm·속도0.011475cm/s·접힘96.508도, 최악 변868–1230으로 실패했다. 이전0.3.13의22.3532%/0.013760cm와 사실상 유사한 위치에 정착해 초기 봉제 수정만으로 대표 착장은 해결되지 않았다. 정면 렌더를 확인했고 기본 템플릿은 유지한다.
- 저장된 `TorsoSkin-Sitting-1` 정면/측면, `TorsoSkin-ArmsRaised-1` 정면/측면, `TorsoSkin-Knees90-1` 정면을 추가 시각 검토했다. 목표 자세는 보이지만 팔 올리기의 어깨·겨드랑이와 앉기의 골반 경계에 국소적인 돌출/접힘이 남아 있다. 수치상 충돌 누락 결과와 함께 바디 포즈 전체 인수를 보류한다. 새 렌더나 애셋 수정은 아니다.
- `SurfaceSewing-Green`: 생성기0.3.14-surface-sweep Editor 빌드·57/57 통과, 오류/경고0. 초기120단계 면 관통0·변형6.0149%, 최종384단계 면 관통0·변형5.1428%·속도0.034521cm/s. 기존 허용치를 유지했다. `TShirt-SurfaceSewing-1`은 같은 FreeUnderarm 원본으로 실제 착장 비교를 진행 중이며 최신 저장/다시열기는 아직 실행하지 않았다.
- `TShirt-FreeUnderarmWide-1`: 겨드랑이 고정점을 제거한 동일 후보에서 폭36→44mm만 확대한 비교. 240단계에서 변형46.1754%, 면 관통0.013252cm, 속도0.164447cm/s로 실패했다. 기존22.3532%보다 악화되어 기본 템플릿에 반영하지 않았다.
- `SurfaceSewing-Red`: Editor 빌드 성공,56/57. CoarseSurfaceContact의 초기 봉제120단계에서는 면 관통0.046472cm·변형6.0508%였다. 최종 연립 풀이에만 면 표본 접촉이 있고 초기 봉제에는 정점 접촉만 있는 공백을 재현했다. 변 중점/면 중심의 이동 경로를 충돌 검사하고, 고정점과 봉제 결합 질량을 고려해 정점에 보정을 분배하는0.3.14를 검증 중이다. 이는 표본 경로 검사이며 완전한 연속 삼각형 충돌을 주장하지 않는다.
- `BodyPoseDiagnostics-Green`: Editor 빌드·55/55 통과, 오류/경고0. 이후 `LowerLegCoverage-Red`는 종아리·발 포함 검사에서 완전 포함이 증명되지 않는2438면을 재현해54/55(HTTP timeout 경고1). 실제 표면의 전체 인접 삼각형을 포함하는 calf/foot 캡슐 생성과 terminal foot 축 처리를 추가했다.
- `LowerLegCoverage-Build` 빌드 성공 → `LowerLegCoverage-Save` 충돌0.3.11 저장 → 새 프로세스 `LowerLegCoverage-Green`55/55, 오류/경고0.19bodies/41shapes이며 기존 겨드랑이·허벅지 빈 공간 검사도 유지된다. body.json은 collisionFitVersion/capsules만 달라지고 정점·면·UV·convex는 동일하다. 저장 경로가 바디 관련5애셋을 다시 저장하므로 PA 외 SK/M/T의 바이너리 해시도 바뀌었고 Skeleton 해시는 같다. 포즈 충돌 전체 합격을 뜻하지 않는다.
- `LowerLeg-Reference/Knees90/Sitting/ArmsRaised-1`: 새 프로세스의332686개 충돌 표본에서 기준 누락0, 무릎196개/최대0.016687cm, 앉기3041개/0.172818cm, 팔 올리기6567개/0.322043cm. 독립 audit는 기준 자세만 전체 수치 검사 통과, 나머지3개는 충돌 포함 실패이다. 목표 본 각도·방향과 붕괴 면0은 유지된다. 남은 최대 누락은 무릎 앞, 앉기의 골반 위 몸통, 팔 올리기의 겨드랑이 경계이며 스킨 영향과 충돌체를 추가 조사한다.
- `BodySkinDiagnostics-Build` 빌드 후 읽기 전용 `BodySkin-ArmsRaised/Sitting/Knees90-1`에서 누락 주변3정점의 실제 영향을 기록했다. 몸통 정점1313(기준Z7.447cm)에 thigh_r36.394%/upperarm_r8.980%, 겨드랑이 정점1291(Z7.983cm)에 thigh_r11.264%가 포함되어 있다. 기준 hip 관절Z6.2cm보다 훨씬 높은 몸통에 다리 영향이 전해지며, 앉기에서 해당 정점은(-0.420,1.272,7.027)로 이동한다. 근접 영향 진단은 저장 애셋을 바꾸지 않았다.
- `TorsoSkin-Red`: 몸통 양옆90정점의 팔·다리 영향을 재현해55/56. UE MeshDescription 기반 명시적 RepairSkin 경로로 원본 프로필을 보존하고 기존 몸통 영향에 재분배했다. `TorsoSkin-Build` 성공, `TorsoSkin-Save-1`3566정점 보정·SK_OB11 저장, `TorsoSkin-Green` 새 프로세스56/56(오류/경고0). `TorsoSkin-Repeat-1` 변경0/저장false 및 SK SHA-256 8C0D5D2DAE6B7C4CF496EA3EBB41C662FB1B45F12C55E8C0B3603BC3F9722BD7 유지. 바디 포즈 인수는 별도 검사 중이다. 스킨 수정 전 애셋은 Saved/Verification/BodySkin-BeforeAssets에 보존했다.
- `TorsoSkin-Reference/ArmsRaised/ArmsForward/Elbows90/Knees90/Sitting/HeadTurn-1`:7개 모두 기준 정점·UV0·삼각형 순서 해시ef68bdc9380527a76fd18af4e5dd471e8e7142d7가 보정 전 body.json과 일치, 목표 포즈 및 붕괴 면0. 충돌 표본은 기준/고개 회전0, 팔 옆1812개/최대0.263743cm, 팔 앞1411개/0.172793cm, 팔꿈치36개/0.013096cm, 무릎196개/0.016687cm, 앉기3587개/0.223552cm이다. 기준/고개 회전만 전체 수치 통과. 몸통의 잘못된 영향 회귀는 해결했지만 팔 앞으로·앉기의 최대 충돌 누락은 이전보다 커져 전체 포즈 개선/완료로 보고하지 않는다. 앉기 면적비 최대는6.9645→3.0177로 감소했으나 최소는0.06976→0.02152이다. 앉기·팔 앞으로 측면 렌더를 확인했다.
- `WaistbandDiagnostics-Build`는 mutable Cloth 패턴 getter에 const 변수를 사용한 컴파일 오류로 실패했고, 수정한 `WaistbandDiagnostics-Build-2` 빌드는 성공했다. 바디 형상/UV/삼각형 해시 진단 및 쇼츠 임시 허리선 고정 진단을 포함하며, 이 빌드에서는 전체 자동화를 다시 실행하지 않았다.
- `NativeMotionDiagnostics-Green`: 포즈별 직전 프레임 속도·MaxDistance 도달 수·native3D rest 오차 진단을 추가한 Editor 빌드 및56/56 자동화 통과, 오류/경고0. 이후 `ShortsRuntime-MotionLimits-1`은 중력0/20반복/8substeps/tether 조건에서 무릎 원본2D 오차48.8055%, native3D rest 오차41.2187%, 면 관통0.009028cm로 실패했다. 해당 단계 최대 속도0.007409cm/s, 이동 한도 최대 비율0.67677,99% 도달 정점0개이므로 단순 정착 시간이나 MaxDistance 포화가 주된 원인이라는 근거는 없다. 이 진단은 저장 Cloth를 변경하지 않는다.
- `ShortsRuntime-Waistband-1`: 임시 복사본의 허리 경계96정점을 pelvis에 고정하고 tether2077개를 다시 생성했다. 실제20반복/8substeps에서 기준13.6594%, 무릎91.5407%·면 관통0.008353cm로 실패했다. 허리선 전체 고정만으로 가랑이 문제가 해결되지 않으므로 기본 설정에 적용하지 않았다. 저장 CA_DolphinShorts SHA-256은20A0A3289DE03429A46FB98124D964ECDC7E04ABA3DFA581636C888C966F88CB로 유지된다.
- `ShortsRuntime-Iterations160NoFriction-1`: 충돌0.3.11 바디, 실제160반복/8substeps, 중력1/마찰0/기본 굽힘의 임시 복사본 진단. 기준·팔·복귀9.81%로 통과했지만 무릎40.8749%·면 관통0.003916cm로 실패. 최악 변은 뒤 허리1146–1147이며 직접5% 복원 경로는 막히지 않았다. 동적 정점 tether 직접 거리/기준길이1.34155, 고정점1147에도 양쪽 허벅지 합계29.708% 영향이 있다. native 시뮬레이션 EMA606~768ms로 이 설정은 속도도 좋지 않다. 이전160반복 실험과 바디 충돌 버전도 달라 순수 마찰 효과 비교로 해석하지 않는다. 제품 기본 설정과 저장 쇼츠는 변경하지 않았다.
- `BodyPose-Reference-1`, `ArmsRaised-1`, `ArmsForward-1`, `Elbows90-1`, `Knees90-1`, `Sitting-1`, `HeadTurn-1`: 읽기 전용 렌더/실제 component 본 변환/스킨 정점 기록. 독립 audit에서 목표 방향 및90도·고개45도 확인,38174정점/73628면,0면적 붕괴0. 기준 자세 정점 차이6.78e-15cm. 기준 정면, 팔 옆 수평 정면, 팔 앞으로 측면, 앉기 측면 이미지를 확인했다. 전체 시각 검수 완료는 아니다.
- 충돌 표본332686개에서 기준8400개·최대 누락 거리0.12744cm, 팔 옆 수평14967개/0.32204cm, 팔 앞으로8430개/0.12744cm, 팔꿈치90도8436개/0.12744cm, 무릎90도8573개/0.28402cm, 앉기11399개/0.28418cm, 고개 회전8400개/0.12744cm. 전체 바디 충돌 인수는 실패이다. 기존 ContactSurfaceCoverage는 종아리·발을 제외한29454면 검사였으며 이 결과와 구분한다.
- 기준 바디 원본의 독립 half-space 하한 검사도 calf_l/r와 foot_l/r 정점의 누락을 확인했다. 기존 capsule 실측 재생성 대상에는 이 네 본이 없었다. 회귀 실패를 확인한 뒤 이 영역을 보완하고 포즈별 관절 누락을 별도로 다룬다. 면적비는 팔 앞으로 최소0.0121, 앉기 최대6.9645 등 국소 변형이 있으므로0면적 붕괴0만으로 관절 품질 합격을 선언하지 않는다.
- `TShirt-CenteredSleeve-RaisedGuides-1`:480단계 후 속도0.01983cm/s지만 변형67.7141%, 면 관통0.034667cm로 실패. 최악 변413–218은 겨드랑이 고정점 인접이다. 충돌 허용0.005cm까지 반영한 보수적 평면 경로 하한2.58787cm가 해당 옷본 최단 변 경로의10% 상한2.51362cm를 넘는다(`anchor-path-bound-with-tolerance.json`와 재현 스크립트). 물리 가이드를 유지하고 위쪽 목/어깨 구간을4mm 연장한 `TShirt-ArmholeAllowance-1`도240단계에서 변 길이 오차32.9794%·면 관통0.031766cm·속도0.06738cm/s로 실패했다. 최악 변399–400은 겨드랑이 인접이며 렌더를 확인했다. 기본 템플릿은 변경하지 않았다.
- ArmholeAllowance의 양쪽 겨드랑이 최단 원본 변 경로3.60538cm가 피팅 후4.16689cm로 늘어났다. 중앙 평면을 통한 보수적 경로 하한3.88415cm는10% 허용3.96592cm보다 작아 폭 부족의 불가능 증명은 아니다. `TShirt-UnderarmWidth-1`은 겨드랑이 폭만36→44mm로 비교했으나240단계·52.9698% 변형·면 관통0.039485cm로 실패했다. 최악 변1251–1032는 고정 겨드랑이에 붙은 소매 쪽이다. 이 후보도 기본값에 적용하지 않았다. 다음 후보는 ArmholeAllowance에서 진단용 겨드랑이4고정점만 제거하고 목/어깨6개를 유지하는 FreeUnderarm이며 아직 결과가 없다.
- `TShirt-FreeUnderarm-1`:240단계 후 변형18.0671%, 속도0.095908cm/s, 정점/봉제 관통0, 면 관통0.062648cm로 실패. 정면 렌더 확인. 내보낸 cooked convex 평면의 독립 삼각형 clipping에서도 같은 최대 관통을 재현했다. 뒤쪽 겨드랑이 삼각형1272–1273–1314와1272–1314–1313이 같은 변 내부점(-0.44763,1.29337,8.13238)에서 collider12를 가로지른다. 진단은 `face-contact-audit.json`에 저장했다. 정점만 제약하는 피팅의 면 접촉 공백을 작은 회귀 사례로 조사하며, 실패 후보를 기본 템플릿에 적용하지 않았다.
- `SurfaceContact-Red`: Editor 빌드 성공,56/57. 기존 캡슐 봉제 사례의 메시 간격을10mm로 둔 회귀에서336단계·속도0.02353cm/s·변형5.1404%로 정착했지만 면 관통0.033597cm로 실패했다. 대표 의상의 변형 문제와 별개로 정점 충돌만으로 면 접촉을 유지하지 못하는 경로를 재현했다. 변 중점/면 중심 접촉 잔차를 같은 정점의 연립 풀이에 분배하고, 면 관통이 남은 정착 후보는 계산을 계속하도록 수정하여 검증 중이다. 원본 topology와 최종 연속 면 검사는 유지한다.
- `SurfaceContact-Green`: Editor 빌드 성공,56/57. 새 회귀의 면 관통은0으로 해소됐지만360단계에서 속도0.053733cm/s여서0.05 기준을 만족하지 못했다. 기존56개는 모두 통과했다. 허용치를 바꾸지 않고 회귀의 계산 한도를480단계로 확장한 재검증을 진행한다.
- `SurfaceContact-Green-2`: 생성기0.3.13-surface-contact/플러그인0.3.13-dev Editor 빌드 및57/57 통과, 오류/경고0. 새 회귀는396단계·속도0.027393cm/s·면 관통0·변형5.1423%로 통과했다. 저작 피팅의 표본 접촉 보완이며 native runtime 변경이나 대표 의상 인수 통과를 뜻하지 않는다. 기존 저장 Cloth의 최신 재생성/다시열기는 별도로 실행한다.
- `SurfaceContact-NativeSave/NativeReopen`:0.3.13의 두 패널 Cloth 저장 및 새 프로세스 다시열기 통과. 재생성 원본과 저장 geometry/UV/skin/맵/tether 일치,312particles/240frames, native tether620개. runtime 변형8.3258%·면 관통0.003847cm·접힘28.04도. 저장 렌더 확인. `SurfaceContact-BodySkinSave/BodySkinReopen`도 최신 바디 스킨으로 저장/원본 대응/새 프로세스720frames의5단계 모두 통과. 최대 변형8.3259%, 최대 면 관통0.003871cm, 움직이는 고정점 오차3.69e-7cm. 두 경로의 충돌은 단일 캡슐이므로 전체 바디 대표 의상 성공과 구분한다. 이전4개 프로브/재질/Physics 애셋은 `SurfaceContact-BeforeProbes`에 해시와 함께 보존했다.
- `TShirt-SurfaceContact-1`: FreeUnderarm와 동일 원본·240단계에서 면 관통0.062648→0.013760cm로 감소했으나 변형18.067→22.353%로 늘어 전체 실패. 속도0.029311cm/s·접힘94.05도·면적비0.5861..1.3089, 정면 렌더 확인. 최악 변은 뒤 겨드랑이868–1230이다. 과거 폭 확대 비교에는 겨드랑이 고정점이 남아 있었으므로, 이번에는 고정점6개를 유지한 채 겨드랑이 폭36→44mm만 바꾼 FreeUnderarmWide를 별도 진단으로 준비했다. 기본 템플릿은 유지한다.
- `ShortsRuntime-FreeSlidingNoBend-1`: 기존 단일 조건 비교 뒤 굽힘/마찰의 상호작용을 확인하기 위해 중력0·마찰0·굽힘0을 함께 적용한 임시 복사본 진단. 무릎 변형30.5788%·접힘177.859도·면 관통0.004960cm로 실패했고, 기준 자세도 접힘138.364도로 실패했다. 세 요소를 제거해도 인수 기준을 충족하지 못하므로 제품 설정에 반영하지 않았다.
- `SkinnedBodyDiagnostics-Green` 및 `SkinnedBodyNativeFactory-Green`: 임시 스킨 삼각형 충돌 비교 경로의 Editor 빌드·57/57 통과(오류/경고0). 최초 수동 조립 진단은38174정점/73628면이었고, 최종 코드는 엔진 `FPhysicsAssetUtils::GenerateSkinnedTriangleMesh`를 사용해 중복 정점 병합·본 매핑을 엔진에 맡긴다.
- `ShortsRuntime-SkinnedBody-1/SkinnedBody-NativeFactory`: 중력0에서 rigid proxy 대신 실제 스킨 충돌체를 사용하는 임시 복사본720프레임 완료. 엔진 로그의 실제 skinned triangle collider 추출을 확인했다. 내장 factory 출력은36816정점/73628면, 무릎 변형53.5906%로 최초53.7586%와 유사하며 접힘93.53도이다. 충돌체 교체만으로 변형 실패가 해소되지는 않았다. 보고서 관통 수치는 원래 proxy 기준으로 남아 있어 실제 스킨 표면 합격에 사용할 수 없으며 모드는 항상passed=false이다. SK_OB11의8C0D5D…22BD7 및 CA_DolphinShorts의20A0A3…F88CB 해시 유지. 제품 충돌체는 변경하지 않았다.

### 중간 커밋 이후 재개

최신 후속 결과(기존 실행 결과를 읽어 기록했으며 기록 정리 시 새 빌드·테스트를 실행하지 않음):

- `DolphinShorts-FinePCG-1`: 진단 원본2246정점,293단계에서 피팅 통과. 속도0.029796cm/s, 최대 변 길이 오차6.03056%, 봉제 간격·정점 관통·면 관통0, 내부 접힘79.0906도, 면적비0.879624~1.178334. 직접 풀이0회/PCG20063회. 실제 Cloth runtime 인수는 아님. 보고서 버전은0.3.8이며 이후 생성기 표기를0.3.9-feasible-start로 갱신했다.
- 기본 쇼츠 템플릿0.3.4-draft에 검증한 형상·곡면 배치·옆 허리 가이드·2mm 메시/1mm 경계 간격을 반영했다. 기본 선형 풀이는PCG이며 sparse LDLT는 명시적 진단 옵션으로 남겼다. 티셔츠 기본 형상은 변경하지 않았다.
- `BodyUV-Checker2048`, `BodyUV-checker-1024/512`, `BodyUV-sides-2048/1024/512`: 읽기 전용 렌더18장 생성. 저장 바디 애셋5개의 전후 해시 동일. 확인한 이미지에서 좌우 색 복제나 뚜렷한 외부 색 번짐은 없었으나 뒤통수의 국소 체크무늬 밀도 집중이 보인다. 명시적 이미지 축소와 bilinear 샘플링 검사이며 자동 mip-chain 검증은 아니다.
- `BodyUV-Inputs/local-distortion.json`: 면적 가중 비등방성P50 1.086/P95 1.231/최대1.992. `local-density.json`의 전체 px/cm P05/P50/P95는60.479/102.924/156.987로, 차트 평균 밀도 일치가 모든 국소 밀도의 균일성을 뜻하지 않는다.
- 재개 후 추가 시각 검토: checker1024의 정면/측면/후면과 sides2048/1024의 측면/후면을 직접 확인했다. 좌우 색이 분리되고 외부 자홍색의 뚜렷한 번짐은 보이지 않으며, 뒤통수 체크무늬 집중과 중앙 도색 경계의 작은 계단 현상은 보인다. 이전에 생성된 이미지의 검토이며 새 렌더 실행은 아니다.
- `ShortsDefault-MaterialRed`: Editor 빌드 성공,54/55. 누락된 저장 재질Tint/Roughness 두 항목에서 실패. `BodyMaterial-Upgrade`로 기본 흰색Tint와Roughness0.65를 추가·저장했다. 이전 ThighTaperSupports와 새 body.json의SHA-256은46B9987ACBEE0C6177D8EEAC67C8070737BF22416C4FF03BD5C5C18FE46DBB5D로 같아 바디 기하·충돌 보고서는 동일하다.
- `ShortsDefault-MaterialGreen`: 저장된 결과55/55, 경고 포함 성공0/실패0/미실행0. 기본 쇼츠·PCG 경로와 저장 재질 검사는 통과했으며 대표 의상 runtime·포즈 인수는 포함하지 않는다.

다음 미완료 항목은 기본 쇼츠의 실제 Cloth 생성/저장/새 프로세스 다시열기/전체 바디 포즈 검사, 티셔츠 후보 피팅, 최신 생성기로 작은 프로브 애셋 재생성과 다시열기이다. Task4~7은 진행하지 않는다.

- `ShortsDefault-NativeSave-1`: 기본 템플릿도293단계·6.03056% 변형·면 관통0으로 피팅 및 CA_DolphinShorts 저장 성공. 저장 원본 대응 일치,2173particles/720프레임, 최대 정점 관통0·움직이는 고정점 오차8.04e-7cm. 그러나 기준 자세 변형24.8612%, 무릎 자세63.3146%·면 관통0.006272cm·면적비 최대1.5615로 전체 실패. 기준/무릎 렌더 직접 확인. 저장된 의상을 인수 완료로 취급하지 않는다.
- 해당 실행의 연립 보정643.24초 중 선형 풀이469.26초·충돌 sweep121.50초·에너지 평가30.92초. `strain-diagnostic.json`에서 기준 자세의 최대 변형은 뒤 허리 고정점 인접1147–2225, 무릎 자세는 옆 허리0–1083으로 확인했다.
- `NativeTethers-Red`: Editor 빌드 성공,54/55(성공 중 HTTP 연결 확인 timeout 경고1개). ClothCollection 회귀에서 동적 정점245개의 native tether 연결 누락을 재현했다. 엔진 런타임 설정의 tether 활성화만으로 Collection 연결 데이터가 생성되지는 않는다. 기본 생성 경로에 engine geodesic tether 생성을 추가하고 검증 중이다.
- `NativeTethers-Green`: Editor 빌드 및55/55 통과(HTTP 연결 확인 timeout 경고1개). 동적 정점의 연결 존재, 고정 정점 종점, 유효한 geodesic 길이 검증. 저장/다시열기 원본 비교에 tether 연결과 길이를 추가했다. 이 자동화 통과가 대표 의상 포즈 통과를 뜻하지 않는다.
- `ShortsRuntime-Baseline-1`: 읽기 전용 runtime 진단에서 native tether0개, 기준24.8612%/무릎63.3144%로 기존 실패 재현. float2D rest와 원본 double 간 미세 측정 차이를 제외하면 동일하다. 원본 재생성은 하지 않아 sourceIdentityChecked=false이다.
- `ShortsRuntime-Tethers-1`: 임시 복사본에 geodesic tether8676개만 추가. 기준21.2709%/무릎59.5393%, 무릎 면 관통0.005168cm로 개선됐지만 여전히 실패. 저장 CA_DolphinShorts의 전후SHA-256은20A0A3289DE03429A46FB98124D964ECDC7E04ABA3DFA581636C888C966F88CB로 같아 변경되지 않았다. 다음 비교는 runtime 반복 횟수만160으로 높이는 진단이며 제품 기본값은 아직20/최대40/8substeps이다.
- `RuntimeIterations-Build` / `ShortsRuntime-Iterations160-1`: 빌드 후 설정160/최대160/8substeps만 비교. 약8분 실행, 기준·팔·복귀의 변형7.32~7.38%로 통과했으나 무릎50.0828%·면 관통0.005732cm로 실패. 최악 변은 가랑이108–426으로 이동했다. 이 실행은 실제 적용 반복 횟수 통계 추가 전이다.
- `RuntimeDiagnostics-Build` / `ShortsRuntime-ZeroGravity-1`: 빌드 후20회 설정에서 중력만0인 진단. 엔진 통계로 실제20회/8substeps 확인. 기준·팔6.0304%와 복귀8.2717%는 통과, 무릎49.6809%·면 관통0.007180cm로 실패. 중력만으로 포즈 문제를 설명할 수 없고, 제품 중력은 유지한다. 생성기 표기는0.3.10-native-tethers로 갱신했으며 실제 쇼츠 저장 애셋은 아직 tether 미포함 원본이다.
- `RuntimeBending-Build` / `ShortsRuntime-NoBending-1`: 중력·20회·tether를 유지하고 굽힘만0. 기준27.1453%·무릎59.4019%로 실패하며 접힘163.28/177.50도로 악화. 굽힘 제거를 제품 설정으로 적용하지 않는다.
- `ShortsRuntime-HingeBending-1`: 굽힘 강도1과 다른 설정을 유지하고 교차 변 스프링 대신 native 힌지 각도 제약으로 비교. 기준24.9246%·무릎78.0322%·무릎 면 관통0.010521cm로 실패. 제품의 굽힘 설정은 유지하고 실제 실패 변의 복원 경로가 posed collision에 막히는지 진단을 추가했다.
- `NativeRuntimeDiagnostics-Green`은 새 sweep 진단의 헤더 누락으로 컴파일 실패했다. include와 UE5.8 FName 속성 API를 수정한 `NativeRuntimeDiagnostics-Green-2`는 Editor 빌드와55/55 자동화 통과(오류/경고0). 이는 진단 경로의 컴파일/회귀 확인이며 실제 무릎 포즈 문제의 해결을 뜻하지 않는다.
- `ShortsRuntime-ClearCorrection-1`: 중력0 진단에서 기존 무릎49.6809% 실패를 재현. 최악 변426–108의 두 끝점을 원본 길이+5%까지 옮기는 직접 sweep은 차단 거리0이었다. 하지만 이 이동은 이웃 변107–108의 오차를24.85→38.93%,425–426을3.61→20.65%로 키운다. 전체6396변 중384개가10%를 넘으므로 한 변의 이동 가능성을 전체 원단의 실현 가능성으로 해석하지 않는다.
- `NativeEndpointDiagnostics-Build`는 Editor 빌드 성공(테스트 미실행). `ShortsRuntime-EndpointLimits-1`에서426/108의 스킨 대비 이동0.309/0.399cm는1cm 한도보다 작고, tether 직접 거리 비율0.941/0.958 및 충돌면 거리0.143/0.138cm를 확인했다. 이 두 정점의 직접 이동 제한이 포화된 상태는 아니다.
- `ShortsRuntime-ZeroGravityNoBending-1`: 중력0에서 굽힘 강도만0으로 비교. 같은426–108이 무릎48.0179%로 실패하며 접힘179.29도. 굽힘 제거만으로 해소되지 않았고 제품 설정은 유지한다.
- `NativeFrictionDiagnostics-Build`는 Editor 빌드 성공(테스트 미실행). `ShortsRuntime-ZeroGravityNoFriction-1`은 중력0 비교에서 마찰만0으로 변경하여 무릎34.7433%·면 관통0.003419cm·접힘88.40도. 최악 변이 가랑이에서 뒤 허리1146–1147로 이동했고, 동적1146의 tether 직접 거리 비율은1.283이었다. 마찰이 잔류 변형에 영향을 주지만 전체 기준은 여전히 실패하며 제품 마찰0.4를 변경하지 않았다.
- `TShirt-CenteredSleeve-FeasibleStart-1`: 이전 후보의 양쪽 어깨 가이드(0,±1.2,9.75)가 현재 convex 안에 약0.088cm 들어가 초기 거부(0단계). 보고된 collision plane으로 확인했다. 모든 가이드를 동일하게Z+0.2cm 이동한 `TShirt-CenteredSleeve-RaisedGuides-draft.json`은 가이드 간 거리를 유지하며 보수적 평면/캡슐 거리 최소0.0403cm로 두께0.03cm 밖이다. 별도 피팅 진단 중이며 기본 템플릿에는 적용하지 않았다.

| 실행 | 실제 결과 |
| --- | --- |
| WeldedJunction-1 | Editor 빌드 성공, 기존49개 통과/새 접합점 검사1개 실패. 초기부터 닫힌 세 패널 접합점을60단계 뒤에 묶어 부하 중 벌어지는 문제 확인 |
| WeldedJunction-2 | 초기부터 닫힌 봉제 즉시 그룹화, 분리 봉제는 닫힘 후 충돌 sweep 확인 뒤 그룹화. 생성기0.3.5. Editor 빌드 및50/50 통과(오류/경고0), 전이 접합점 일치와 상충 고정점 거부 확인 |
| DolphinShorts-WeldedJunction-1 | 기존 LongerRiseWideHem 초안 그대로360단계 비교. 봉제0, 최대 변 길이 오차31.8502%(이전44.3764%), 속도0.459cm/s, 면 관통0.014786cm, 내부 접힘124.515도, 면적비0.759..1.435. 실패이며 실제 렌더 확인 |
| TShirt-WeldedJunction-1 | 기존 DeeperNeck 초안 그대로360단계 비교. 봉제0, 최대 변 길이 오차111.616%, 속도0.2412cm/s. 최악 변이 겨드랑이의 봉제점과 내부 정점 사이로 이동했으나 여전히 실패 |
| DolphinShorts-WeldedNoBend-1 | 굽힘만0으로 비교.240단계, 오차27.0612%, 내부 접힘177.405도, 면 관통0.019337cm로 실패. 굽힘 제거를 기본값으로 적용하지 않음 |
| BoundedEase-1 | 닫힌 봉제 이후 원본 길이의±5% 범위로 투영하여 국소 변형을 분산. 최종10% 기준과 원본 rest는 유지. Editor 빌드·51/51 통과,5% 고정 가이드 성공 및30% 강제 늘어남 거부 확인 |
| DolphinShorts-BoundedEase-1 | 같은 초안360단계, 오차17.2570%, 속도0.29494cm/s, 면 관통0.020412cm, 접힘75.384도, 면적비0.898..1.195로 여전히 실패 |
| DihedralFit-1 / DihedralFit-2 | UE 헤더의 Chaos::Softs 네임스페이스 누락으로 첫 빌드 실패. 수정 후 Editor 빌드·51/51 통과. 생성기0.3.6은 opposite-vertex 거리 대신 설치된 Chaos dihedral 미분으로 패널 굽힘을 계산 |
| DolphinShorts-Dihedral-1 | 480단계, 오차17.9668%, 속도0.08666cm/s, 면 관통0.007608cm, 접힘61.301도.360단계에서 속도가0.05 아래였지만 최종 속도는 다시 상승했으며 전체 실패 |
| DolphinShorts-WaistEase-1 | 허리 폭만44→48mm 비교.360단계, 오차16.0378%, 속도0.01470cm/s. 안정 속도여도 변형 기준 실패 |
| DolphinShorts-SoftBend-1 | 같은 허리 여유 초안에서 굽힘0.0005 비교.240단계, 오차16.0304%, 속도0.08377cm/s. 남은 오류는 굽힘 강도만으로 해소되지 않음 |
| WrappedPlacement-1 | 초기 원통 배치 API와 원본 rest/삼각형 보존·잘못된 반경 거부 검사 추가. Editor 빌드·53/53 통과 |
| DolphinShorts-Wrapped-1 / SideGuide167-1 / FineMesh-1 | 곡면 배치·옆 고정점·조밀한 메시를 각각 비교했으나 최대 변 길이 오차16.0375% /16.8443% /20.9569%로 실패. 기본 템플릿에 적용하지 않음 |
| TShirt-UnderarmGuides-1 | 바디를 가로지르지 않는 겨드랑이 가이드 추가 비교.360단계, 오차130.273%, 속도0.11476cm/s로 실패 |
| CoupledFit-1~3 | 연립 보정 도입 후 Editor 빌드 성공,52/53 통과. 소형 캡슐 천의 진동 회귀를 검출. 별도 굽힘 투영과 연립 보정이 서로 취소하는 문제 수정 중의 실패 기록 |
| CoupledFit-4 | 굽힘·길이·접촉을 같은 연립식과 비선형 오차에 포함. Editor 빌드·53/53 통과. 소형 캡슐 천650단계, 속도0.02528cm/s, 길이 오차8.279%, 봉제/정점 관통0. 대표 의상 통과를 의미하지 않음 |
| DolphinShorts-Coupled-1 | 단계당 한 번의 연립 보정은 실제 쇼츠에서 부족.720단계, 오차95.2211%, 속도14.24525cm/s로 실패 |
| CoupledFit-5 / DolphinShorts-Coupled-2 | 중력은 단계당 한 번, 비선형 보정은 최대8회로 변경. Editor 빌드·53/53 통과. 실제 쇼츠720단계에서 오차11.0938%, 속도1.98087cm/s, 면 관통0.009800cm로 여전히 실패 |
| DolphinShorts-CoupledNoGravity-1 | 중력0 진단에서도720단계 후 오차10.5948%, 속도0으로 정체. 중력을 끄는 것을 해결책으로 채택하지 않음. 충돌 후 Newton 방향이 오차를 줄이지 못할 때의 대체 감소 방향을 검증 중 |
| CoupledFit-6 / DolphinShorts-Coupled-3 | Newton 방향이 채택되지 않을 때 음의 기울기와 더 긴 backtracking 추가. Editor 빌드·53/53 통과. 쇼츠720단계, 오차11.4545%, 속도2.34715cm/s로 실패. 짧은 경계 변의 오차가 남아 상대 변형률 기준 보정을 검증 중 |
| CoupledFit-7 | 상대 변형률을 사용한 첫 변경은 Editor 빌드 성공,52/53 통과. 소형 캡슐 천4800단계 후 오차5.1%이나 속도0.29543cm/s로 실패. 굽힘 항도 같은 무차원 기준으로 맞추는 후속 검증 진행 |
| CoupledFit-8 | 굽힘 항도 무차원으로 변경. Editor 빌드 성공,52/53 통과. 소형 천 오차5.1%·속도0.27262cm/s로 실패. 단계 내 예측 위치를 고정하는 일관된 근접 항으로 수정 중 |
| CoupledFit-9 / DolphinShorts-Coupled-4 | 단계 내 중력 예측 위치를 고정하는 근접 항 적용. Editor 빌드·53/53 통과, 소형 천801단계·속도0.04378cm/s·오차5.251%. 실제 쇼츠1200단계·오차16.3491%·속도5.51830cm/s로 실패. 여러 충돌체 접촉 시 연립 이동 방향이 뒤틀리는 문제를 검증 중 |
| CoupledFit-10 / DolphinShorts-TangentContact-1 | 몸 안쪽을 향하는 방향을 접촉 접선공간으로 제한한 연립 풀이. Editor 빌드·53/53 통과(검사 중 엔진 HTTP 경고1). 쇼츠720단계·오차6.4936%·속도2.37138cm/s로 속도 및 면 관통 기준 실패 |
| PosedCollision-1 | 본별 component-space 포즈에서 실제 cooked capsule/convex를 읽는 overload 추가. 강체 이동 일치·잘못된 포즈 거부·실패 시 기존 출력 보존 포함 Editor 빌드·54/54 통과(오류/경고0). 실제 착장 포즈 인수는 별도이며 아직 미실행 |
| DolphinShorts-Residual-1/2 |240단계에서 최종 선형 상대 잔차와 비선형 보정량 측정.100회는 잔차0.018321·보정0.00093750cm·속도2.11871cm/s,200회는 잔차0.00056753·보정0.00318346cm·속도1.53693cm/s. 두 번째는 초기 순차 보정도200회이므로 동일 초기 결과에 대한 순수 선형 반복 비교는 아님 |
| RefinementBudget-1 | 초기 순차/선형/비선형 반복 상한을100/200/32로 분리. Editor 빌드·54/54 통과(오류/경고0). 소형 천802단계·속도0.04345cm/s·오차5.251%. 대표 의상 비교 진행 중 |
| DolphinShorts-Residual-3 |100/200/32로360단계. 오차5.5089%·속도0.74553cm/s·최종 보정0.00016050cm·선형 상대 잔차0.0045169로 여전히 실패. 이전8회 결과의240→720단계 위치 차이도 최대0.009013cm·RMS0.001687cm여서 큰 드레이프 이동보다 작은 진동임을 확인 |
| DoubleDihedral-1 | Chaos와 같은 부호/각도 정의를 배정밀도 위치·미분으로 계산. 작은 삼각형의 중앙 수치 미분과1000cm 평행이동 불변성 회귀 포함 Editor 빌드·55/55 통과(오류/경고0). 실제 쇼츠 비교 진행 중 |
| DolphinShorts-DoubleDihedral-1 / BlockSolve-1 |240단계 실제 쇼츠 비교. 배정밀도 굽힘은 오차5.5178%·속도0.44708cm/s,3×3 블록 전처리는 오차5.5194%·속도0.58017cm/s로 모두 실패. 최종 선형 상대 잔차는 각각0.0014472/0.0042688 |
| PoseRuntime-1 / BlockPreconditioner-1 | 포즈 실행 경로,3×3 블록 전처리, 다중 캡처 후 카메라·조명 해제 적용. 각각 Editor 빌드·55/55 통과 |
| Current-Native-Save / Current-Native-Surface-Reopen |312개 native 천 정점의 저장 및 새 프로세스 다시열기,240프레임·원본 대응·단계 끝 면 검사 통과. 단일 캡슐의 두 패널 프로브이며 대표 의상 인수가 아님 |
| Current-BodySkin-Poses-Save / Current-BodySkin-Surface-Poses-Reopen | 실제 바디 skin을 사용하는 두 패널을 저장/다시열기하고720프레임·다섯 포즈 단계의 면 검사 통과. 최대 변형률 약8.3564%·면 관통0.003847cm·고정점 오차0.000000311cm. 충돌은 단일 캡슐이며 전체 바디 착장 성공이 아님. 기존 포즈의 고정점이 거의 움직이지 않아 루트 회전과 고정점 이동 확인을 추가함 |
| SparseDirect-1 | 자유 정점4096개 이하 UE 내장 Eigen sparse LDLT 추가, 잔차 확인 실패/큰 입력은 PCG로 복귀. Editor 빌드 성공(포즈 컴포넌트의 deprecated setter 경고1), 자동화55/55 통과(검사 오류/경고0). 실제 쇼츠 및 변경된 포즈 프로브 확인 중 |
| DolphinShorts-SparseDirect-1 |360단계, 직접 풀이26175회·PCG0회. 최종 선형 상대 잔차1.18e-14이나 속도0.55248cm/s·길이 오차5.5219%·면 관통0.010221cm로 실패. 마지막 비선형 보정량0.000004787cm. 반복 선형 풀이의 잔차만으로 진동을 설명할 수 없으며, 기본 템플릿에 반영하지 않음 |
| PoseDiagnostics-Build / MovingAnchors-Save / MovingAnchors-Reopen | Editor 빌드 성공. 현재 직접 풀이로 두 패널 애셋 저장·새 프로세스 다시열기,720프레임과 다섯 단계 면 검사 통과. 실제 고정점 이동0.104687cm·추종 오차0.000000369cm·최대 면 관통0.003871cm. 렌더에서 팔 회전 부호가 의도와 반대로 팔을 내리는 동작임을 확인했으므로 팔 올리기 인수로 간주하지 않는다. 회전 부호와 실제 팔꿈치 높이 상승 검사를 수정 후 재확인한다 |
| ArmLiftDirection-Build / ArmLift-Reopen | Editor 빌드 성공. 팔 회전 부호 수정 후 저장 애셋을 새 프로세스에서 다시 열어720프레임·다섯 단계 면 검사 통과. 양쪽 팔꿈치 높이가 각각0.743069cm 상승한 것을 실제 본 변환과 렌더에서 확인. 고정점 이동·추종 검사도 통과. 이 근거로 Task 2의 작은 패널 경로는 완료하며 Task 3의 실제 의상 인수는 계속 미완료 |
| DolphinShorts-Nonlinear128-1 | 같은 초안에서 비선형 상한만32→128.180단계 후 속도0.55087cm/s로 실패. 마지막 단계는35회 만에 보정량3.04e-8cm로 종료되어 단순 상한 부족이 아님. 마지막 정점 이동을 독립 계산하면 변형 에너지가5.53e-5 증가했고, 근접 항까지 포함한 변화도+5.63e-5(접촉 벌점 제외). 중력 예측을 보정의 초기 위치로 사용하는 순서를 수정 중 |
| FeasibleStart-1 | 중력 예측을 목표로 유지하고 이전 유효 위치에서 비선형 감소를 시작하도록 수정. Editor 빌드·55/55 통과(엔진 HTTP 연결 확인 경고1). 소형 캡슐 천802단계·속도0.04339cm/s·변형률5.251%. 실제 쇼츠 비교 진행 중이며, 작은 애셋은 최종 솔버 확정 후 다시 저장/다시열기 필요 |
| DolphinShorts-FeasibleStart-1 | 같은 초안256단계에서30회 연속 속도·봉제·변형·정점 관통 기준 충족. 속도0.038913cm/s·변형률5.5131%·봉제/정점 관통0. 마지막 비선형 반복2회, 직접 풀이6062회. 다만 정확한 면 관통0.009773cm로 최종 실패(접힘81.941도·면적비0.748..1.285는 기준 충족). 진동 수정과 면 관통 문제를 구분하여 조밀한 메시를 비교 중 |
| DolphinShorts-FineFeasible-1 |2246정점의 조밀한 초안.180단계에서 변형률6.052%·속도0.28867cm/s. 직접 풀이의120→180단계에 약468초가 걸려 비용 비교를 위해 이 실행만 중단했다. 최종 fit/면 관통 결과 없음, 성공으로 간주하지 않음. 같은 초안을 PCG로 비교할 옵션 추가 중 |

### 사용자 요청 중간 보존

- 추가 구현·검토를 중단하고 현재 플러그인 소스·원본 애셋·도구·문서를 커밋한다. 아래 검증은 중단 전에 실행한 기록이며 이번 마무리를 위한 재실행은 하지 않는다.
- `DolphinShorts-LongerRiseWideHem-1`: 360단계, 최대 상대 변 길이 오차0.4437638, 속도0.23068cm/s, 면 관통0.013501cm로 실패. 기본 템플릿에 적용하지 않은 진단 비교다.
- `TShirt-DeeperNeck-1`: 이미 시작된 실행의 종료 결과만 회수했다. 360단계, 최대 상대 변 길이 오차1.0842024, 속도1.29718cm/s, 봉제 간격6.78e-8cm, 정점 관통0cm, fitted=false/엔진 exit7. 기본 템플릿에 적용하지 않았다.
- 실제 티셔츠·쇼츠의 수렴, Cloth 저장/다시열기·런타임·포즈 인수는 미완료다. 단순 패널 프로브의 통과를 실제 의상의 통과로 대체하지 않는다.
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
| Probe-Integrity-Build / Probe-CanonicalConfig | 각 Editor 빌드 및 10/10 통과. 다시열기 동일성/전 프레임 검사 추가, canonical runtime 속성을 생성 단계 한 곳에서 설정하도록 수정 |
| Native-Probe-Reopen-Integrity | 새 프로세스에서 rest/3D/UV/face/stitch/skin/고정점이 원본 재생성과 동일. 240/240 frame particle 확인, 최고 변위 0.08652435cm, 최고 정점 관통 0cm, 고정점 변위 0cm |
| Probe-Fold-Red | 9/10 통과. capsule 내부 인접면 접힘 179.592도로 새 품질 회귀 실패 |
| Probe-Bend-Regularization | Editor 빌드 및 10/10 통과. 패널 내부 opposite-vertex 굽힘 spring 추가. 최대 내부 접힘 52.520도, 198단계, 변 길이 오차 9.943% |
| Native-Probe-5 | 재생성·저장·runtime·렌더 exit=0, passed=true. 원본 데이터 동일, 816 particles, 최종 변위 0.1488592cm, 정점 관통 0cm. 3장 이미지 직접 확인: 기존 미세한 반전 접힘 개선, 양옆 접힘은 남아 있음. 완성 의상 판정 아님 |
| Probe-FaceCollision-Red | 10/11 통과. 모든 정점이 몸 밖에 있어도 면이 몸을 가로지르는 배치를 기존 검사가 허용함을 재현 |
| Probe-SurfaceValidation | Editor 빌드 및 11/11 통과. 초기/결과 삼각형 면과 capsule의 정확한 거리 검사, 내부 인접면 접힘 120도 및 면적비 0.5~1.5 제한 추가 |
| Native-Probe-Surface / Native-Probe-Surface-Reopen | 생성·렌더·새 프로세스 다시열기 모두 exit=0/passed=true. 정지 결과 면 관통0cm, 내부 접힘52.520도, 면적비0.965~1.219, runtime 전 프레임 최고 변위0.1904002cm, 정점 관통0cm, 고정점 변위0cm |
| Body-Red | 11/12 통과. 독립 바디 애셋 누락을 예상대로 검출 |
| Body-Import-Build / Body-Import-1 | Editor 빌드 후 고정된 독립 FBX/PNG에서 플러그인 바디·골격·재질·Physics 생성. 15cm,20bones,19collision bodies. 자동 Physics fit의 착장 적합성은 별도 검증 대상 |
| Body-Green / Body-Render-Build | 각 12/12 통과. 유효한 골격 참조, 정점당1~4개·합65535의 skin, UV margin, 절대 import 경로 제거 확인 |
| Body-Import-1/uv-audit.json | UE LOD0 추출 좌표로 독립 검사. 38174verts/73628tris/16charts,197795 후보쌍의 정확한 UV clipping에서 면적 겹침0·퇴화0. 외곽50.097px/2K, 최대/최소 섬 밀도비1.00000488. 섬 사이 거리·도색 경계 시각 검수는 아직 별도 |
| Body-Render-1 | 자체 바디를 UE renderer에서 3방향 캡처. 정면/사선 이미지 직접 확인. 실제 의상 착장·관절 포즈 검수는 아님 |
| Body-Skin-Red | 미구현 전송 테스트 실패(12/13) |
| Body-Skin-Green / Body-Skin-Chest | 구현 후12/13. 엄격한 삼각형 보간이 일부 지점에서5영향을 만들어 계약에 따라 거부. 자동으로 잘라내지 않음 |
| Body-Skin-Modes | Editor 빌드 및13/13 통과. 명시적 SurfaceVertex 모드는 가까운 표면 삼각형 정점의 전체 원래 웨이트를 복사하고 BarycentricStrict는4초과 시 실패. 봉제 그룹 동일 웨이트·render/sim 보존 및 실패 시 기존 데이터 보존 확인 |
| Body-Skin-ProbeBuild | Editor 빌드 및13/13 통과 |
| BodySkin-Probe-1 / BodySkin-Probe-Reopen-1 | 별도 CA_BodySkinProbe 생성·렌더·새 프로세스 다시열기 모두 exit0/passed. reference skeleton20bones, 원본의 sim/render skin 동일,816 particles, 최종 변위0.1488565cm, 정점 관통0. 좌표 검사는 simulation reference bone transform을 component space로 적용함 |
| Document-Red / Document-Seams-Red | 미구현 source model 테스트가 예상대로 실패(각13/16,13/17). 곡선·보정·Undo/Redo·봉제 구간/순환 포함 |
| Document-Green | Editor 빌드 및17/17 통과. 안정 ID 참조/수치 범위/봉제 순환 검사, cubic 평가, 실제 UE transaction Undo/Redo, topology가 달라진 보정의 미해결 보존. UI/archive 연결 및 생성 bridge는 아직 미구현 |
| AdaptiveMesh-Red / AdaptiveMesh-Green | 각17/19,18/19. 전체 균등 세분화의 과다 곡선 face와 봉제 경계 샘플 불일치를 재현. 필요한 변만 분할하여 경계 대응은 해결했지만 곡선 face 예산 회귀는 남음 |
| BoundarySamples-Regression | 18/20. 일직선상 다수의 경계 점에서 기존 ear clipping이 퇴화 삼각형을 만드는 문제 추가 재현 |
| ConstrainedMesh-Green | Editor 빌드 및20/20 통과. 경계를 제약으로 보존하는 Delaunay·내부 격자·선택적 변 분할 적용. 조밀한 곡선·공선 경계·경계 독립 샘플링 확인 |
| Constrained-Probe-1 / Constrained-Probe-Reopen-1 | 생성기0.2.0-adaptive-probe로 새 저장·렌더·새 프로세스 다시열기 통과. 176단계, 내부 접힘33.996도, 면적비0.980~1.034, 변 길이 오차2.989%, 면 관통0.000881cm(0.005cm 허용 기준 이내). 312particles,240frame,최고 runtime변위0.059982cm,정점 관통0,고정점 변위0. 렌더 사선 이미지 직접 확인 |
| Constrained-BodySkin-1 / Constrained-BodySkin-Reopen-1 | 새 생성기의 바디 skin Cloth 저장·렌더·다시열기 exit0/passed.312particles,20bone reference,원본 sim/render skin·형상 동일성 통과. 최종 runtime변위0.041187cm,정점 관통0 |
| DocumentSampling-Red | Editor 빌드 성공,20/23. 곡선 경계·초안 생성 차단·부분 봉제/ease의 새 테스트가 미구현 stub에서 예상대로 실패 |
| DocumentSampling-Green / DocumentSampling-Integration | Editor 빌드 및각23/23,25/25 통과. cubic arclength 경계·notch 파라미터·부분 봉제·ease·원본 고정점 대응. 길이가5% 다른 경계의 실제 드레이핑 수렴, 반대 방향 notch의 명시적 stitch, 교차 notch/중복 봉제 거부 확인 |
| InnerLoops-Red | 26/27. 안쪽 루프의 실제 면적 제외가 미구현 상태에서 실패 |
| SourceCurveReview-Red / SourceCurveReview-Green | 각26/29,27/29. 검토가 지적한 성긴 샘플의 원곡선 자기교차 누락 및 curve arc/chord 봉제 허용치 차이를 재현. 첫 수정은 유효한 접선 연결까지 근접 교차로 거부하여 미통과 |
| CurveReview-And-InnerLoops | Editor 빌드 및29/29 통과. 출력 간격과 별도인 정밀 source polyline 검사·동시 봉제 재샘플링·할당 전 누적 샘플 예산 추가. 안쪽 루프 OddWinding 삼각분할, 면적 제외, 방향 독립성, 외부/접촉/중첩 구멍 거부 확인. source curve 검사는 수치 근사이며 해석적 곡선 교차 인증은 아님 |
| DocumentCutout-Red / DocumentCutout-Green | 각29/30 예상 실패 후 Editor 빌드 및30/30 통과. 문서 inner loop가 실제 rest 메시에서25mm²를 제외하고 native drape에서도 유지됨. 각 loop의 source curve 교차 및 포함 관계를 검사 |
| FreshBlueprint-Checkpoint-1 | BuildPlugin 실패. 배포 플러그인의 UHT 규칙에서 편집 노출 property의 Category 누락 검출. 모든 문서 property에 명시적 Category 추가 |
| FreshBlueprint-Checkpoint-2 | Win64 BuildPlugin 선컴파일 성공. 산출물을 새 빈 Blueprint 프로젝트에 복사해30/30 자동화와 BodySkin Cloth 새 프로세스 다시열기·240frame 검사 통과. 개발 체크포인트이며 완성품 배포 아님. 이 PC에는 컴파일러가 설치되어 있으므로 컴파일러 없는 환경 검증을 주장하지 않음 |
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
| FreshProjects-Checkpoint-3 | 최신0.3.0-dev의 Win64 BuildPlugin 선컴파일 및 새 C++ 호스트 Editor 빌드 성공. 새 빈 Blueprint/C++ 프로젝트에서 각각36/36 자동화와 저장 BodySkin Cloth 다시열기·312particles/240frame 검사 통과. 개발 체크포인트로 A–F 전체 통과나 컴파일러 없는 환경·cook·게임 패키지 검증을 의미하지 않음 |
| TShirt-Fit-11/collision-section-audit.json | UE 원본 삼각형과 충돌체의 X=0 단면 독립 비교. 높이7.8cm의 몸통/팔 틈은 복원되었지만8.0~8.2cm에는 실제 틈을 충돌체가 메움. 옷의 경로·길이 제약을 함께 조사 중이며 추가 임의 치수 조정으로 성공 처리하지 않음 |
| NeckLength-Red / NeckLength-Green | Editor 빌드 후36/37 예상 실패: 기존 뒤목 반쪽1.626187cm는10%변형과 면 관통.005cm를 허용해도 단일 충돌 구 우회 하한1.865905cm에 못 미침. 독립 검토 확인. 원본 cubic의 control polygon 길이 상한과 실제 Physics 끝 구의 최단 외부 경로 하한으로 필요조건 회귀 추가. 뒤목 깊이1.5→4mm만 바꾼 템플릿0.3.2-draft는 Editor 빌드 및37/37 통과. 전체 충돌체 합집합이나 착장 성공을 보장하는 검사는 아님 |
| TShirt-Fit-12 | 뒤목 수정만 적용한1200단계 비교. 봉제 간격2.22e-16cm·정점/면 관통0·내부 접힘70.93도로 개선. 하지만 최대 변 길이 오차342.1%·면적비0.289~8.972로 실패. 전체 구조 변의약29%가10%변형을 초과하며 겨드랑이와 목 고정점 주변에 집중됨. 렌더 직접 확인, 완성품으로 저장/배포하지 않음 |
| FreshProjects-Checkpoint-4 | 템플릿0.3.2-draft와 목길이 회귀를 포함한 Win64 BuildPlugin 및 새 C++ Editor 빌드 성공. 새 Blueprint/C++에서 각각37/37와 저장 Cloth 다시열기/312particles 통과. 뒤따른 진단 문서 재실행 경로는 이 체크포인트에 포함하지 않음 |
| DraftReplay-Build / DraftReplay-Build-2 | 개발용 source-draft.json 재실행 옵션 추가. 처음 사용한 JSON 변환 overload가 UE5.8에 없어 빌드 실패. 실제 엔진 헤더의 클래스 지원 template overload로 수정 후 Editor 빌드와37/37 통과. versioned .lgarment 읽기/쓰기 구현이나 관문 B 통과가 아님 |
| TShirt-Guide970-1 | 진단 원본에서 어깨 가이드4개의 높이만9.75→9.70cm 변경. UE 재직렬화 JSON과 입력 문서가 완전히 같은지 별도 확인.1200단계 후 봉제 간격은 수치 오차 수준이나 변 길이 오차340.7%로 실패. 기본 템플릿의 가이드 높이는 변경하지 않음 |
| TShirt-Armhole25-1 | 겨드랑이 경계점4개의 rest 높이만8mm 내린 사용자 편집 진단 원본.1200단계 후 봉제/관통0, 최대 변 길이 오차185.9%·면적비0.379~4.025로 실패. 실제 렌더 확인. 기본 템플릿의 암홀 치수는 유지함 |
| FreshProjects-Checkpoint-5 | REQ-017 전달 전에 시작한 마지막 반복 인수. 최신 진단 재실행 코드를 포함한 Win64 선컴파일·새 C++ Editor 빌드·각37/37·저장 Cloth 다시열기 통과. 이후 이 검증을 이번 범위의 반복 필수조건으로 사용하지 않음 |
| ConvexCollision-Red | UE5.8 native Cloth의 convex 지원을 로컬 엔진에서 확인하고 생성/연속접촉/초기 면관통 회귀 추가. Editor 빌드 성공, 기존37개 통과·미구현3개 예상 실패(37/40). 실제 바디 Physics는 아직22개 기존 capsule/sphere로 보존됨 |
| ConvexCollision-Green / Green-2 / Green-3 | factory friend의 DLL linkage 오류, ChaosCore 직접 링크 누락, GeometryCore face 추출용 neighbor 저장 누락을 순서대로 확인·수정. Green-3은 빌드 성공했으나 hull 생성 ensure로37/40. 통과로 취급하지 않음 |
| ConvexCollision-Green-4 | Editor 빌드 및40/40 통과. 불변 native FConvex와 병합 polygon face, Chaos 거리·둥근 두께 sweep, capsule/convex 합집합 투영, 초기/최종 정점·면 관통 검사 구현. 실제 바디 애셋 연동은 아직 없음 |
| ConvexNearSurface-Red / ConvexGrazingSurface-Red / ConvexNearSurface-Green | near-corner 외부 시작점에서 GJK의 보수적인 음수 진입값을 버려 반대편 외부로 관통하는 결함을 실제 UE에서 재현(각39/40). 해당 구간이 현재 이동을 포함하고 안쪽으로 이동하면 현재점 접촉으로 처리하여 Editor 빌드 및40/40 통과 |
| ConvexPrecision-Red / ConvexPrecision-Green | 중심Z=1000cm·반두께.00001cm의 double hull이 native float에서 평면으로 붕괴해도 승인되는 회귀39/40. float 양자화 좌표에서 hull을 먼저 계산하여 거부·이전 결과 보존 확인. 최신 Editor 빌드 및40/40(오류/경고0) 통과 |
| ConvexOutward-Red / ConvexOutward-Green / ConvexOutward-Green-2 | 보수적 음수 진입 구간에서 먼 진입면 법선을 사용해 바깥 이동을 막는 회귀39/40. 현재점의 거리 법선으로 수정. 첫 Green은 실제 접선 이동이 Y=0을 넘는 것을 관통으로 잘못 판정한 시험1개 실패. 양의 X면 유지·실제 외부 거리 기준으로 수정하고 결과좌표를 기록한 Green-2는 최신 Editor 빌드·40/40(오류/경고0) 통과 |
| CommitCheckpoint-Native-Reopen | 기존 CA_NativePanelProbe의 새 프로세스 다시열기 실패(exit9).312 particles/240frame runtime의 변위·관통은 허용 범위지만 savedGeometryMatchesSource=false여서 전체 실패. 해당 애셋을 최신 검증 완료 대상으로 커밋하지 않으며 삭제하지 않음 |
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
| ConvexVersion-Native-Save / Reopen | 오래된 CA_NativePanelProbe를 최신 생성기로 다시 저장하고 새 프로세스 다시열기 통과. 원본 형상/UV/skin/map 동일성,312particles/240frame,최종 변위.0411868cm·정점 관통0. 이전 CommitCheckpoint의 source 불일치 해소. 두 패널 하위 경로이며 대표 의상 인수가 아님 |
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

현재 제한: 대칭 평가/hem·binding은 미구현이며 문서 생성 시 명시적으로 거부한다. 천 자체충돌은 미구현이다. 봉제 순서는 목록에 보존하지만 물리적으로 단계별 수행하지는 않는다. 굽힘은 Chaos 각도 관례의 패널 내부 dihedral regularization이며 실제 원단의 보정된 물성이 아니다. 면-캡슐 거리·내부 접힘·면적 검사는 있으나 전역적인 자체 교차 검증은 없다. SurfaceVertex skin은 명시적 정점 샘플링으로 불연속 가능성이 있으며 실제 의상 포즈 검증을 대체하지 않는다. 엄격한 보간의4초과는 아직 수동 재분배 UI가 없어 작업을 중단한다. 수렴 플래그는 `.lgarment`의 validated 상태가 아니다.

실제 애셋은 플러그인 `Content/Development/`의 CA_NativePanelProbe, CA_BodySkinProbe, M_NativeCloth, PA_NativeProbe에 저장되어 있다. runtime 240 tick의 전체 프레임 변위·정점 관통·고정점 검사와 다시열기 원본 동일성 비교를 실행했다. 바뀐 최종 생성 결과로 다시 검증하며, 이전 결과를 최신 출력의 통과 근거로 혼용하지 않는다. 새 빈 Blueprint 프로젝트의 현재 하위 경로는 확인했으며 Designer, 새 C++ 프로젝트, cook/Development/Shipping은 아직 실행하지 않음.

검토: 별도 읽기 전용 Astra/High 검토에서 좌표 double→float 범위와 세분화 예산 P2를 발견했고 재현 후 수정했다. 가변 FBuildResult의 구조 방어와 저장 후 동일성·전체 runtime frame 검증 공백도 추적한다. 검토자가 빌드/UE 실행을 수행한 것은 아니다.

바디 준비: 기존 절차형 더미의 FBX/흰 텍스처/치수/UV 자료를 `Resources/ReferenceBody/`에 고정 복사. 원본 전후 및 사본 SHA-256 일치. provenance.json에 출처와 아직 제공되지 않은 배포 라이선스 검토 상태를 기록했다. 이후 UE 독립 import/UV/skin 검증을 위 실행에서 수행했다. 복사된 원본 UV 보고서와 독립 검사 결과를 구분한다. Blender 실행이나 제품의 외부 저작툴 의존성 추가 없음. Python UV audit는 개발용 검증 도구이며 제품 저작 경로에 포함되지 않는다.
