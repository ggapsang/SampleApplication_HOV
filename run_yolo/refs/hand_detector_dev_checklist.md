# Hand Detection App — 개발 체크리스트

> - 참조 스펙: `hand_detector_outsource_spec.yaml`
> - 참조 샘플: `run_neural_network` (Classification 클래스)
> - 타겟: PNV-A9082RZ (WN9), SDK 25.04.09, C++17

---

## Phase 1: 프로젝트 스캐폴딩 ✅

- [x] 프로젝트 생성, 컴포넌트 리네이밍 (HandDetector)
- [x] 매니페스트 작성 (ReceiverNames, SourceNames, LCM)
- [x] Attribute JSON (confidence_threshold, nms, alarm 등)
- [x] CMakeLists.txt, 빌드/배포 검증

---

## Phase 2: NPU 모델 로딩 ✅

- [x] `network_binary.nb` 배치 (`app/res/ai_bin/`)
- [x] NeuralNetwork 생성 → CreateInputTensor("images") → CreateOutputTensor("output0") → LoadNetwork
- [x] DebugLog 유틸리티
- [x] 텐서 shape 로그: input=[640,640,3], output=[8400,5,1]

---

## Phase 3: Raw Video 수신 및 PreProcess ✅

- [x] eVideoRawData 이벤트 핸들러
- [x] 멀티 이미지 프레임 선택, Tensor::Allocate
- [x] skip_frames 로직
- [x] SDK Tensor::Resize (640x640 letterbox)
- [x] letterbox 좌표 보정: scale=3.0, pad_y=140 (수정 완료)

---

## Phase 4: 추론 실행 및 PostProcess ✅

- [x] RunNetwork + 추론 시간 측정 (~26ms)
- [x] 출력 텐서 [8400,5,1] AoS 파싱
- [x] bbox dequantize: (raw+128)*2.4988
- [x] conf sigmoid: 1/(1+exp(-logit))
- [x] NMS + 좌표 역변환 + 클램핑
- [x] Inference 파이프라인: PreProcess → Execute → PostProcess

---

## Phase 5: MQTT 로깅 모듈 ✅

- [x] POSIX 소켓 기반 최소 MQTT 3.1.1 클라이언트 직접 구현 (외부 라이브러리 없음)
  - [x] `mqtt_logger.h` / `mqtt_logger.cc`
  - [x] CONNECT + PUBLISH (QoS 0) + DISCONNECT
  - [x] 연결 실패 시 추론 블로킹 안 함
- [x] 브로커 IP 하드코딩: `192.168.9.199:1883`
- [x] Initialize()에서 자동 연결
- [x] 토픽 발행:
  - [x] `hand/status` — Start/Stop 시 상태
  - [x] `hand/detection` — 매 프레임 감지 수 + confidence
  - [x] `hand/debug` — 10프레임마다 타이밍 (pre/inf/post ms)
- [ ] `hand/alarm` — 알람 ON/OFF (Phase 7에서 추가)
- [x] PC에서 mosquitto 브로커 + mosquitto_sub 수신 확인
- 참조: `refs/MQTT_Setup_Guide.md`

---

## Phase 6: ONVIF 메타데이터 및 OSD — 부분 완료

### 완료된 것:
- [x] MetaFrame Schema 등록 (SetMetaFrameSchema, SetMetaFrameCapabilitySchema)
- [x] Frame Metadata 전송 (SendFrameMetadata → eRequestMetadata)
- [x] MetadataWriter XML 전송 (eRequestRawMetadata)
- [x] OSD 오버레이 (SendOsd → eExternalMultiLineOsd)
  - [x] "Hand: N | Conf: XX%" 좌상단 표시
- [x] 타임스탬프: chrono::system_clock (Unix ms)

### 미완료 (OpenEventDispatcher 매핑 문제로 보류):
- [ ] 6-1. Metadata Schema 등록 (OpenEventDispatcher → eMetadataSchema)
- [ ] 6-2. EventStatus Schema 등록 (OpenEventDispatcher → eEventstatusSchema)
- [ ] 6-3. Event Metadata 전송 + NeedBroadcasting(true)
  - 코드는 작성됨 (SendMetadataSchema, SendEventStatusSchema, NotifyEventMetadata)
  - ReceiverNames에 OpenEventDispatcher 추가 시 앱 크래시 (500 에러)
  - 원인 미해결 → 비활성화 상태

### 웹뷰어 바운딩 박스 표시:
- [x] FrameMetadata + MetadataWriter XML로 전송 중이나 웹뷰어에 박스 미표시
- [x] 대안: 웹 UI(index.html)에서 Canvas 기반 라이브 감지 뷰 구현 (Phase 8)

---

## Phase 7: 알람 출력 — ❌ 미구현

- [ ] 알람 상태 변수 (consecutive_detect/miss_count, alarm_active)
- [ ] 알람 트리거 로직 (on_threshold, off_threshold, cooldown)
- [ ] ConfigurableAlarmOut 이벤트 전송 (RelayInstance)
- [ ] 이벤트 로그 기록 (WriteEventLog)
- [ ] MQTT alarm 토픽 발행

---

## Phase 8: 웹 UI — 대부분 완료

### 완료된 것:
- [x] HTML 페이지 (index.html)
  - [x] Start/Stop/Info/Test 버튼
  - [x] 디버그 로그 패널 (mode=log, auto-refresh)
  - [x] 상태 바 (state, model, confidence, nms)
- [x] HTTP 핸들러 (HandleStreamRequest)
  - [x] mode=start/stop/info/config/log/clear/test
  - [x] mode=capture (스틸샷 프레임 + 감지 결과)
  - [x] mode=detections (감지 결과 JSON)
  - [x] mode=snapshot (raw 프레임)
- [x] 실시간 감지 뷰
  - [x] Canvas 기반 프레임 표시 (input_tensor planar RGB 캡처)
  - [x] 바운딩 박스 + confidence 오버레이
  - [x] 스틸샷 캡처 버튼
  - [x] 라이브 폴링 (200ms 간격)

### 미완료:
- [ ] 설정 UI (confidence/nms 슬라이더, alarm threshold 입력)
- [ ] Config 변경 시 실시간 반영 확인

---

## Phase 9: 통합 테스트 — ❌ 미시작

- [ ] 카메라 재부팅 후 자동 시작
- [ ] 다양한 조건 탐지 정확도
- [ ] FPS 측정 (목표: 15 FPS 이상, 현재: ~30 FPS)
- [ ] 24시간 안정성 테스트
- [ ] MQTT 브로커 장애 시 추론 계속 동작

---

## Phase 10: 패키징 및 납품 — ❌ 미시작

- [ ] 디버그 로그 레벨 조정
- [ ] `hand_detector.cap` 최종 패키징
- [ ] 테스트 결과 보고서

---

## 알려진 이슈

1. **모델 오검출**: 빈 화면에서 Hand 100% 오검출 지속 → 모델 재컴파일 또는 threshold 추가 조정 필요
2. **OpenEventDispatcher**: ReceiverNames 추가 시 앱 크래시 → 매핑 방법 조사 필요
3. **rgb->VirtAddr()=NULL**: 원본 프레임 텐서는 CPU 접근 불가 → input_tensor(planar RGB)에서 캡처로 우회
4. **input_tensor stride**: 물리 stride = frame_width*3 (planar 형식) → 올바른 plane 분리 읽기로 해결

---

*— 마지막 업데이트: 2026-03-24 —*
