# OpenSDK 디버깅 교훈록 — Phase 1 (HTTP API + 이벤트 로그)

> 작성일: 2026-03-23
> 대상: hand_detector (PNV-A9082RZ, WN9, SDK 25.04.09)
> 목적: 동일한 실수를 반복하지 않기 위한 기록. Phase 2 이후 개발 및 외주 관리 시 참조.

---

## 1. 근본 원인: 500 에러의 정체

### 증상
- Go App 페이지의 모든 API 버튼이 `{"Response":"Fail","Error":{"Code":500,"Details":"Unknown Error"}}` 반환
- 카메라 이벤트 로그에 커스텀 로그 없음
- 디버그 콘솔에도 앱 로직 출력 없음

### 최초 추정 (틀렸던 것)
- "RegisterOpenAPIURI()에서 GET을 등록하지 않아서 GET 요청이 500이다"
- 실제로는 POST만 등록하는 것이 올바른 패턴이었음

### 실제 원인
Instance 매니페스트 `SourceNames`에 `OpenPlatform` 구독이 있었는데, LCM `RemoteComponentNames`에 대응하는 `OpenPlatform` 매핑이 없었음. 이 불일치로 **컴포넌트 초기화 자체가 실패**하여, HTTP 요청이 컴포넌트에 도달하기 이전에 SDK 프레임워크 레벨에서 500이 반환되었음.

### 해결
```json
// SourceNames에서 OpenPlatform 항목 제거
// AS-IS (오류)
"SourceNames": [
  {"Source": "SPMgrVideoRaw_${APPCHANNEL}", "GroupName": "GroupSPMgrVideoRaw2"},
  {"Source": "OpenPlatform", "GroupName": "AppMessage"},
  {"Source": "OpenPlatform", "GroupName": "SettingChange"}
]

// TO-BE (정상)
"SourceNames": [
  {"Source": "SPMgrVideoRaw_${APPCHANNEL}", "GroupName": "GroupSPMgrVideoRaw2"}
]
```

### 교훈
- OpenPlatform 이벤트(eNetworkSettingChanged 등)는 SDK가 자동 전달함. 명시적 구독 불필요.
- **SourceNames에 등록한 모든 Source는 LCM RemoteComponentNames에 매핑이 존재해야 함.** 하나라도 매핑이 없으면 컴포넌트가 초기화되지 않음.
- SDK는 이 실패를 로그로 알려주지 않음.

---

## 2. 함께 발견된 문제들 (모두 해결 완료)

### 2.1 SchedulerNames 형식

| 항목 | 틀린 값 | 올바른 값 |
|------|---------|-----------|
| LCM SchedulerNames | `"eScheduler1"` | `"EComponents::eScheduler1"` |

- LCM과 AppDispatcher 매니페스트 양쪽을 **동시에** 같은 값으로 맞춰야 함
- 한쪽만 바꾸면 앱이 기동되지 않음

### 2.2 Channel 값 타입

| 항목 | 틀린 값 | 올바른 값 |
|------|---------|-----------|
| Instance 매니페스트 Channel | `"${APPCHANNEL}"` | `0` (정수) |
| AppDispatcher Channel | `"${APPCHANNEL}"` | `0` (정수) |

- SDK 샘플 문서에는 `${APPCHANNEL}`로 표기되어 있지만, 실제 동작하는 값은 정수 `0`

### 2.3 LogManager ID

| 항목 | 틀린 값 | 올바른 값 |
|------|---------|-----------|
| LCM LocalComponentName | `"LogManager"` | `"2009004"` |
| Instance ReceiverNames RealName | `"LogManager"` | `"2009004"` |

- `2009004`는 `LifeCycleManagerInterface::EReceivers::eLogManager`의 enum 값
- 코드에서는 `SendNoReplyEvent("LogManager", ...)`로 SymbolName을 사용하지만, 매핑 키는 RealName인 `"2009004"`

### 2.4 Log 객체 생성 방식

```cpp
// 틀림 — Log 클래스는 placement new 미지원
auto* log = new ("Log") Log(...);

// 올바름
auto* log = new Log(Log::LogType::EVENT_LOG, Log::LogDetailType::EVENT_OPENAPP,
    0, time(NULL), Platform_Std_Refine::String(message.c_str()));
```

- SDK의 다른 클래스(SerializableString, NetworkConfig, RelayRequest 등)는 `new ("ClassName") ClassName(...)` 형태를 사용하지만, `Log`는 예외

### 2.5 Component::Initialize() 미호출

