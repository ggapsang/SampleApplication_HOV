# Hand Detection App — 개발 체크리스트

> 외주 개발자용 상세 작업 목록
> 반드시 순서대로 진행할 것. 각 단계 완료 후 체크 표시.
>
> - 참조 스펙: `hand_detector_outsource_spec.yaml`
> - 참조 샘플: `run_neural_network` (Classification 클래스)
> - 타겟: PNV-A9082RZ (WN9), SDK 25.04.09, C++17

---

## Phase 1: 프로젝트 스캐폴딩

### 1-1. 프로젝트 생성

- [x] `opensdk_new_project -n hand_detector -v 1.0 -c wn9 -s 25.04.09` 실행
- [x] 생성된 디렉토리 구조 확인 (`app/`, `config/`, `docker-compose.yml`)

### 1-2. 컴포넌트 리네이밍

- [x] `SampleComponent` → `HandDetector` 클래스명 변경
  - [x] 헤더 파일명: `classification.h` 덮어씀 (HandDetector 클래스)
  - [x] 소스 파일명: `classification.cc` 덮어씀 (HandDetector 구현)
  - [x] ClassID 상수명: `kComponentId` 유지 (동일 값)
  - [x] 클래스 선언: `class HandDetector : public Component`
  - [x] 생성자에서 컴포넌트 이름 문자열 `"HandDetector"` 로 변경
- [x] `extern "C"` 팩토리 함수에서 `HandDetector` 인스턴스 반환 확인
- [x] 라이브러리 출력명: `libhand_detector.so` (CMakeLists TARGET_LIB)
- [x] 소스 파일 디렉토리: `app/src/classification/` (classification 폴더 재활용)

### 1-3. 매니페스트 작성

- [x] `Classification_manifest_instance_0.json` 덮어씀 (HandDetector 인스턴스)
  ```
  LibraryFileName: "libhand_detector.so"
  InstanceName: "HandDetector"
  SchedulerName: "HandDetectorScheduler"
  ```
- [x] ReceiverNames 등록:
  - [x] `AppDispatcher` — HTTP 요청 수신
  - [x] `SPMgrVideoRaw_0` — Raw Video 수신
  - [x] `OpenPlatform` — 웹 UI 이벤트 수신
  - [x] `AppNetworkManager` — 네트워크 이벤트 수신 (MQTT 응답)
  - [x] `ConfigurableAlarmOut` — 알람 이벤트 수신
- [x] SourceNames 등록:
  - [x] `{Source: "SPMgrVideoRaw_${APPCHANNEL}", GroupName: "GroupSPMgrVideoRaw2"}` — Raw Video 그룹
  - [x] `{Source: "OpenPlatform", GroupName: "AppMessage"}` — 웹 UI 메시지
  - [x] `{Source: "OpenPlatform", GroupName: "SettingChange"}` — 설정 변경
- [x] Channel: `${APPCHANNEL}`
- [x] SettingPath: `"../storage/settings/"`
- [x] `PLifeCycleManagermanifest.json`에 HandDetector 컴포넌트 등록 확인

### 1-4. Attribute JSON 작성

- [ ] `storage/settings/` 경로에 기본 attribute 파일 생성
  - [ ] `confidence_threshold`: 0.5
  - [ ] `nms_iou_threshold`: 0.45
  - [ ] `skip_frames`: 0
  - [ ] `mqtt_broker_host`: ""
  - [ ] `mqtt_broker_port`: 1883
  - [ ] `alarm_on_threshold`: 5
  - [ ] `alarm_off_threshold`: 30
  - [ ] `alarm_cooldown_sec`: 10
- [ ] Initialize()에서 `PrepareAttributes()` 호출하여 attribute 로드 확인

### 1-5. CMakeLists.txt 설정

