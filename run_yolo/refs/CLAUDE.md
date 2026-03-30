# OpenSDK Hand Detector — 개발 핵심 지식

> 타겟: PNV-A9082RZ (WN9), SDK 25.04.09
> 이 파일은 실제 디버깅을 통해 확인된 사실만 기록합니다.

---

## 1. 500 에러 근본 원인 (해결 완료)

**원인**: Instance manifest `SourceNames`에 `OpenPlatform` 구독이 있었지만 LCM `RemoteComponentNames`에 `OpenPlatform` 매핑이 없었음 → 컴포넌트 초기화 실패 → 모든 HTTP 요청 500

**해결**:
- `SourceNames`에서 OpenPlatform 항목 전부 제거
- OpenPlatform 이벤트(eNetworkSettingChanged 등)는 SDK가 자동 전달 — 명시적 구독 불필요

---

## 2. 매니페스트 필수 규칙 (검증 완료)

### SchedulerNames
```json
"SchedulerNames": ["EComponents::eScheduler1", "HandDetectorScheduler"]
```
- `"eScheduler1"` (prefix 없음) 는 **작동 안 함**
- 반드시 `"EComponents::eScheduler1"` 사용
- LCM과 AppDispatcher 매니페스트가 **동시에** 같은 값이어야 함 (하나만 바꾸면 앱 불기동)

### AppDispatcher 매니페스트
```json
"SchedulerName": "EComponents::eScheduler1",
"Channel": 0,
"ModelPath": "",
"SettingPath": ""
```
- Channel: `"${APPCHANNEL}"` **아님**, 반드시 `0` (정수)

### Instance 매니페스트 SourceNames 규칙
| Source | LCM 매핑 필요 | 비고 |
|--------|--------------|------|
| `SPMgrVideoRaw_${APPCHANNEL}` | ✅ 필요 | video frame 수신 |
| `OpenPlatform` | ❌ 불필요/금지 | SDK 자동 전달, LCM 매핑 없음 |

### Instance 매니페스트 Channel
```json
"Channel": 0
```
- `"${APPCHANNEL}"` 문자열이 아닌 정수 `0`

### LCM RemoteComponentNames 최소 구성 (HTTP + 로그)
```json
{"LocalComponentName": "Stub::Dispatcher::OpenAPI", "RemoteComponentName": "OpenAPIDispatcher"},
{"LocalComponentName": "2009004", "RemoteComponentName": "2009004"}
```
- SPMgrVideoRaw 등은 Phase 2(영상 처리) 구현 시 추가

---

## 3. HTTP API 규칙 (검증 완료)

### RegisterOpenAPIURI — POST만 등록
```cpp
methods.push_back("POST");  // GET 추가하면 오동작 가능
```

### Initialize() 순서 — RegisterOpenAPIURI 반드시 먼저
```cpp
bool HandDetector::Initialize() {
  RegisterOpenAPIURI();           // 1. FIRST
  PrepareAttributes(...);         // 2.
  ParseManifest(...);             // 3.
  // ...
  return Component::Initialize(); // 반드시 base 호출
}
```
- `Component::Initialize()` 미호출 시 SDK 프레임워크 등록 불완전

### index.html API 호출 — 모두 POST + JSON body
```js
fetch(base_uri, {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ mode: mode })
})
```
- mode 추출 순서 (C++): QUERY_STRING → body JSON

---

## 4. Log 관련 규칙 (검증 완료)

### Log 객체 생성
```cpp
// ✅ 올바름
auto* log = new Log(Log::LogType::EVENT_LOG, Log::LogDetailType::EVENT_OPENAPP,
    0, time(NULL), Platform_Std_Refine::String(message.c_str()));

// ❌ 틀림 — Log는 placement new 미지원
auto* log = new ("Log") Log(...);
```

### LogManager 매핑
- 코드: `SendNoReplyEvent("LogManager", ...)`
- Instance ReceiverNames: `{"SymbolName": "LogManager", "RealName": "2009004"}`
- LCM: `{"LocalComponentName": "2009004", "RemoteComponentName": "2009004"}`
- `"LogManager"` 문자열로 LCM에 넣으면 **작동 안 함**

### 이벤트 로그 (카메라 웹 UI 이벤트탭)
```cpp
SendNoReplyEvent("LogManager", (int32_t)ILogManager::EEvent::eWrite, 0, log);
```

### 디버그 인메모리 로그 (HTTP mode=log로 조회)
```cpp
void HandDetector::AppendLog(const std::string& msg);  // debug_log_ 벡터에 추가
```
- `mode=log` POST → `debug_log_` 배열 반환
- `mode=clear` POST → 로그 초기화
- 최대 200개, FIFO

---

## 5. 참조 샘플 앱 비교

| 기능 | 참조 샘플 | 위치 |
|------|-----------|------|
| HTTP API + 이벤트 로그 | **write_event_log** | `../write_event_log/` |
| 영상 처리 + NPU 추론 | **run_neural_network** | `../run_neural_network/` |

- HTTP 라우팅 문제 → write_event_log 구조와 1:1 비교
- NPU/영상 문제 → run_neural_network 구조 참조

---

## 6. 현재 구현 상태 (Phase 1 완료)

| 기능 | 상태 |
|------|------|
| HTTP API (start/stop/info/config/log/clear/test) | ✅ 동작 |
| 카메라 이벤트 로그 WriteEventLog | ✅ 동작 |
| 인메모리 디버그 로그 | ✅ 동작 |
| 웹 UI (index.html) | ✅ 동작 |
| NPU 모델 로딩 | ⬜ Phase 2 |
| 영상 프레임 수신 (SPMgrVideoRaw) | ⬜ Phase 2 |
| YOLO 추론 파이프라인 | ⬜ Phase 3 |
| MQTT 알람 | ⬜ Phase 4 |

---

## 7. 빌드 및 배포

```bash
# .env 파일
APP_NAME=hand_detector
SDK_VER=25.04.09
SOC=wn9

# 빌드
docker compose up
docker compose down --remove-orphans

# 설치
opensdk_install -a hand_detector -i 192.168.2.60 -u admin -w {password}
```

결과물: `hand_detector.cap`

---

## 8. 디렉토리 구조 (핵심 파일)

```
run_yolo/
├── CLAUDE.md                          ← 이 파일
├── OpenSDK_Developer_Reference.md     ← SDK 아키텍처 레퍼런스
├── .env                               ← 빌드 환경변수
├── docker-compose.yml
├── config/
│   └── app_manifest.json              ← AppName: "hand_detector"
└── app/
    ├── html/index.html                ← 웹 UI
    ├── res/models/
    │   └── AppDispatcher_manifest_instance_0.json
    └── src/
        ├── PLifeCycleManagermanifest.json   ← LCM (컨테이너 레벨)
        └── classification/
            ├── classification.cc            ← 메인 로직
            ├── includes/classification.h
            └── manifests/
                ├── Classification_manifest.json
                ├── Classification_manifest_instance_0.json
                └── Classification_default_attribute_0.json
```
