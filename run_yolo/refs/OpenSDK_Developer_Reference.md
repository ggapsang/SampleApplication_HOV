# Hanwha Vision OpenSDK — 개발자 핵심 레퍼런스

> 타겟: PNV-A9082RZ (WN9 칩셋, NPU 경로)
> SDK: opensdk:25.04.09
> 작성일: 2026-03-23

---

## 1. 시스템 아키텍처

카메라 내부에 두 개의 컨테이너가 IPC로 통신합니다.

```
┌─────────────────────────────────────────────────────────┐
│                     카메라 (ARM64)                        │
│                                                          │
│  ┌──────────────────────┐    ┌─────────────────────────┐ │
│  │   System Container   │◄──►│  OpenSDK App Container  │ │
│  │   (port 8587)        │IPC │  (우리 앱)                │ │
│  │                      │    │                          │ │
│  │  - VideoRaw Provider │    │  - AppDispatcher         │ │
│  │  - MetadataManager   │    │  - UserComponent         │ │
│  │  - LogManager        │    │    (Classification 등)    │ │
│  │  - OpenPlatform      │    │                          │ │
│  │  - AlarmIO           │    │                          │ │
│  │  - OpenAPIDispatcher │    │                          │ │
│  └──────────────────────┘    └─────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

핵심: 앱 컨테이너에서 System Container의 컴포넌트를 사용하려면 **매니페스트에 매핑을 선언**해야 합니다. 선언하지 않은 대상에 이벤트를 보내면 에러 없이 무시됩니다.

---

## 2. 매니페스트 구조 — 가장 중요한 부분

앱은 3개의 매니페스트 파일로 구성됩니다. 이 세 파일의 매핑이 모두 일치해야 이벤트가 전달됩니다.

### 2.1 PLifeCycleManagermanifest.json (LCM)

앱 컨테이너 전체의 설정입니다. System Container와의 연결을 정의합니다.

```json
{
  "SkeletonPortNumber": "auto",
  "ContainerName": "hand_detector",
  "AcceptLocalOnly": false,
  "SchedulerNames": [
    "EComponents::eScheduler1",
    "HandDetectorScheduler"
  ],
  "RemoteContainerNames": [
    {
      "ContainerName": "System",
      "Address": "localhost",
      "PortNumber": 8587
    }
  ],
  "RemoteComponentNames": [
    {
      "LocalComponentName": "Stub::Dispatcher::OpenAPI",
      "ContainerName": "System",
      "RemoteComponentName": "OpenAPIDispatcher"
    },
    {
      "LocalComponentName": "SPMgrVideoRaw_${APPCHANNEL}",
      "ContainerName": "System",
      "RemoteComponentName": "SPMgrVideoRaw_${APPCHANNEL}"
    },
    {
      "LocalComponentName": "Stub::Dispatcher::OpenSDK",
      "ContainerName": "System",
      "RemoteComponentName": "OpenSDKCGIDispatcher"
    },
    {
      "LocalComponentName": "MetadataManager_${APPCHANNEL}",
      "ContainerName": "System",
      "RemoteComponentName": "MetadataManager_${APPCHANNEL}"
    },
    {
      "LocalComponentName": "2009004",
      "ContainerName": "System",
      "RemoteComponentName": "2009004"
    },
    {
      "LocalComponentName": "ConfigurableAlarmOut",
      "ContainerName": "System",
      "RemoteComponentName": "ConfigurableAlarmOut"
    },
    {
      "LocalComponentName": "AppNetworkManager",
      "ContainerName": "System",
      "RemoteComponentName": "AppNetworkManager"
    }
  ]
}
```

**주의 사항:**
- `SchedulerNames`의 기본 스케줄러는 `"EComponents::eScheduler1"` (eScheduler1이 아님)
- `LocalComponentName`은 인스턴스 매니페스트의 `RealName`과 **정확히** 일치해야 함
- LogManager의 `LocalComponentName`은 `"2009004"` (문자열 "LogManager"가 아님)


### 2.2 인스턴스 매니페스트 (Classification_manifest_instance_0.json)

컴포넌트 인스턴스의 이벤트 수신/발신 대상을 정의합니다.

```json
{
  "LibraryFileName": "libclassification.so",
  "Instance": {
    "InstanceName": "HandDetector",
    "SchedulerName": "HandDetectorScheduler",
    "ReceiverNames": [
      "AppDispatcher",
      "RuleManager",
      "Stub::Dispatcher::OpenSDK",
      {
        "SymbolName": "MetadataManager",
        "RealName": "MetadataManager_${APPCHANNEL}"
      },
      {
        "SymbolName": "LogManager",
        "RealName": "2009004"
      },
      "OpenPlatform",
      "AppNetworkManager",
      "ConfigurableAlarmOut"
    ],
    "SourceNames": [
      {
        "Source": "SPMgrVideoRaw_${APPCHANNEL}",
        "GroupName": "GroupSPMgrVideoRaw2"
      },
      {
        "Source": "OpenPlatform",
        "GroupName": "AppMessage"
      },
      {
        "Source": "OpenPlatform",
        "GroupName": "SettingChange"
      }
    ],
    "Channel": "${APPCHANNEL}",
    "SettingPath": "../storage/settings/"
  }
}
```

**ReceiverNames 규칙:**
- `SendNoReplyEvent("LogManager", ...)` 를 코드에서 호출하려면, ReceiverNames에 `SymbolName: "LogManager"`가 반드시 있어야 합니다.
- `RealName`은 LCM의 `LocalComponentName`과 일치해야 합니다.
- ReceiverNames에 없는 대상으로 이벤트를 보내면 **에러 없이 무시**됩니다.


### 2.3 AppDispatcher 매니페스트 (AppDispatcher_manifest_instance_0.json)

HTTP 요청을 컴포넌트로 라우팅하는 디스패처입니다.

```json
{
  "Instance": {
    "InstanceName": "AppDispatcher",
    "SchedulerName": "EComponents::eScheduler1",
    "ReceiverNames": ["Stub::Dispatcher::OpenAPI", "HandDetector"],
    "SourceNames": [
      {
        "Source": "Stub::Dispatcher::OpenAPI",
        "GroupName": "OpenSDK::hand_detector::Dispatcher"
      }
    ],
    "Channel": "${APPCHANNEL}"
  }
}
```

**주의:** `ReceiverNames`에 있는 `"HandDetector"`는 인스턴스 매니페스트의 `InstanceName`과 일치해야 합니다.


### 2.4 세 파일 간 매핑 관계 (요약)

```
코드에서 SendNoReplyEvent("LogManager", ...)
          │
          ▼