- [ ] `app/src/hand_detector/CMakeLists.txt` 작성
  - [ ] `add_library(hand_detector MODULE hand_detector.cc yolo_postprocess.cc mqtt_logger.cc)`
  - [ ] include 디렉토리: `includes/`
  - [ ] 매니페스트 install 규칙 (manifest.json, instance.json, attribute.json)
- [ ] `app/src/CMakeLists.txt`에 `add_subdirectory(hand_detector)` 추가
- [ ] LCM, AppDispatcher 복사 규칙 확인

### 1-6. 빌드 및 배포 검증

- [ ] `docker-compose.yml`에서 SOC=wn9 설정 확인
- [ ] 빌드 실행: `cmake -DSOC=wn9 .. && make clean && make install -j4`
- [ ] 패키징: `opensdk_packager` → `hand_detector.cap` 생성 확인
- [ ] 카메라 설치: 웹 뷰어 또는 `opensdk_install` CLI
- [ ] 카메라 로그에서 `HandDetector Initialize` / `Start` 메시지 확인
- [ ] 웹 뷰어 > 오픈 플랫폼 메뉴에서 앱 표시 확인

**Phase 1 완료 기준**: 빈 HandDetector 컴포넌트가 카메라에서 정상 기동

---

## Phase 2: NPU 모델 로딩

### 2-1. 모델 파일 배치

- [x] `hand_yolov11n.nb` 파일을 `app/res/ai_bin/` 경로에 배치
- [ ] 빌드 시 res 디렉토리가 패키지에 포함되는지 확인

### 2-2. NeuralNetwork 관련 멤버 변수 선언 (hand_detector.h)

- [ ] `NeuralNeworkMap nn_map_` (네트워크 객체 맵)
- [ ] `NnLoadInfo` 구조체 (model_name, input_tensor_names, output_tensor_names)
- [ ] `bool run_flag` (추론 시작/중지 플래그)
- [ ] `uint64_t raw_pts` (프레임 PTS)
- [ ] 헬퍼 함수 선언:
  - [ ] `NeuralNetwork* GetOrCreateNetwork(const std::string& name)`
  - [ ] `NeuralNetwork* GetNetwork(const std::string& name)`
  - [ ] `void RemoveNetwork(const std::string& name)`

### 2-3. NPU 초기화 시퀀스 구현 (Initialize 또는 Start에서 호출)

아래 순서 엄수:

- [ ] **Step 1 — CreateNetwork**
  - [ ] `GetOrCreateNetwork("hand_yolov11n.nb")` 호출
  - [ ] NeuralNetwork 객체 생성 확인
- [ ] **Step 2 — CreateInputTensor**
  - [ ] 텐서 이름: `"images"` (Netron 확인값)
  - [ ] `network->CreateInputTensor("images")` 호출
  - [ ] 반환값 nullptr 체크
- [ ] **Step 3 — CreateOutputTensor**
  - [ ] 텐서 이름: YAML 스펙의 `output_tensors[0].name` 참조 (PM 제공)
  - [ ] `network->CreateOutputTensor(output_name)` 호출
  - [ ] 출력 텐서가 복수 헤드인 경우 각각 호출
- [ ] **Step 4 — LoadNetwork**
  - [ ] 모델 경로: `"../res/ai_bin/hand_yolov11n.nb"`
  - [ ] mean: `{0.0, 0.0, 0.0}` (add_preproc_node: true 기준)
  - [ ] scale: `{1.0, 1.0, 1.0}` (add_preproc_node: true 기준)
  - [ ] `network->LoadNetwork(path, mean, scale)` 호출
  - [ ] 반환값 false 시 에러 로그 출력 후 return

### 2-4. DebugLog 유틸리티 구현

- [ ] Classification 샘플의 `DebugLog()` 패턴 참조
- [ ] `SendTargetEvents(ILogManager::remote_debug_message_group, eRemoteDebugMessage, ...)` 호출
- [ ] `std::cout`에도 동시 출력 (카메라 콘솔 디버깅용)

### 2-5. 빌드 및 검증

