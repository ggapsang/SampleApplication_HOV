# OpenSDK Object Detector — 개발 핵심 지식

> 타겟: PNV-A9082RZ (WN9), SDK 25.04.09
> 실제 디버깅으로 검증된 사실만 기록.

---

## 1. 프로젝트 개요

- **앱 이름**: `object_detector` (과거 `hand_detector`에서 일반화됨)
- **C++ 클래스**: `ObjectDetector`
- **라이브러리**: `libobject_detector.so`
- **목표**: `.nb` 모델 파일과 `model_config.json`만 교체하면 다른 객체 감지 가능한 범용 앱

## 2. 설계 철학

- 모델/후처리 파라미터(텐서명, 입력 크기, 앵커, 클래스 라벨, OSD 표시명, MQTT 토픽 프리픽스 등)는 전부 `app/res/ai_bin/model_config.json`으로 외부화
- C++ 코드는 `ObjectDetector` 클래스 하나로 모든 YOLO 계열 모델 커버
- 현재 설정: YOLOv7 앵커 기반 후처리 (640x640 입력, 단일 클래스 hand)
- 모델 교체 시 코드 수정 없이 JSON + .nb 파일만 교체

## 3. 매니페스트 필수 규칙 (검증 완료)

### SchedulerNames
```json
"SchedulerNames": ["EComponents::eScheduler1", "ObjectDetectorScheduler"]
```
- `"eScheduler1"` (prefix 없음)은 **작동 안 함**
- 반드시 `"EComponents::eScheduler1"` 사용
- LCM과 AppDispatcher 매니페스트가 **동시에** 같은 값이어야 함

### AppDispatcher 매니페스트
```json
"SchedulerName": "EComponents::eScheduler1",
"Channel": 0,
"ModelPath": "",
"SettingPath": "",
"ReceiverNames": ["Stub::Dispatcher::OpenAPI", "ObjectDetector"],
"SourceNames": [{
  "Source": "Stub::Dispatcher::OpenAPI",
  "GroupName": "OpenSDK::object_detector::Dispatcher"
}]
```
- Channel: `"${APPCHANNEL}"` **아님**, 반드시 `0` (정수)
- `ReceiverNames`의 `"ObjectDetector"`는 instance 매니페스트의 `InstanceName`과 일치해야 함
- `GroupName`의 `object_detector`는 `config/app_manifest.json`의 `AppName`과 일치해야 함

### Instance 매니페스트 SourceNames
| Source | LCM 매핑 필요 | 비고 |
|--------|--------------|------|
| `SPMgrVideoRaw_${APPCHANNEL}` | ✅ 필요 | 비디오 프레임 수신 |
| `OpenPlatform` | ❌ 불필요/금지 | SDK 자동 전달, LCM 매핑 없음 |

### LCM 최소 구성
```json
{"LocalComponentName": "Stub::Dispatcher::OpenAPI", "RemoteComponentName": "OpenAPIDispatcher"},
{"LocalComponentName": "2009004", "RemoteComponentName": "2009004"},
{"LocalComponentName": "SPMgrVideoRaw_${APPCHANNEL}", "RemoteComponentName": "SPMgrVideoRaw_${APPCHANNEL}"},
{"LocalComponentName": "MetadataManager_${APPCHANNEL}", "RemoteComponentName": "MetadataManager_${APPCHANNEL}"},
{"LocalComponentName": "SRMgrVideo_${APPCHANNEL}", "RemoteComponentName": "SRMgrVideo_${APPCHANNEL}"},
{"LocalComponentName": "ConfigurableAlarmOut", "RemoteComponentName": "ConfigurableAlarmOut"},
{"LocalComponentName": "AppNetworkManager", "RemoteComponentName": "AppNetworkManager"}
```

## 4. HTTP API 규칙 (검증 완료)

### RegisterOpenAPIURI — POST만
```cpp
methods.push_back("POST");  // GET 추가하면 오동작 가능
```

### Initialize() 순서
```cpp
bool ObjectDetector::Initialize() {
  RegisterOpenAPIURI();           // 1. FIRST
  LoadModelConfig(...);           // 2. 모델 설정 로드
  PrepareAttributes(...);         // 3.
  ParseManifest(...);             // 4.
  // ...
  return Component::Initialize(); // 반드시 base 호출
}
```

### index.html API — 모두 POST + JSON body
```js
fetch(base_uri, {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ mode: mode })
})
```
mode 추출 순서: `QUERY_STRING` → body JSON