```cpp
// 틀림
bool HandDetector::Initialize() {
  // ... 초기화 코드 ...
  return true;
}

// 올바름
bool HandDetector::Initialize() {
  RegisterOpenAPIURI();
  PrepareAttributes(...);
  // ...
  return Component::Initialize();  // 반드시 base 클래스 호출
}
```

- `Component::Initialize()`를 호출하지 않으면 SDK 프레임워크 내부 등록이 완료되지 않음

### 2.6 RegisterOpenAPIURI — POST만 등록

```cpp
// 올바른 패턴 (hand_detector 기준)
Vector<String> methods;
methods.push_back("POST");  // POST만. GET은 추가하지 않음.
```

- index.html에서 모든 API 호출을 POST + JSON body로 통일
- `mode` 값은 body의 JSON에서 추출

---

## 3. 공통 패턴: 왜 디버깅이 어려웠는가

이번 Phase 1에서 발생한 모든 문제에는 공통점이 있음:

**SDK 프레임워크가 설정 오류를 에러 메시지 없이 조용히 무시한다.**

- SourceNames-LCM 매핑 불일치 → 컴포넌트 초기화 실패, 에러 로그 없음
- ReceiverNames-LCM 매핑 불일치 → 이벤트가 조용히 버려짐
- SchedulerNames 형식 오류 → 앱 불기동, 원인 불명
- Log placement new 오류 → 컴파일 에러는 나지만 원인 메시지가 불친절

**결론: 문제가 발생하면 코드 로직을 의심하기 전에, 매니페스트 3종(LCM, Instance, AppDispatcher)의 값을 동작하는 샘플과 한 글자씩 비교하는 것이 가장 빠르다.**

---

## 4. 검증된 매니페스트 구성 (Phase 1 기준)

### PLifeCycleManagermanifest.json
```json
{
  "SkeletonPortNumber": "auto",
  "ContainerName": "hand_detector",
  "AcceptLocalOnly": false,
  "SchedulerNames": ["EComponents::eScheduler1", "HandDetectorScheduler"],
  "RemoteContainerNames": [
    {"ContainerName": "System", "Address": "localhost", "PortNumber": 8587}
  ],
  "RemoteComponentNames": [
    {"LocalComponentName": "Stub::Dispatcher::OpenAPI", "RemoteComponentName": "OpenAPIDispatcher"},
    {"LocalComponentName": "2009004", "RemoteComponentName": "2009004"}
  ]
}
```

### Classification_manifest_instance_0.json
```json
{
  "LibraryFileName": "libclassification.so",
  "Instance": {
    "InstanceName": "HandDetector",
    "SchedulerName": "HandDetectorScheduler",
    "ReceiverNames": [
      "AppDispatcher",
      {"SymbolName": "LogManager", "RealName": "2009004"}
    ],
    "SourceNames": [],
    "Channel": 0,
    "SettingPath": "../storage/settings/"
  }
}
```

### AppDispatcher_manifest_instance_0.json
```json
{
  "Instance": {
    "InstanceName": "AppDispatcher",
    "SchedulerName": "EComponents::eScheduler1",
    "ReceiverNames": ["Stub::Dispatcher::OpenAPI", "HandDetector"],
    "SourceNames": [
      {"Source": "Stub::Dispatcher::OpenAPI", "GroupName": "OpenSDK::hand_detector::Dispatcher"}
    ],
    "Channel": 0,
    "ModelPath": "",
    "SettingPath": ""
  }
}
```

Phase 2에서 영상 처리를 추가할 때는 LCM에 `SPMgrVideoRaw_0`, `MetadataManager_0` 등의 매핑을 추가하고, Instance SourceNames에도 대응하는 항목을 넣어야 함.

---

## 5. Phase 2 진입 시 확인 사항

Phase 2(NPU 모델 로딩 + 영상 프레임 수신)에서 매니페스트를 확장할 때, 이번 교훈을 반드시 적용할 것:

1. SourceNames에 항목을 추가하면 LCM RemoteComponentNames에도 반드시 매핑 추가
2. ReceiverNames에 항목을 추가하면 LCM RemoteComponentNames에도 반드시 매핑 추가
3. 추가 후 빌드 전에 `run_neural_network` 샘플의 매니페스트와 비교 대조
4. 한 번에 하나씩만 추가하고, 추가할 때마다 카메라에 설치하여 앱 기동 여부 확인

---

## 6. 참고 자료

| 문서 | 용도 |
|------|------|
| `CLAUDE.md` | 검증된 규칙과 현재 구현 상태 |
| `OpenSDK_Developer_Reference.md` | SDK 아키텍처 전체 레퍼런스 |
| `write_event_log` 샘플 | HTTP + 이벤트 로그 레퍼런스 |
| `run_neural_network` 샘플 | NPU 추론 레퍼런스 |