- [ ] 빌드 성공 확인
- [ ] 카메라 설치 후 로그에서 `"Load Network"` 성공 메시지 확인
- [ ] 에러 발생 시 텐서 이름/모델 경로 재확인

**Phase 2 완료 기준**: NPU에 모델 정상 로드, LoadNetwork 성공 로그

---

## Phase 3: Raw Video 수신 및 PreProcess

### 3-1. Raw Video 이벤트 수신 구현

- [ ] `ProcessAEvent()`에 이벤트 핸들러 추가
  ```
  case (int32_t)IPStreamProviderManagerVideoRaw::EEventType::eVideoRawData:
  ```
- [ ] 이벤트에서 `IPVideoFrameRaw` 추출
- [ ] `shared_ptr<RawImage>` 획득
- [ ] `run_flag == false`이면 즉시 return (추론 미시작 상태)

### 3-2. 프레임 선택 로직

- [ ] 멀티 이미지 프레임 처리 (Classification 샘플 Inference() 참조)
  - [ ] `images_cnt > 1`인 경우: 3840x2160 미만 && MAX_SIZE 이하 프레임 선택
  - [ ] 단일 이미지: 그대로 사용
- [ ] `Tensor::Create()` → `rgb->Allocate(*image)` 호출
- [ ] Allocate 실패 시 return

### 3-3. skip_frames 로직 (선택)

- [ ] 프레임 카운터 변수 선언
- [ ] `frame_count % (skip_frames + 1) != 0`이면 추론 건너뛰기
- [ ] attribute에서 `skip_frames` 값 로드

### 3-4. PreProcess 구현

**방식 A — SDK Tensor::Resize (기본)**:
- [ ] `network->GetInputTensor(0)` 에서 입력 텐서 크기 획득
- [ ] `img_size_t size = { .width = tensor->Length(0), .height = tensor->Length(1) }`
- [ ] `rgb->Resize(*input_tensor, size)` 호출

**방식 B — OpenCV letterbox (권장, 정확도 향상)**:
- [ ] OpenCV 라이브러리 링크 (CMakeLists.txt에 추가, 또는 SDK 내장 OpenCV 확인)
- [ ] RawImage → cv::Mat 변환 (YUV→RGB 또는 직접 RGB 사용)
- [ ] cv::resize로 비율 유지 리사이즈
- [ ] cv::copyMakeBorder로 패딩 (pad_value=114)
- [ ] scale_x, scale_y, pad_x, pad_y 값 저장 (PostProcess 좌표 역변환용)
- [ ] 결과를 입력 텐서 메모리에 복사

### 3-5. 빌드 및 검증

- [ ] 빌드 성공 확인
- [ ] DebugLog로 "PreProcess completed" + 프레임 해상도 출력 확인
- [ ] 프레임 수신 주기(FPS) DebugLog로 확인

**Phase 3 완료 기준**: Raw Video 프레임 수신 및 PreProcess 정상 동작 로그

---

## Phase 4: 추론 실행 및 PostProcess

### 4-1. 추론 실행

- [ ] `stat_t stat = {0,}` 선언
- [ ] `network->RunNetwork(stat)` 호출
- [ ] 반환값 체크 (false 시 에러 로그)
- [ ] stat에서 추론 시간(ms) 추출 → 디버그 로그

### 4-2. 출력 텐서 파싱

- [ ] `network->GetOutputTensorCount()`로 출력 텐서 수 확인
- [ ] 각 출력 텐서에 대해:
  - [ ] `network->GetOutputTensor(k)` 획득
  - [ ] `output_tensor->VirtAddr()`로 float* 데이터 포인터 획득
  - [ ] `output_tensor->Length(0)`, `Length(1)` 등으로 shape 확인

### 4-3. YOLOv11 디코딩 로직 (yolo_postprocess.h / .cc)

- [ ] 결과 구조체 정의:
  ```cpp
  struct Detection {
      float x1, y1, x2, y2;  // 원본 해상도 기준 bbox
      float confidence;
      int class_id;
  };
  ```