인스턴스 매니페스트 ReceiverNames:
  SymbolName: "LogManager" ── 코드에서 사용하는 이름
  RealName: "2009004"       ── LCM으로 연결되는 키
          │
          ▼
LCM RemoteComponentNames:
  LocalComponentName: "2009004"           ── RealName과 일치
  RemoteComponentName: "2009004"          ── System Container의 실제 이름
          │
          ▼
System Container의 LogManager 컴포넌트에 이벤트 도달
```

**세 단계 중 하나라도 불일치하면 이벤트가 조용히 버려집니다.**

---

## 3. 컴포넌트 라이프사이클

```cpp
extern "C" {
  // SDK 프레임워크가 호출하는 팩토리 함수
  Classification* create_component(void *mem_manager) {
    Component::allocator = decltype(Component::allocator)(mem_manager);
    Event::allocator = decltype(Event::allocator)(mem_manager);
    return new("Classification") Classification();
  }
  void destroy_component(Classification *ptr) { delete ptr; }
}
```

호출 순서: `create_component()` → `Initialize()` → `Start()` → `ProcessAEvent()` 반복 → `Finalize()`

```cpp
bool HandDetector::Initialize() {
  // 1. 속성 초기화
  PrepareAttributes();
  // 2. OpenAPI URI 등록 (HTTP 엔드포인트)
  RegisterOpenAPIURI();
  // 3. 기타 초기화
  return Component::Initialize();
}

bool HandDetector::Finalize() {
  // 리소스 정리
  return Component::Finalize();
}

