# REQ-026 BodyPins 고정 마네킹 사본

- 원본 저장소 커밋: `69583eb`.
- 원본 경로: `Content/GarmentFitLab/Source/SKM_LunaMk2_Mannequin.fbx`.
- Git blob: `8d7916124c01666172a92bbb4b424dfbaa9d0237`.
- 크기: 2,837,596 bytes.
- SHA-256: `a72877543b69a3b035a6da373394a0f762df6847f589ae4e147c03515edfc956`.
- 추출 시각: `2026-10-16T17:42:06.1148485Z` (UTC).
- 독립 사본: `PluginDev/GarmentFitClothProbeHost/Plugins/GarmentFitClothProbe/Content/BodyPins/Source/SKM_LunaMk2_Mannequin_69583eb.fbx`.

Git의 고정 blob을 바이너리 스트림으로 추출했다. 추출 뒤 `git hash-object`가 원본 blob과 일치했으며 변경 중인 작업 트리 FBX/uasset을 읽거나 복사하지 않았다. Blender를 이용한 재구성도 하지 않았다.

실험 임포트 대상은 `/GarmentFitClothProbe/BodyPins/SM_LunaBindPose`다. FBX를 독립 정적 바인드 포즈 메시로 임포트하고 실험 전용 무광 재질을 사용한다. 이 선택은 강체 바디 이동·회전 추종을 검증하기 위한 것이며 골격 변형/포즈 추종을 지원한다고 주장하지 않는다. 실제 임포트·저장·새 프로세스 재열기 결과는 조정자의 실행 후 별도 결과 문서에 기록한다.

메인 `/Game/GarmentFitLab/` 애셋이나 원본 골격·재질에 대한 실행 의존성을 만들지 않는다. 라이선스 또는 배포 이용조건은 이 문서에서 추가하거나 확정하지 않는다.