- [ ] 출력 텐서 shape 기반 디코딩 (YAML 스펙 참조):
  - [ ] shape `[1, 5, 8400]` 기준: dim1의 row0~3 = cx,cy,w,h / row4 = confidence
  - [ ] 전체 8400개 후보 순회
- [ ] confidence_threshold 이하 필터링
- [ ] center_xywh → xyxy 좌표 변환
- [ ] NMS 구현:
  - [ ] confidence 기준 내림차순 정렬
  - [ ] IoU 계산 함수 구현
  - [ ] iou_threshold 초과하는 중복 박스 제거
- [ ] 좌표 역변환 (모델 입력 해상도 → 원본 프레임 해상도):
  - [ ] letterbox 패딩 오프셋 제거 (pad_x, pad_y)
  - [ ] 리사이즈 비율 역산 (scale_x, scale_y)
  - [ ] 원본 해상도 범위 클램핑 (0 ~ width, 0 ~ height)
- [ ] max_detections(100) 초과 시 상위 N개만 유지

### 4-4. 추론 파이프라인 통합

- [ ] Inference() 메서드에서 순서 통합:
  ```
  PreProcess() → Execute() → PostProcess()
  ```
- [ ] 실패 시 DebugLog + break
- [ ] 각 단계별 소요 시간 측정 (std::chrono)

### 4-5. 빌드 및 검증

- [ ] 빌드 성공 확인
- [ ] DebugLog로 탐지 결과 출력: bbox 좌표, confidence, 탐지 수
- [ ] 손 객체 앞에서 탐지 수 변화 확인
- [ ] 빈 프레임(손 없음) 시 탐지 수 0 확인

**Phase 4 완료 기준**: 손 탐지 결과가 DebugLog에 정상 출력

---

## Phase 5: MQTT 로깅 모듈

### 5-1. 라이브러리 준비

- [ ] Eclipse Paho MQTT C 라이브러리 ARM64 크로스컴파일
- [ ] 빌드된 .so 파일 `app/libs/` 경로에 배치
- [ ] CMakeLists.txt에 링크 추가
- [ ] 크로스컴파일 실패 시 → 대안: Network Manager TCP 방식으로 전환

### 5-2. MqttLogger 클래스 구현 (mqtt_logger.h / .cc)

- [ ] 멤버 변수:
  - [ ] `MQTTClient client` (Paho)
  - [ ] `std::string broker_host`, `int broker_port`
  - [ ] `std::string client_id`
  - [ ] `bool connected`
- [ ] `bool Connect(const std::string& host, int port)`
  - [ ] 연결 실패 시 exponential backoff 재시도 (최대 5회)
  - [ ] 비동기 처리 — 연결 실패가 추론 블로킹하지 않도록
- [ ] `void Disconnect()`
- [ ] `void Publish(const std::string& topic, const std::string& payload)`
  - [ ] QoS 0 (fire-and-forget)
  - [ ] 전송 실패 시 로그만 출력, 예외 미발생

### 5-3. JSON 직렬화 (rapidjson 사용)

- [ ] detection 메시지 생성 함수:
  - [ ] frame_id, timestamp, detections[] (bbox, conf, class)
- [ ] debug 메시지 생성 함수:
  - [ ] preprocess_ms, inference_ms, postprocess_ms, total_fps
- [ ] status 메시지 생성 함수:
  - [ ] state, model, uptime_sec, total_detections
- [ ] alarm 메시지 생성 함수:
  - [ ] event(triggered/released), trigger_count, max_confidence, timestamp

### 5-4. 토픽 발행 통합

- [ ] PostProcess 완료 후 detection 토픽 발행
- [ ] 매 프레임(또는 N프레임마다) debug 토픽 발행
- [ ] Initialize/Finalize 시 status 토픽 발행
- [ ] 알람 트리거/해제 시 alarm 토픽 발행