## 5. Log 관련 규칙 (검증 완료)

### Log 객체 생성 — placement new 미지원
```cpp
// ✅ 올바름
auto* log = new Log(Log::LogType::EVENT_LOG, Log::LogDetailType::EVENT_OPENAPP,
    0, time(NULL), Platform_Std_Refine::String(message.c_str()));

// ❌ 틀림
auto* log = new ("Log") Log(...);
```

### LogManager 매핑 — enum 값 사용
- 코드: `SendNoReplyEvent("LogManager", ...)`
- Instance ReceiverNames: `{"SymbolName": "LogManager", "RealName": "2009004"}`
- LCM: `{"LocalComponentName": "2009004", "RemoteComponentName": "2009004"}`
- `"LogManager"` 문자열로 LCM에 넣으면 **작동 안 함**

### 디버그 인메모리 로그
- `AppendLog()`로 `debug_log_` 벡터에 추가 (FIFO, 최대 200개)
- `mode=log` POST → 로그 배열 반환
- `mode=clear` POST → 로그 초기화

## 6. model_config.json 구조

```json
{
  "model": {
    "file": "network_binary.nb",
    "input_width": 640, "input_height": 640,
    "input_tensor": "images_0",
    "output_tensors": ["attach_output0/out0"],
    "mean": [0,0,0], "scale": [1,1,1]
  },
  "postprocess": {
    "num_classes": 1, "max_detections": 100,
    "anchors": [[...], [...], [...]],
    "strides": [8, 16, 32],
    "num_anchors_per_level": 3
  },
  "classes": ["hand"],
  "display": {
    "name": "Hand",
    "osd_format": "{name}: {count} | Conf: {conf}%",
    "osd_no_detection": "{name}: 0"
  },
  "mqtt": {
    "topic_prefix": "hand",
    "client_id": "hand_detector"
  }
}
```

파일이 없으면 C++ 기본값(`model_config.h`의 `ModelConfig` 구조체 초기값) 사용.

## 7. 빌드 및 배포

자세한 내용은 루트 [`BUILD.md`](../BUILD.md) 참조.

```bash
# .env
APP_NAME=object_detector
SDK_VER=25.04.09
SOC=wn9

# 클린 빌드
docker-compose down --remove-orphans
docker-compose up
```

결과물: `object_detector.cap`

## 8. 디렉토리 구조

```
run_yolo/
├── BUILD.md                              ← 빌드 가이드
├── .env                                  ← APP_NAME, SDK_VER, SOC
├── docker-compose.yml                    ← cert_keys 볼륨 마운트 포함
├── cert_keys/                            ← 서명 키 (git 미추적)
│   ├── AppTest.crt
│   └── AppTest.key
├── config/
│   └── app_manifest.json                 ← AppName: "object_detector"
└── app/
    ├── html/index.html                   ← 웹 UI
    ├── res/
    │   ├── ai_bin/
    │   │   ├── network_binary.nb         ← NPU 모델
    │   │   └── model_config.json         ← 모델별 설정
    │   └── models/
    │       ├── AppDispatcher_manifest_instance_0.json
    │       └── Classification_manifest_instance_0.json
    └── src/
        ├── PLifeCycleManagermanifest.json
        └── classification/
            ├── classification.cc         ← ObjectDetector 메인
            ├── includes/
            │   ├── classification.h
            │   ├── model_config.h
            │   ├── mqtt_logger.h
            │   └── yolo_postprocess.h
            ├── model_config.cc           ← JSON 파서
            ├── mqtt_logger.cc            ← 최소 MQTT 3.1.1 클라이언트
            ├── yolo_postprocess.cc       ← (미사용, DFL 경로)
            └── manifests/
                ├── Classification_manifest.json
                ├── Classification_manifest_instance_0.json
                └── Classification_default_attribute_0.json
```

## 9. 참조 샘플 앱

| 기능 | 참조 샘플 | 위치 |
|------|-----------|------|
| HTTP API + 이벤트 로그 | **write_event_log** | `../write_event_log/` |
| 영상 처리 + NPU 추론 | **run_neural_network** | `../run_neural_network/` |

## 10. 브랜치 전략

- `master` — 리팩토링된 범용 `object_detector` 버전 (현재 작업 중)
- `hand_detector` — 리팩토링 이전 `hand_detector` 버전 보존용
