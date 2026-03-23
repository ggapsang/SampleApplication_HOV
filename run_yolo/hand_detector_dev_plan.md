# Hand Detection AI App — 상세 개발 계획서

> 한화비전 OpenSDK 써드파티 애플리케이션

| 항목 | 내용 |
|------|------|
| **타겟 카메라** | PNV-A9082RZ (WN9) |
| **모델** | YOLOv11 (.nb) |
| **언어** | C++17 |
| **SDK 버전** | 25.04.09 |
| **문서 버전** | v1.0 |
| **작성일** | 2026-03-23 |

---

## 목차

1. [프로젝트 개요](#1-프로젝트-개요)
2. [시스템 아키텍처](#2-시스템-아키텍처)
3. [프로젝트 디렉토리 구조](#3-프로젝트-디렉토리-구조)
4. [상세 개발 계획](#4-상세-개발-계획)
5. [개발 순서 요약](#5-개발-순서-요약)
6. [리스크 및 미해결 사항](#6-리스크-및-미해결-사항)
7. [외주 개발 위탁 시 전달 사항](#7-외주-개발-위탁-시-전달-사항)

---

## 1. 프로젝트 개요

### 1.1 프로젝트 목적

한화비전 WisenetPlatform OpenSDK 기반 PNV-A9082RZ 카메라 WN9 NPU 상 YOLOv11 커스텀 객체 탐지(Hand Detection) 써드파티 애플리케이션 개발

- 탐지된 손 객체의 실시간 영상 바운딩 박스 표시
- 탐지 이벤트 발생 시 카메라 알람 및 ONVIF 메타데이터 기반 외부 시스템(VMS 등) 알림 전송
- MQTT 프로토콜 기반 추론 로그/디버깅 정보 외부 브로커 전송 (원격 모니터링 및 문제 진단)

### 1.2 기술 환경 요약

| 항목 | 내용 |
|------|------|
| **타겟 하드웨어** | PNV-A9082RZ (WN9 칩셋, Vivante NPU) |
| **SDK** | Hanwha Vision OpenPlatform SDK 25.04.09 |
| **AI 모델** | YOLOv11n (Hand Detection), APL_AI Compiler .nb 변환 완료 |
| **개발 언어** | C++17 (CMake 빌드, ARM64 크로스컴파일) |
| **영상 전처리** | OpenCV (Tensor::Resize 또는 cv::resize 병행) |
| **디버깅 채널** | MQTT (외부 브로커 연동, JSON 포맷) |
| **결과 출력** | ONVIF 메타데이터, OSD 오버레이, 카메라 알람 출력 |
| **개발 환경** | Windows + Docker Desktop(WSL2) + VS Code Dev Containers |
| **소스 관리** | GitHub (ggapsang/SampleApplication_HOV) |

### 1.3 전제 조건 및 완료 작업

- Docker 개발 환경 구성 완료 (opensdk:25.04.09 이미지)
- CLI 도구(opensdk_new_project) 동작 확인 완료
- 테스트 서명 인증서(AppTest.crt/AppTest.key) 발급 완료
- run_neural_network / deepstream_raw_image 샘플 아키텍처 분석 완료
- YOLOv11 모델 .pth → .onnx → .nb 변환 파이프라인 완료 (APL_AI Compiler)
- .nb → .bin 확장자 변환 불필요 확인 완료 (.nb 파일 그대로 사용)

---

## 2. 시스템 아키텍처

### 2.1 전체 시스템 구조

한화비전 OpenPlatform 컴포넌트 기반 이벤트 드리븐 아키텍처 상 동작. 카메라 내부 System Container와 OpenSDK App Container 간 IPC(Stub-Skeleton) 통신 구조. 앱 컴포넌트는 매니페스트 기반 의존성 주입으로 시스템 컴포넌트와 연결.

#### 2.1.1 데이터 플로우

- **영상 입력**: 이미지센서 → SoC ISP → DMA-BUF(제로카피) → FeedFrame 이벤트 → 앱 컴포넌트
- **추론 파이프라인**: Raw Video 수신 → PreProcess(리사이즈/정규화) → NPU 추론 → PostProcess(NMS/디코딩) → 결과 출력
- **출력 분기**: ONVIF XML → MetadataManager → VMS / OSD 오버레이 / Alarm Output / MQTT 로그

#### 2.1.2 주요 컴포넌트 구성

| 컴포넌트 | 역할 |
|----------|------|
| **HandDetector** | 메인 컴포넌트. NPU 추론 파이프라인 전체 관장 (PreProcess, Execute, PostProcess, 결과 출력) |
| **AppNetworkManager** | SDK 제공 App Support Package. MQTT 브로커 연결용 TCP 클라이언트 역할 |
| **MetadataManager** | 시스템 컨테이너. ONVIF 메타데이터 외부 전송 담당 |
| **ConfigurableAlarmOut** | 시스템 컨테이너. 알람 출력 ON/OFF 제어 |
| **SRMgrVideo** | 시스템 컨테이너. OSD 메시지 오버레이 담당 |

### 2.2 NPU 경로 특성 (WN9)

WN9 칩셋은 NPU 경로 사용. DeepStream 경로(NVIDIA Jetson 전용)와 근본적으로 상이한 개발 방식. 커스텀 모델 탑재 시 PreProcess()와 PostProcess()를 C++로 완전 신규 작성 필요.

- **입력 텐서**: YOLOv11 모델 입력 해상도 RGB888 NHWC 포맷
- **출력 텐서**: FLOAT32 출력 (add_postproc_node: false + force_float32: true, NPU 역양자화 노드 미사용)
- **전처리**: mean/scale 파라미터 LoadNetwork() 호출 시 전달 (0-1 정규화 기준 mean=0, scale=1/255)
- **후처리**: YOLOv11 출력 텐서 디코딩 + NMS 앱 단 직접 구현

---

## 3. 프로젝트 디렉토리 구조

opensdk_new_project CLI 생성 템플릿 기반. 프로젝트명: `hand_detector`

| 경로 | 설명 |
|------|------|
| `app/CMakeLists.txt` | 루트 CMake 설정, 크로스컴파일 툴체인 포함 |
| `app/toolchain.cmake` | WN9 ARM64 크로스컴파일 설정 |
| `app/src/CMakeLists.txt` | 컴포넌트 빌드 관리, LCM/AppDispatcher 복사 |
| `app/src/PLifeCycleManagermanifest.json` | 컴포넌트 라이프사이클 및 Stub-Skeleton 설정 |
| **`app/src/hand_detector/`** | **메인 컴포넌트 소스 디렉토리** |
| `  includes/hand_detector.h` | HandDetector 클래스 헤더 (Component 상속) |
| `  includes/yolo_postprocess.h` | YOLOv11 후처리 유틸리티 헤더 (NMS, 디코딩) |
| `  includes/mqtt_logger.h` | MQTT 로깅 래퍼 클래스 헤더 |
| `  hand_detector.cc` | 메인 컴포넌트 구현부 |
| `  yolo_postprocess.cc` | NMS 및 바운딩 박스 디코딩 구현 |
| `  mqtt_logger.cc` | MQTT publish 로직 구현 |
| `  manifests/` | 매니페스트 인스턴스 JSON 파일 |
| `  CMakeLists.txt` | 컴포넌트별 빌드 설정 |
| `app/res/ai_bin/` | .nb 모델 파일 배치 경로 |
| `app/html/index.html` | 웹 UI (Start/Stop/설정) |
| `config/app_manifest.json` | 앱 이름, 채널 타입, 버전, 퍼미션 정의 |
| `docker-compose.yml` | 빌드/패키징 자동화 설정 |

---

## 4. 상세 개발 계획

### Phase 1: 프로젝트 스캐폴딩 및 빌드 환경 구성

> **목표**: 프로젝트 생성 → 빈 컴포넌트 카메라 기동 확인

#### 1-1. 프로젝트 생성

- `opensdk_new_project -n hand_detector -v 1.0 -c wn9 -s 25.04.09` 명령으로 프로젝트 템플릿 생성
- SampleComponent → HandDetector 리네이밍 (클래스명, 파일명, 매니페스트 일괄 변경)
- `extern "C"` 팩토리 함수에서 HandDetector 인스턴스 반환하도록 수정

#### 1-2. 매니페스트 설정

- HandDetector_manifest_instance_0.json 작성
- ReceiverNames: SPMgrVideoRaw_0 (Raw Video 수신), OpenPlatform (웹 UI 이벤트), AppNetworkManager (MQTT 네트워크)
- SourceNames: MetadataManager (메타데이터 전송), ConfigurableAlarmOut (알람), SRMgrVideo (OSD), LogManager (이벤트 로그)
- PLifeCycleManagermanifest.json에 HandDetector 컴포넌트 등록

#### 1-3. 빌드 및 배포 확인

- docker-compose.yml SOC=wn9 설정 확인
- `cmake -DSOC=wn9 .. && make install -j4 && opensdk_packager` → hand_detector.cap 생성
- 카메라 웹 뷰어 또는 `opensdk_install` CLI로 .cap 설치
- 카메라 로그에서 HandDetector Initialize/Start 라이프사이클 메시지 확인

**산출물**: 빌드 성공된 hand_detector.cap, 카메라 기동 로그 확인

---

### Phase 2: NPU 모델 로딩 및 추론 파이프라인 구축

> **목표**: .nb 모델 NPU 로드 → Raw Video 프레임 수신 → 추론 실행까지의 전체 파이프라인 구성

#### 2-1. 모델 파일 배치

- .nb 파일 `app/res/ai_bin/` 경로 배치 (예: hand_yolov11n.nb)
- LoadNetwork()의 `../res/ai_bin/` 상대 경로 모델 로드 구조 확인
- .nb 확장자 그대로 사용 가능 확인 완료

#### 2-2. NPU 초기화 시퀀스 구현

run_neural_network 샘플 Classification 클래스 참조, 아래 순서로 NPU 초기화:

1. **CreateNetwork**: NeuralNetwork 객체 생성 (모델명 기반)
2. **CreateInputTensor**: 입력 텐서 생성 (YOLOv11 입력 텐서 이름 지정)
3. **CreateOutputTensor**: 출력 텐서 생성 (YOLOv11 출력 헤드별 텐서 이름 지정)
4. **LoadNetwork**: 모델 로드 + mean/scale 파라미터 전달

mean/scale 설정 기준:
- 0-1 정규화: mean={0,0,0}, scale={1/255, 1/255, 1/255}
- APL_AI Compiler에서 add_preproc_node: true 컴파일한 경우: NPU 내부 정규화 처리 → LoadNetwork 시 mean={0,0,0}, scale={1,1,1} 전달

#### 2-3. Raw Video 수신 및 PreProcess 구현

System Container SPMgrVideoRaw로부터 FeedFrame 이벤트 수신

- ProcessAEvent()에서 eVideoRawData 이벤트 타입 처리
- RawImage에서 DMA-BUF 기반 제로카피 프레임 획득
- Tensor::Allocate()로 프레임 데이터 텐서 할당
- Tensor::Resize()로 모델 입력 해상도 리사이즈 (모델 입력 텐서 크기 기준)

OpenCV 활용:
- SDK Tensor::Resize()는 기본 리사이즈 제공
- 레터박싱(letterbox) 등 고급 전처리 필요 시 OpenCV cv::resize + cv::copyMakeBorder 사용
- YOLO 계열 모델은 레터박싱 전처리가 정확도에 직결 → OpenCV 사용 권장

#### 2-4. 추론 실행

- RunNetwork()으로 NPU 추론 실행
- stat_t 구조체 기반 추론 시간 측정
- run_flag 기반 추론 시작/중지 제어 구현

**산출물**: Raw Video 수신 → PreProcess → NPU 추론 실행 확인 (PostProcess 미구현 상태)

---

### Phase 3: YOLOv11 PostProcess 구현

> **목표**: NPU 추론 결과 텐서 파싱 → 손 객체 바운딩 박스, 신뢰도, 클래스 ID 추출

#### 3-1. 출력 텐서 구조 분석

YOLOv11 출력 텐서 레이아웃은 모델 export 설정에 따라 상이. Netron으로 .onnx 모델 출력 노드 shape/이름 정확히 확인 필수.

- 일반적 YOLOv11 Detection 출력: `[1, num_classes+4, num_detections]` 또는 `[1, num_detections, num_classes+4]`
- 4개 좌표값(cx, cy, w, h) + 클래스별 confidence score 구성
- Hand Detection 단일 클래스(hand) → num_classes = 1

#### 3-2. 디코딩 로직 구현 (yolo_postprocess.cc)

- 출력 텐서 VirtAddr()에서 float* 포인터 획득
- 바운딩 박스 좌표 디코딩: center_x, center_y, width, height → x1, y1, x2, y2 변환
- confidence threshold 필터링 (기본값 0.5, 웹 UI 조정 가능)
- NMS(Non-Maximum Suppression) 구현: IoU threshold 기반 중복 제거 (기본값 0.45)
- 좌표 원본 프레임 해상도 역변환 (리사이즈/레터박싱 비율 역산)

#### 3-3. 결과 데이터 구조 정의

| 필드 | 타입 | 설명 |
|------|------|------|
| bbox (x1,y1,x2,y2) | float[4] | 바운딩 박스 좌상단/우하단 좌표 (원본 해상도 기준) |
| confidence | float | 탐지 신뢰도 (0.0~1.0) |
| class_id | int | 클래스 ID (0: hand) |
| timestamp | uint64_t | 프레임 PTS 기반 타임스탬프 |

**산출물**: PostProcess 결과 디코딩 검증 (MQTT 로그로 바운딩 박스 좌표 출력)

---

### Phase 4: MQTT 로깅 모듈 구현

> **목표**: 추론 결과/디버깅 정보 MQTT 프로토콜 기반 외부 브로커 전송 → 원격 모니터링 구현

#### 4-1. 통신 방식 결정

| 방식 | 설명 | 비고 |
|------|------|------|
| **방식 A** | Network Manager TCP 클라이언트 + 자체 MQTT 프로토콜 구현 | SDK 기본 제공 기능 활용, MQTT 패킷 직접 조립 필요 |
| **방식 B** | Eclipse Paho MQTT C 라이브러리 직접 링크 | 완전한 MQTT 지원, 3rd party 라이브러리 app/libs에 배치 |

방식 B(Paho MQTT C 라이브러리) 권장. CMakeLists.txt에서 3rd party 라이브러리를 app/libs 경로에 배치/링크하는 구조는 SDK 공식 지원 패턴.

#### 4-2. MQTT 토픽 설계

| 토픽 | 페이로드 내용 |
|------|-------------|
| `cam/{cam_id}/hand/detection` | 탐지 결과 JSON (bbox, confidence, timestamp, frame_id) |
| `cam/{cam_id}/hand/status` | 앱 상태 (running/stopped, model_loaded, fps) |
| `cam/{cam_id}/hand/debug` | 디버그 로그 (preprocess_ms, inference_ms, postprocess_ms, error) |
| `cam/{cam_id}/hand/alarm` | 알람 이벤트 (trigger_count, last_detection_time) |

#### 4-3. 페이로드 포맷 (JSON)

전체 MQTT 메시지 JSON 포맷 전송. 카메라 내부 JSON 직렬화는 SDK 포함 rapidjson 활용.

- **detection**: `{"frame_id": 12345, "timestamp": "...", "detections": [{"bbox": [100,200,300,400], "conf": 0.87}]}`
- **debug**: `{"preprocess_ms": 2.1, "inference_ms": 15.3, "postprocess_ms": 1.2, "total_fps": 45.2}`
- **status**: `{"state": "running", "model": "hand_yolov11n.bin", "uptime_sec": 3600}`

#### 4-4. 연결 관리

- Initialize()에서 MQTT 브로커 연결 시도 (브로커 IP/Port: attribute JSON에서 로드)
- 연결 실패 시 재시도 로직 (exponential backoff, 최대 5회)
- 연결 실패가 추론 파이프라인에 영향 미발생하도록 비동기 처리
- Finalize()에서 정상 disconnect 수행

**산출물**: MQTT 브로커에서 detection/debug/status 토픽 메시지 수신 확인

---

### Phase 5: 영상 오버레이 및 알람 출력

> **목표**: 탐지 결과의 영상 시각화 표현 + 카메라 알람 시스템 연동

#### 5-1. ONVIF 메타데이터 전송 (Frame Metadata)

탐지 바운딩 박스 좌표를 ONVIF 표준 메타데이터로 전송. VMS 수신 시 영상 위 바운딩 박스 렌더링 가능.

- StreamMetadata 객체 생성 (채널, 타임스탬프)
- FrameMetadataItem에 해상도, 타임스탬프, ObjectItem(바운딩 박스 좌표) 설정
- `SendNoReplyEvent("MetadataManager", eRequestMetadata, ...)` 호출
- 메타데이터 전송 간격: 최소 30ms 이상 (SDK QoS 제약)

#### 5-2. OSD 오버레이

카메라 자체 OSD 기능 활용, 탐지 상태 영상 위 텍스트 표시

- MultilineOsdItem 구조체로 텍스트/위치/색상/폰트 크기 설정
- 표시 예시: `"Hand Detected: 3 | FPS: 30 | Conf: 0.92"`
- `SendNoReplyEvent("SRMgrVideo", eExternalMultiLineOsd, ...)` 호출

#### 5-3. 알람 출력

손 탐지 시 카메라 Alarm Output 트리거

- `IConfigurableAlarmOut::EEvent::eRelayRequest` 이벤트 사용
- `RelayInstance::Builder`로 `SetAction("on")`, `SetChannel(0)` 설정
- 탐지 지속 시간 기반 트리거: N프레임 연속 탐지 시 알람 ON, M프레임 미탐지 시 알람 OFF
- 알람 쿨다운 시간 설정 (반복 트리거 방지)

#### 5-4. 이벤트 로그 기록

- `Log::LogType::EVENT_LOG`, `Log::LogDetailType::EVENT_OPENAPP` 타입 기록
- 탐지 시작/종료, 알람 트리거/해제 시점 로그 기록
- `SendNoReplyEvent("LogManager", eWrite, ...)` 호출

**산출물**: VMS 바운딩 박스 확인, 카메라 OSD 표시 확인, 알람 출력 동작 확인

---

### Phase 6: 웹 UI 및 설정 인터페이스

> **목표**: 카메라 웹 뷰어에서 앱 시작/중지 및 파라미터 조정을 위한 HTTP 기반 웹 UI 구성

#### 6-1. 웹 UI 구성 (app/html/index.html)

- Start/Stop 버튼: NPU 추론 시작/중지 제어
- 모델 정보 표시: 모델명, 입출력 텐서 정보
- 파라미터 조정: confidence threshold, NMS IoU threshold
- MQTT 설정: 브로커 IP, 포트, 토픽 프리픽스
- 알람 설정: 트리거 프레임 수, 쿨다운 시간
- 상태 모니터링: 현재 FPS, 추론 시간, 탐지 카운트

#### 6-2. HTTP API 엔드포인트

웹 UI에서 `/stream?mode=view` 엔드포인트 통해 앱 컴포넌트와 통신. JSON 포맷 요청/응답.

- `mode=start`: 추론 시작
- `mode=stop`: 추론 중지
- `mode=info`: 현재 상태 조회
- `mode=config`: 파라미터 변경 (confidence, nms_iou, mqtt_broker 등)

#### 6-3. Attribute 파일 관리

- `storage/settings/` 경로에 attribute JSON 파일 배치
- 앱 업그레이드 시 기존 설정 유지 여부 선택 가능 (OpenSDK storage 구조 활용)
- 기본값: confidence_threshold=0.5, nms_iou=0.45, mqtt_broker="", mqtt_port=1883

**산출물**: 웹 UI에서 파라미터 조정 및 추론 제어 동작 확인

---

### Phase 7: 메타데이터 스키마 등록 및 이벤트 연동

> **목표**: 카메라 이벤트 시스템에 커스텀 이벤트 등록 → VMS/NVR 표준 연동

#### 7-1. Dynamic Schema 등록

- Metadata Schema: ONVIF 표준 포맷 Hand Detection 이벤트 스키마 정의
- EventStatus Schema: 이벤트 상태(탐지 중/미탐지) 스키마 정의
- OpenEventDispatcher 통해 `eMetadataSchema`, `eEventStatusSchema` 이벤트 전송
- 카메라 CGI(`eventstatus.cgi?msubmenu=metadataschema&action=view`)로 등록 확인

#### 7-2. Event Metadata 전송

- EventMetadataItem으로 이벤트 토픽/소스/데이터 구성
- 토픽 패턴: `tns1:VideoSource/tnssamsung:hand_detector`
- Data 필드: State(boolean), DetectionCount(int), MaxConfidence(float)
- `NeedBroadcasting(true)` 설정으로 전체 구독자 전송

**산출물**: VMS 커스텀 이벤트 수신 확인, 이벤트 기반 녹화 룰 동작 확인

---

### Phase 8: 통합 테스트 및 최적화

#### 8-1. 기능 테스트

| # | 테스트 항목 | 검증 방법 | 합격 기준 |
|---|-----------|----------|----------|
| 1 | 모델 로딩 | 카메라 재부팅 후 자동 로드 확인 | LoadNetwork 성공 로그 |
| 2 | 손 탐지 정확도 | 다양한 거리/조명 테스트 | mAP 50% 이상 |
| 3 | 추론 성능 | FPS 측정 (MQTT debug 토픽) | 15 FPS 이상 |
| 4 | 알람 출력 | N프레임 연속 탐지 시 트리거 | 5초 이내 응답 |
| 5 | ONVIF 메타데이터 | VMS 바운딩 박스 표시 | 좌표 정확성 |
| 6 | OSD 오버레이 | 영상 텍스트 표시 확인 | 정상 표시 |
| 7 | MQTT 로깅 | 브로커 메시지 수신 확인 | JSON 파싱 성공 |
| 8 | 웹 UI | Start/Stop/Config 동작 | 정상 제어 |
| 9 | 장시간 안정성 | 24시간 연속 운영 | 메모리 누수 없음 |
| 10 | 네트워크 단절 | MQTT 연결 끊김 후 재연결 | 추론 중단 없음 |

#### 8-2. 성능 최적화 포인트

- **PreProcess 최적화**: 레터박싱 연산 최소화 위해 카메라 프로파일 해상도를 모델 입력 비율에 맞게 조정
- **메모리 관리**: Tensor 객체 재사용 (매 프레임 할당/해제 방지), DMA-BUF 제로카피 활용
- **추론 주기 조절**: 전 프레임 추론이 아닌 N프레임 간격 추론 (skip frame, 웹 UI 조정 가능)
- **메타데이터 전송**: 30ms 최소 간격 준수, 변화 없을 시 전송 생략
- **MQTT 전송**: QoS 0 사용 (디버깅 용도), 전송 실패가 추론에 영향 없도록 비동기 처리

#### 8-3. 패키징 및 배포

- `opensdk_packager`로 최종 hand_detector.cap 생성
- 서명 인증서(AppTest.crt/AppTest.key) 적용
- STEP 파트너 포털 앱 등록 (배포 시)

---

## 5. 개발 순서 요약

| # | Phase | 핵심 산출물 | 예상 소요 | 의존성 |
|---|-------|-----------|----------|--------|
| 1 | **스캐폴딩/빌드** | 빈 컴포넌트 카메라 기동 | 2~3일 | 없음 |
| 2 | **NPU 추론 파이프라인** | Raw Video → PreProcess → 추론 실행 | 3~5일 | Phase 1 |
| 3 | **PostProcess 구현** | 바운딩 박스 디코딩 + NMS | 3~5일 | Phase 2 |
| 4 | **MQTT 로깅** | 브로커 연동, JSON 로그 전송 | 2~3일 | Phase 1 |
| 5 | **영상 출력/알람** | ONVIF 메타데이터, OSD, 알람 | 3~4일 | Phase 3 |
| 6 | **웹 UI/설정** | 파라미터 조정 인터페이스 | 2~3일 | Phase 1 |
| 7 | **스키마/이벤트 연동** | VMS 이벤트 통합 | 2~3일 | Phase 5 |
| 8 | **통합 테스트/최적화** | 기능/성능/안정성 검증 | 5~7일 | 전체 |

Phase 4(MQTT), Phase 6(웹 UI)은 Phase 2~3과 병렬 진행 가능. 전체 예상 소요 기간: 약 4~5주.

---

## 6. 리스크 및 미해결 사항

| # | 리스크 항목 | 상세 내용 | 대응 방안 |
|---|-----------|----------|----------|
| 1 | **YOLOv11 출력 텐서 형태** | NPU 컴파일 후 출력 텐서 shape이 원본 ONNX와 상이 가능성 | Netron .onnx 확인 + APL_AI `./inference` 결과 .tensor 파일명의 shape 비교 검증 |
| 2 | **MQTT 라이브러리 호환성** | Paho MQTT C 라이브러리 ARM64 크로스컴파일 이슈 가능성 | 대안: Network Manager TCP + 최소 MQTT 패킷 구현 또는 mosquitto_pub CLI 호출 |
| 3 | **메모리 제약** | 카메라 임베디드 환경 제한 RAM에서 모델+앱+OpenCV 동시 운용 | YOLOv11n(nano) 모델로 메모리 최소화, OpenCV 최소 빌드 검토 |
| 4 | **양자화 정확도 손실** | FLOAT32 → INT8 양자화 Hand Detection 정확도 저하 가능성 | 캘리브레이션 이미지 품질 확보 (실 운영 환경 유사 이미지 500~1000장) |

---

## 7. 외주 개발 위탁 시 전달 사항

외주 개발사 위탁 시 반드시 전달 필요한 핵심 정의 항목:

| # | 전달 항목 | 상세 내용 |
|---|----------|----------|
| 1 | **모델 입출력 텐서 스펙** | 입력 텐서 이름/크기/포맷, 출력 텐서 이름/크기/파싱 방식 (Netron 캡처 포함) |
| 2 | **전처리 파라미터** | mean, scale(variance), 입력 해상도, 컬러 스페이스 변환 방식, 레터박싱 여부 |
| 3 | **후처리 스펙** | 출력 텐서 디코딩 방식, NMS 파라미터, 좌표 역변환 로직 |
| 4 | **메타데이터 출력 요구사항** | ONVIF XML 스키마, 이벤트 토픽명, 포함 데이터 필드 정의 |
| 5 | **타겟 SoC 및 SDK 버전** | WN9, OpenSDK 25.04.09, Docker 이미지 opensdk:25.04.09 |
| 6 | **MQTT 통신 요구사항** | 토픽 구조, 페이로드 포맷(JSON), QoS, 재연결 정책 |
| 7 | **웹 UI 범위** | Start/Stop/Info/Config 수준, 추가 설정 페이지 요구사항 |
| 8 | **참조 소스코드** | run_neural_network 샘플 Classification 클래스 (GitHub 레포) |
| 9 | **아키텍처 문서** | Hanwha_OpenSDK_Architecture_Analysis.md, npu_dataflow.mermaid |

> 상세 스펙은 `hand_detector_outsource_spec.yaml` 참조.
> YAML 내 `TODO` 항목은 PM이 Netron 확인 및 APL_AI inference 실행 후 실측값으로 채워 넣을 것.

---

*— 문서 끝 —*