### 5-5. Initialize / Finalize 연동

- [ ] Initialize()에서 attribute 로드 → MqttLogger.Connect() 호출
- [ ] Finalize()에서 MqttLogger.Disconnect() 호출

### 5-6. 빌드 및 검증

- [ ] 빌드 성공 확인
- [ ] 외부 PC에서 MQTT 브로커(mosquitto 등) 실행
- [ ] mosquitto_sub로 각 토픽 구독 → 메시지 수신 확인
- [ ] JSON 파싱 정상 여부 확인
- [ ] 브로커 미연결 시에도 추론 정상 동작 확인

**Phase 5 완료 기준**: 외부 MQTT 브로커에서 detection/debug/status 메시지 수신

---

## Phase 6: ONVIF 메타데이터 및 OSD

### 6-1. Metadata Schema 등록

- [ ] ProcessAEvent()에서 `eMetadataSchema` 이벤트 핸들러 추가
- [ ] SendMetadataSchema() 구현:
  - [ ] EventName: `"OpenSDK.hand_detector.HandDetection"`
  - [ ] EventTopic: `"tns1:OpenApp/hand_detector/HandDetection"`
  - [ ] EventSchema: ONVIF XML (State, DetectionCount, MaxConfidence 필드)
  - [ ] PROPRIETARY + ONVIF 양쪽 모두 등록
- [ ] `SendNoReplyEvent("OpenEventDispatcher", eMetadataSchema, ...)` 호출

### 6-2. EventStatus Schema 등록

- [ ] `eEventStatusSchema` 이벤트 핸들러 추가
- [ ] 이벤트 상태 스키마 정의 (State: boolean)
- [ ] `SendNoReplyEvent("OpenEventDispatcher", eEventStatusSchema, ...)` 호출

### 6-3. Event Metadata 전송

- [ ] StreamMetadata 객체 생성 (channel=0, timestamp)
- [ ] EventMetadataItem 구성:
  - [ ] topic: `"tns1:VideoSource/tnssamsung:hand_detector"`
  - [ ] source: `{"VideoSourceToken", "VideoSourceToken-0"}`
  - [ ] data: `{"State", "true"/"false"}`
  - [ ] element: 탐지 객체 정보 (bbox, confidence)
- [ ] `NeedBroadcasting(true)` 설정
- [ ] `SendMetadata(std::move(metadata))` 호출
- [ ] 전송 간격: 최소 30ms 이상 보장

### 6-4. Frame Metadata 전송 (바운딩 박스)

- [ ] FrameMetadataItem 구성:
  - [ ] timestamp, resolution (원본 프레임)
  - [ ] ObjectItem: 각 탐지 결과의 bbox 좌표
- [ ] StreamMetadata에 frame_metadata 추가
- [ ] MetadataManager로 전송

### 6-5. OSD 오버레이

- [ ] MultilineOsdItem 구성:
  - [ ] Enable: true
  - [ ] Index: 101 (고유 인덱스)
  - [ ] OSDType: "Title"
  - [ ] OSD: `"Hand: {count} | FPS: {fps} | Conf: {max_conf:.2f}"`
  - [ ] FontSize: "Small", OSDColor: "White"
  - [ ] PositionX/Y: 좌상단
- [ ] `SendNoReplyEvent("SRMgrVideo", eExternalMultiLineOsd, ...)` 호출
- [ ] 탐지 없을 시 `"Hand: 0 | FPS: {fps}"` 표시

### 6-6. 빌드 및 검증

- [ ] 빌드 성공 확인
- [ ] 카메라 CGI로 메타데이터 스키마 등록 확인:
  `http://<IP>/stw-cgi/eventstatus.cgi?msubmenu=metadataschema&action=view`
- [ ] VMS 연결 시 바운딩 박스 표시 확인
- [ ] 카메라 라이브 뷰에서 OSD 텍스트 표시 확인