// 모든 이벤트는 이 함수 하나로 들어옴
bool HandDetector::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest:
      // HTTP 요청 처리
      break;
    case static_cast<int32_t>(IPStreamProviderManagerVideoRaw::EEventType::eVideoRawData):
      // 비디오 프레임 처리 (추론)
      break;
    // ... 기타 이벤트
  }
  return true;
}
```

---

## 4. HTTP API (OpenAPI) — 웹 UI 연동

### 4.1 URI 등록 (필수)

`Initialize()`에서 반드시 호출해야 합니다. **GET과 POST를 모두 등록**해야 합니다.

```cpp
void HandDetector::RegisterOpenAPIURI() {
  Vector<String> methods;
  methods.push_back("GET");    // ← 반드시 포함
  methods.push_back("POST");

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(
      String("/configuration"), GetInstanceName(), methods);

  SendNoReplyEvent("AppDispatcher",
      static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, uriRequest);
}
```

**등록하지 않은 HTTP method로 요청이 들어오면 500 에러가 반환됩니다.**

### 4.2 HTTP 요청 처리

`ProcessAEvent()`에서 `eHttpRequest` 이벤트를 받아 처리합니다.

```cpp
case (int32_t)IAppDispatcher::EEventType::eHttpRequest: {
  auto param = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());

  // 요청 정보 추출
  string path_info = param->GetFCGXParam("PATH_INFO").c_str();
  string method = param->GetMethod();            // "GET" 또는 "POST"
  string query = param->GetFCGXParam("QUERY_STRING").c_str();  // GET 파라미터
  string body = param->GetRequestBody();          // POST 본문

  // 응답
  param->SetStatusCode(200);
  param->SetResponseBody("{\"status\":\"ok\"}");
  break;
}
```

### 4.3 웹 UI (index.html)에서의 API 호출

```
GET  /opensdk/{app_id}/configuration?mode=info   → 상태 조회
GET  /opensdk/{app_id}/configuration?mode=log    → 디버그 로그 조회
POST /opensdk/{app_id}/configuration             → body: {"mode":"start"} 등
```

`app_id`는 URL에서 자동 추출됩니다: `AppName` 파라미터 또는 URL 경로에서 추출.

---

## 5. 주요 Output Event 패턴 (코드 → System Container)

### 5.1 Write Event Log (카메라 웹 UI 이벤트 로그탭에 출력)

```cpp
void HandDetector::SendWriteEventLog(const std::string& message) {
  auto* log = new Log(
      Log::LogType::EVENT_LOG,
      Log::LogDetailType::EVENT_OPENAPP,
      0,
      time(NULL),
      String(message.c_str())   // Platform_Std_Refine::String
  );
  SendNoReplyEvent("LogManager", (int32_t)ILogManager::EEvent::eWrite, 0, log);
}
```

**주의:** `Log` 클래스는 `new ("Log") Log(...)` 형태의 placement new를 **지원하지 않습니다**. 반드시 `new Log(...)` 형태로 생성해야 합니다.

필요한 매니페스트 등록:
- 인스턴스: `ReceiverNames`에 `{"SymbolName": "LogManager", "RealName": "2009004"}`
- LCM: `RemoteComponentNames`에 `{"LocalComponentName": "2009004", "RemoteComponentName": "2009004"}`


### 5.2 Send Metadata (ONVIF 메타데이터 → VMS)

```cpp
void HandDetector::SendMetadata(class StreamMetadata&& metadata) {
  auto builder = IPMetadataManager::StreamMetadataRequest::builder();
  builder.set_stream_metadata(std::forward<class StreamMetadata>(metadata));
  auto metadata_request = reinterpret_cast<IPMetadataManager::StreamMetadataRequest*>(builder.build());
  SendNoReplyEvent("MetadataManager",
      static_cast<int32_t>(IMetadataManager::EEventType::eRequestMetadata), 0, metadata_request);
}
```

필요한 매니페스트 등록:
- 인스턴스: `ReceiverNames`에 `{"SymbolName": "MetadataManager", "RealName": "MetadataManager_${APPCHANNEL}"}`
- LCM: `RemoteComponentNames`에 해당 매핑


### 5.3 Alarm Output

```cpp
RelayInstance::Builder builder;
builder.SetAction("on").SetChannel(atoi("1")).SetDuration(1);
SendReplyEvent("ConfigurableAlarmOut",
    static_cast<int32_t>(IConfigurableAlarmOut::EEvent::eRelayRequest), 0,
    new ("RelayRequest") IConfigurableAlarmOut::RelayRequest(builder.Build()));
