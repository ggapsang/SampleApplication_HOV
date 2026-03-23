# Issue Report

## 현상

- URL: `http://192.168.5.60/home/setup/opensdk/html/hand_detector_C3boe/index.html?AppName=hand_detector_C3boe`
- 모든 API 버튼 클릭 시 응답: `{"Response":"Fail","Error":{"Code":500,"Details":"Unknown Error"}}`
- 로그 패널: `(로그 없음 - 앱이 아직 시작되지 않았거나 mode=log 엔드포인트 미응답)`
- 카메라 웹 UI 이벤트 로그탭: `HandDetector Initialize OK` 등 커스텀 로그 없음

---

## 현재 코드 상태 (사실)

### index.html
- 파일: `app/html/index.html`
- API endpoint 구성:
  ```js
  const base_uri = `.../opensdk/${app_id}/configuration`;
  // GET ?mode=log → debug_log_ 반환
  // GET ?mode=info → 상태 JSON 반환
  // POST mode=start/stop → run_flag_ 변경
  ```
- 페이지 로드 시 자동으로 `doInfo()`, `fetchLogs()` 호출

### classification.cc
- 파일: `app/src/classification/classification.cc`
- HTTP PATH_INFO 체크: `if (path_info == "/configuration")`
- GET/POST 모두 `HandleStreamRequest()` 호출
- `mode=` 추출 순서: QUERY_STRING → body JSON
- 구현된 mode 핸들러: `log`, `clear`, `start`, `stop`, `info`, `config`
- `mode=log` 응답: `debug_log_` 벡터를 JSON 배열 문자열로 반환
- `AppendLog()`: Initialize()/Start()/Finalize()/각 mode 호출 시 `debug_log_`에 추가

### classification.h
- `std::vector<std::string> debug_log_` — private 멤버
- `void AppendLog(const std::string& msg)` — protected 선언

### Classification_manifest_instance_0.json
```json
{
  "InstanceName": "HandDetector",
  "SchedulerName": "HandDetectorScheduler",
  "ReceiverNames": [
    "AppDispatcher",
    { "SymbolName": "LogManager", "RealName": "2009004" },
    "SPMgrVideoRaw_0",
    "OpenPlatform",
    "AppNetworkManager",
    "ConfigurableAlarmOut"
  ],
  "SourceNames": [
    { "Source": "SPMgrVideoRaw_${APPCHANNEL}", "GroupName": "GroupSPMgrVideoRaw2" },
    { "Source": "OpenPlatform", "GroupName": "AppMessage" },
    { "Source": "OpenPlatform", "GroupName": "SettingChange" }
  ],
  "Channel": "${APPCHANNEL}",
  "SettingPath": "../storage/settings/"
}
```

### PLifeCycleManagermanifest.json
```json
{
  "ContainerName": "hand_detector",
  "SchedulerNames": ["eScheduler1", "HandDetectorScheduler"],
  "RemoteComponentNames": [
    { "LocalComponentName": "Stub::Dispatcher::OpenAPI", "RemoteComponentName": "OpenAPIDispatcher" },
    { "LocalComponentName": "RuleManager", "RemoteComponentName": "RuleManager" },
    { "LocalComponentName": "SPMgrVideoRaw_${APPCHANNEL}", "RemoteComponentName": "SPMgrVideoRaw_${APPCHANNEL}" },
    { "LocalComponentName": "Stub::Dispatcher::OpenSDK", "RemoteComponentName": "OpenSDKCGIDispatcher" },
    { "LocalComponentName": "MetadataManager_${APPCHANNEL}", "RemoteComponentName": "MetadataManager_${APPCHANNEL}" },
    { "LocalComponentName": "ConfigurableAlarmOut", "RemoteComponentName": "ConfigurableAlarmOut" },
    { "LocalComponentName": "SRMgrVideo_${APPCHANNEL}", "RemoteComponentName": "SRMgrVideo_${APPCHANNEL}" },
    { "LocalComponentName": "2009004", "RemoteComponentName": "2009004" },
    { "LocalComponentName": "AppNetworkManager", "RemoteComponentName": "AppNetworkManager" }
  ]
}
```

### AppDispatcher_manifest_instance_0.json
```json
{
  "Instance": {
    "InstanceName": "AppDispatcher",
    "ReceiverNames": ["Stub::Dispatcher::OpenAPI", "HandDetector"],
    "SourceNames": [{ "Source": "Stub::Dispatcher::OpenAPI", "GroupName": "OpenSDK::hand_detector::Dispatcher" }],
    "Channel": "${APPCHANNEL}"
  }
}
```

---

## 500 에러 발생 지점

HTTP 요청이 `/configuration` endpoint에 도달하지 못하고 있거나,
도달하더라도 `HandleStreamRequest()`가 `false`를 반환하고 SDK 프레임워크가 500으로 처리하는 것으로 보임.

`HandleStreamRequest()`에서 `return false`를 반환하는 경우:
- `mode == "config"` && body 비어있을 때 → 400 반환 후 `return false`
- `mode == "config"` && JSON parse 오류 → 400 반환 후 `return false`
- mode가 `log/clear/start/stop/info/config` 중 어느 것도 아닐 때 → 400 반환 후 `return false`

---

## 확인이 필요한 사항

1. **500 응답 주체**: 카메라 웹 프레임워크(Nginx/CGI)가 반환하는 것인지, SDK AppDispatcher가 반환하는 것인지 불명
2. **요청 경로**: `?mode=log`로 GET 요청 시 QUERY_STRING이 실제로 `mode=log`로 전달되는지 확인 필요
3. **AppDispatcher SchedulerName**: 인스턴스 매니페스트에 `"SchedulerName": "HandDetectorScheduler"`이지만 AppDispatcher 매니페스트에는 `"SchedulerName": "eScheduler1"` — AppDispatcher가 HandDetector를 올바른 스케줄러에서 찾는지 확인 필요
4. **현재 카메라에 설치된 버전**: 위 코드 변경(AppendLog 추가, mode=log 핸들러)이 카메라에 재설치되었는지 여부