**Phase 6 완료 기준**: VMS 바운딩 박스 + 카메라 OSD 정상 표시

---

## Phase 7: 알람 출력

### 7-1. 알람 상태 관리 변수

- [ ] `int consecutive_detect_count` — 연속 탐지 프레임 수
- [ ] `int consecutive_miss_count` — 연속 미탐지 프레임 수
- [ ] `bool alarm_active` — 현재 알람 상태
- [ ] `std::chrono::steady_clock::time_point last_alarm_off_time` — 마지막 알람 해제 시각

### 7-2. 알람 트리거 로직

- [ ] 매 프레임 PostProcess 후 호출
- [ ] 탐지 있음:
  - [ ] `consecutive_detect_count++`, `consecutive_miss_count = 0`
  - [ ] `consecutive_detect_count >= alarm_on_threshold` && `!alarm_active` && 쿨다운 경과:
    - [ ] 알람 ON 실행
    - [ ] `alarm_active = true`
- [ ] 탐지 없음:
  - [ ] `consecutive_miss_count++`, `consecutive_detect_count = 0`
  - [ ] `consecutive_miss_count >= alarm_off_threshold` && `alarm_active`:
    - [ ] 알람 OFF 실행
    - [ ] `alarm_active = false`
    - [ ] `last_alarm_off_time` 갱신

### 7-3. 알람 ON/OFF 이벤트 전송

- [ ] 알람 ON:
  ```
  RelayInstance::Builder → SetAction("on") → SetChannel(0) → build()
  SendNoReplyEvent("ConfigurableAlarmOut", eRelayRequest, ...)
  ```
- [ ] 알람 OFF:
  ```
  RelayInstance::Builder → SetAction("off") → SetChannel(0) → build()
  SendNoReplyEvent("ConfigurableAlarmOut", eRelayRequest, ...)
  ```

### 7-4. 이벤트 로그 기록

- [ ] 알람 ON 시:
  ```
  Log(EVENT_LOG, EVENT_OPENAPP, 0, time(NULL), "HandDetection alarm triggered")
  SendNoReplyEvent("LogManager", eWrite, ...)
  ```
- [ ] 알람 OFF 시 동일 패턴으로 기록

### 7-5. MQTT alarm 토픽 발행

- [ ] 알람 ON 시 `cam/{cam_id}/hand/alarm` → `{"event": "triggered", ...}` 발행
- [ ] 알람 OFF 시 `cam/{cam_id}/hand/alarm` → `{"event": "released", ...}` 발행

### 7-6. 빌드 및 검증

- [ ] 빌드 성공 확인
- [ ] 손을 카메라 앞에 5프레임 이상 유지 → 알람 ON 확인
- [ ] 손 제거 후 30프레임 경과 → 알람 OFF 확인
- [ ] 쿨다운 시간 내 재트리거 방지 확인
- [ ] 카메라 이벤트 로그에 기록 확인

**Phase 7 완료 기준**: 알람 ON/OFF 정상 동작 + 이벤트 로그 기록

---

## Phase 8: 웹 UI

### 8-1. HTML 페이지 작성 (app/html/index.html)

- [ ] Start 버튼 → `/stream?mode=start` 요청
- [ ] Stop 버튼 → `/stream?mode=stop` 요청
- [ ] Info 버튼 → `/stream?mode=info` 요청 → 응답 표시
- [ ] Config 폼 → `/stream?mode=config` + JSON body 전송

### 8-2. HTTP 요청 핸들러 구현

- [ ] ProcessAEvent()에서 웹 UI 요청 이벤트 처리 (HandleRequest)
- [ ] 요청 JSON 파싱 (mode 필드 분기):
  - [ ] `start`: `run_flag = true` → RunNetwork 시작
  - [ ] `stop`: `run_flag = false` → 추론 중지
  - [ ] `info`: 현재 상태 JSON 응답 (fps, model_name, detection_count, alarm_state, mqtt_status)
  - [ ] `config`: attribute 값 업데이트 → WriteAttributes() 호출