```

---

## 6. 주요 Input Event (System Container → 코드)

### 6.1 비디오 프레임 수신

```cpp
case static_cast<int32_t>(IPStreamProviderManagerVideoRaw::EEventType::eVideoRawData): {
  auto blob = event->GetBlobArgument();
  event->ClearBaseObjectArgument();

  std::pair<std::variant<BaseObject*, char*>, uint64_t> ret(
      (char*)blob.GetRawData(), blob.GetSize());

  IPVideoFrameRaw* raw_frame = new ("GetImage") IPLVideoFrameRaw();
  raw_frame->DeserializeBaseObject(raw_frame, ret);

  std::shared_ptr<RawImage> img(raw_frame->GetRawImage());
  if (img) {
    // img로 추론 수행
  }
  blob.ClearResource();
  break;
}
```

필요한 매니페스트 등록:
- 인스턴스 `SourceNames`: `{"Source": "SPMgrVideoRaw_${APPCHANNEL}", "GroupName": "GroupSPMgrVideoRaw2"}`
- LCM: `SPMgrVideoRaw_${APPCHANNEL}` 매핑

---

## 7. placement new 규칙

SDK 클래스마다 `new ("ClassName") ClassName(...)` 지원 여부가 다릅니다.

| 클래스 | 생성 방식 | 예시 |
|--------|----------|------|
| Log | `new Log(...)` | placement new 미지원 |
| SerializableString | `new ("") SerializableString(...)` | placement new 지원 |
| NetworkConfig | `new ("NetworkConfig") NetworkConfig(...)` | placement new 지원 |
| NetworkBufferData | `new ("NetworkBufferData") NetworkBufferData(...)` | placement new 지원 |
| RelayRequest | `new ("RelayRequest") IConfigurableAlarmOut::RelayRequest(...)` | placement new 지원 |
| OpenAPIRegistrar | `new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(...)` | placement new 지원 |
| Classification(컴포넌트) | `new("Classification") Classification()` | create_component 내부 |

**확인 방법:** SDK 공식 샘플 코드에서 해당 클래스의 생성 방식을 확인합니다.

---

## 8. ReceiverNames 등록 레퍼런스 (자주 사용하는 것)

| 기능 | SymbolName (코드용) | RealName (매핑키) | LCM LocalComponentName |
|------|---------------------|-------------------|----------------------|
| HTTP API | AppDispatcher | AppDispatcher | (앱 내부, LCM 불필요) |
| 비디오 프레임 | - | - | SPMgrVideoRaw_${APPCHANNEL} |
| 메타데이터 전송 | MetadataManager | MetadataManager_${APPCHANNEL} | MetadataManager_${APPCHANNEL} |
| 이벤트 로그 | LogManager | 2009004 | 2009004 |
| 알람 출력 | ConfigurableAlarmOut | ConfigurableAlarmOut | ConfigurableAlarmOut |
| OpenPlatform | OpenPlatform | EComponents::eOpenPlatform | OpenPlatform |
| 네트워크 매니저 | AppNetworkManager | AppNetworkManager | AppNetworkManager |
| MetaFrame 스키마 | Stub::Dispatcher::OpenSDK | OpenSDKCGIDispatcher | Stub::Dispatcher::OpenSDK |

---

## 9. 빌드 및 배포

### 9.1 빌드 (.env 파일 사용)

프로젝트 루트에 `.env` 파일 생성:

```
APP_NAME=hand_detector
SDK_VER=25.04.09
SOC=wn9
```

```bash
docker compose up          # 빌드 + 패키징
docker compose down --remove-orphans  # 정리
```

결과물: `hand_detector.cap`

### 9.2 카메라 설치

```bash
opensdk_install -a hand_detector -i 192.168.2.80 -u admin -w {password}
```

또는 카메라 웹 UI > Setup > Open Platform 에서 .cap 파일 업로드.

### 9.3 서명 인증서

Docker 이미지 내 `/opt/opensdk/signature/`에 `AppTest.crt`, `AppTest.key`가 있어야 `opensdk_packager`가 동작합니다.

---

## 10. 디렉토리 구조

```
hand_detector/
├── .env                           # 빌드 환경변수
├── docker-compose.yml             # 빌드 자동화
├── config/
│   └── app_manifest.json          # 앱 이름, 버전, 권한
├── app/
│   ├── html/
│   │   └── index.html             # 웹 UI (Go App 페이지)
│   ├── src/
│   │   ├── PLifeCycleManagermanifest.json       # LCM (컨테이너 레벨)
│   │   ├── app_dispatcher/
│   │   │   └── manifests/
│   │   │       └── AppDispatcher_manifest_instance_0.json
│   │   └── classification/
│   │       ├── classification.cc               # 메인 로직
│   │       ├── includes/
│   │       │   └── classification.h
│   │       └── manifests/
│   │           └── Classification_manifest_instance_0.json  # 인스턴스 매니페스트
│   └── res/
│       └── models/                # NPU 모델 파일 (.bin)
└── storage/
    └── settings/                  # 런타임 속성 파일