### 8-3. 설정 페이지 UI 요소

- [ ] confidence_threshold 슬라이더 (0.1~1.0)
- [ ] nms_iou_threshold 슬라이더 (0.1~1.0)
- [ ] skip_frames 입력 (0~30)
- [ ] MQTT broker host/port 입력
- [ ] alarm_on_threshold 입력
- [ ] alarm_cooldown_sec 입력
- [ ] 현재 상태 실시간 표시 영역 (FPS, 탐지 수, 알람 상태)

### 8-4. 빌드 및 검증

- [ ] 빌드 성공 확인
- [ ] 카메라 웹 뷰어 > 오픈 플랫폼 > 앱 페이지에서 UI 표시 확인
- [ ] Start/Stop 동작 확인
- [ ] Config 변경 후 반영 확인 (confidence 변경 → 탐지 수 변화)
- [ ] Info 조회 정상 응답 확인

**Phase 8 완료 기준**: 웹 UI에서 전체 기능 제어 가능

---

## Phase 9: 통합 테스트

### 9-1. 기능 테스트

- [ ] 카메라 재부팅 후 앱 자동 시작 + 모델 자동 로드 확인
- [ ] 다양한 거리(0.5m, 1m, 3m)에서 손 탐지 정확도 확인
- [ ] 다양한 조명(실내, 역광, 저조도)에서 탐지 확인
- [ ] 여러 손 동시 탐지 (2~5개) 정상 동작 확인
- [ ] ONVIF 메타데이터 → VMS 바운딩 박스 좌표 정확성 확인
- [ ] OSD 텍스트 정상 갱신 확인
- [ ] 알람 ON/OFF 시나리오 전체 검증
- [ ] MQTT 전체 토픽 메시지 수신 확인
- [ ] 웹 UI Start/Stop/Config/Info 전체 동작 확인

### 9-2. 성능 테스트

- [ ] FPS 측정: MQTT debug 토픽 또는 DebugLog
- [ ] 합격 기준: 15 FPS 이상
- [ ] 추론 시간 측정: preprocess_ms + inference_ms + postprocess_ms
- [ ] 메모리 사용량 모니터링 (카메라 시스템 리소스)

### 9-3. 안정성 테스트

- [ ] 24시간 연속 운영 → 메모리 누수/크래시 없음 확인
- [ ] MQTT 브로커 강제 종료 → 추론 중단 없이 계속 동작 확인
- [ ] MQTT 브로커 재시작 → 자동 재연결 확인
- [ ] 네트워크 케이블 분리/재연결 → 정상 복구 확인
- [ ] 카메라 재부팅 10회 반복 → 매번 정상 기동 확인

### 9-4. 성능 최적화 (필요 시)

- [ ] Tensor 객체 매 프레임 재생성 → 풀링/재사용으로 변경
- [ ] skip_frames 값 조정으로 FPS/부하 최적화
- [ ] 메타데이터 전송: 변화 없을 시 생략 로직 추가
- [ ] MQTT 전송 주기 조절 (매 프레임 → N프레임마다)

**Phase 9 완료 기준**: 전체 기능/성능/안정성 합격

---

## Phase 10: 패키징 및 납품

- [ ] 최종 빌드: 디버그 로그 레벨 조정 (불필요한 stdout 출력 제거 또는 조건부)
- [ ] `opensdk_packager`로 최종 `hand_detector.cap` 생성
- [ ] 서명 인증서 적용 확인
- [ ] 납품물 목록:
  - [ ] `hand_detector.cap` (설치 패키지)
  - [ ] 소스 코드 전체 (GitHub 레포 또는 아카이브)
  - [ ] 빌드 가이드 (환경 구성, 빌드 명령어)
  - [ ] 테스트 결과 보고서 (Phase 9 체크리스트 기반)
- [ ] PM 검수 요청

---

*— 문서 끝 —*