```

---

## 11. 검증 체크리스트

개발 후 카메라에 설치하기 전에 아래 항목을 확인하십시오.

**매니페스트 체크:**
- [ ] LCM의 `SchedulerNames`에 `"EComponents::eScheduler1"`이 포함되어 있는가
- [ ] 인스턴스 매니페스트의 `SchedulerName`이 LCM `SchedulerNames`에 정의되어 있는가
- [ ] `SendNoReplyEvent()`로 이벤트를 보내는 모든 대상이 `ReceiverNames`에 등록되어 있는가
- [ ] `ReceiverNames`의 `RealName` 값이 LCM `RemoteComponentNames`의 `LocalComponentName`과 일치하는가
- [ ] `SourceNames`에 등록된 모든 Source가 LCM에 매핑되어 있는가

**코드 체크:**
- [ ] `RegisterOpenAPIURI()`에서 GET과 POST 모두 등록했는가
- [ ] `Log` 객체 생성 시 `new Log(...)` 형태를 사용하고 있는가 (`new ("Log")` 아님)
- [ ] `ProcessAEvent()`에서 처리하지 않는 이벤트는 `base::ProcessAEvent(event)`로 넘기고 있는가
- [ ] `create_component` / `destroy_component`가 `extern "C"`로 정의되어 있는가

**동작 검증 (카메라 설치 후):**
- [ ] 카메라 웹 UI > Open Platform 에서 앱 Status가 "Running"인가
- [ ] 카메라 웹 UI > 이벤트 로그탭에 커스텀 로그가 출력되는가
- [ ] Go App 페이지에서 `mode=info` GET 요청에 정상 JSON 응답이 오는가
- [ ] Go App 페이지에서 `mode=start`/`mode=stop` POST 요청이 정상 동작하는가

---

## 12. 알려진 함정 (Known Pitfalls)

1. **라우팅 실패는 에러를 발생시키지 않습니다.** ReceiverNames나 LCM 매핑이 틀리면 이벤트가 조용히 사라집니다. 디버깅이 매우 어려우므로 매니페스트를 먼저 확인하십시오.

2. **OpenAPI URI 등록 시 method를 빠뜨리면 500 에러가 반환됩니다.** GET을 빠뜨리고 POST만 등록하면, 웹 UI에서 GET 요청이 전부 500이 됩니다.

3. **LogManager의 RealName은 `"2009004"`입니다.** 문자열 "LogManager"가 아닙니다. 이것은 `LifeCycleManagerInterface::EReceivers::eLogManager`의 enum 값입니다.

4. **SchedulerNames 형식은 공식 샘플을 따르십시오.** `"eScheduler1"`이 아니라 `"EComponents::eScheduler1"`입니다.

5. **Docker 경로에 한글이나 공백이 있으면 빌드가 실패합니다.** 반드시 영문 경로를 사용하십시오.

6. **OneDrive 동기화 경로에서 작업하면 Docker I/O가 극도로 느려집니다.** 로컬 경로를 사용하십시오.
