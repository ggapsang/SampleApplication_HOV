# Wisenet OpenSDK 샘플 애플리케이션 종합 분석

> **분석 일자:** 2026-03-10  
> **대상:** `/opt/opensdk_work/SampleApplication/` 내 20개 샘플 앱 전체

---

## 목차

1. [개요](#1-개요)
2. [공통 아키텍처 패턴](#2-공통-아키텍처-패턴)
3. [각 샘플 애플리케이션 상세 분석](#3-각-샘플-애플리케이션-상세-분석)
4. [빌드 시스템 분석](#4-빌드-시스템-분석)
5. [SDK API 레퍼런스](#5-sdk-api-레퍼런스)
6. [앱 분류 및 카테고리](#6-앱-분류-및-카테고리)
7. [권한 모델](#7-권한-모델)
8. [결론 및 개발 가이드](#8-결론-및-개발-가이드)

---

## 1. 개요

본 워크스페이스에는 **Wisenet OpenSDK** 기반의 카메라 앱 개발을 위한 20개 샘플 애플리케이션이 포함되어 있다. 이 샘플들은 알람 출력, 영상 분석, 딥러닝 추론, 이미지 처리, 메타데이터 전송, 시스템 이벤트 핸들링 등 OpenSDK의 다양한 기능을 시연한다.

### 전체 앱 목록

| # | 앱 이름 | 핵심 기능 | 카테고리 |
|---|---------|-----------|----------|
| 1 | `alarm_output` | 릴레이 스위치 ON/OFF 제어 | 출력 제어 |
| 2 | `analytics_input_event` | 모션/객체 감지 이벤트 수신 | 이벤트 입력 |
| 3 | `deepstream_raw_image` | NVIDIA DeepStream AI 영상 처리 | AI/딥러닝 |
| 4 | `display_image` | TCP 서버를 통한 이미지 스트리밍 | 네트워크 |
| 5 | `display_image_opencv` | OpenCV 엣지 검출 + 이미지 스트리밍 | 영상 처리 |
| 6 | `dockerized_app` | FTP 테스트 서버 (Alpine FTP) | 인프라/테스트 |
| 7 | `dynamic_event` | 동적 이벤트 스키마 정의 및 메타데이터 전송 | 메타데이터 |
| 8 | `openblas_sample` | OpenBLAS 크로스 컴파일 및 선형대수 연산 | 연산/라이브러리 |
| 9 | `raw_audio` | PCM 원시 오디오 프레임 수신 및 처리 | 오디오 |
| 10 | `run_neural_network` | 신경망 모델 추론 실행 | AI/딥러닝 |
| 11 | `sdcard_input_event` | SD카드 삽입/제거 이벤트 모니터링 | 이벤트 입력 |
| 12 | `sdcard_output_event` | SD카드 시작/중지/포맷/독점 모드 제어 | 저장소 제어 |
| 13 | `send_metadata` | 이벤트/프레임 메타데이터 전송 | 메타데이터 |
| 14 | `set_osd_message` | 영상 위 OSD 텍스트 오버레이 설정 | 영상 처리 |
| 15 | `setting_changed_input_event` | 카메라 설정 변경 감지 (12+ 이벤트) | 이벤트 입력 |
| 16 | `snapshot_jpeg` | JPEG 스냅샷 캡처 및 저장 | 영상 처리 |
| 17 | `test_sunapi` | SUNAPI 인터페이스 호출 테스트 | API 테스트 |
| 18 | `upload_file_input_event` | 파일 업로드 알림 수신 | 이벤트 입력 |
| 19 | `upload_file_output_event` | FTP 업로드 / 이메일 발송 | 출력 제어 |
| 20 | `write_event_log` | 시스템 이벤트 로그 기록 | 로깅 |

---

## 2. 공통 아키텍처 패턴

### 2.1 프로젝트 디렉토리 구조

모든 샘플 앱(dockerized_app 제외)은 동일한 디렉토리 구조를 따른다:

```
<app_name>/
├── docker-compose.yml          # Docker 빌드 컨테이너 설정
├── readme.adoc                 # AsciiDoc 형식 문서
├── app/
│   ├── CMakeLists.txt          # 루트 CMake 빌드 설정
│   ├── toolchain.cmake         # 크로스 컴파일 툴체인
│   ├── bin/                    # 실행 바이너리 (LCM에서 복사)
│   ├── html/                   # 웹 UI 파일
│   ├── includes/               # 헤더 파일 (일부 앱)
│   ├── res/                    # 리소스 파일
│   └── src/
│       ├── CMakeLists.txt      # 소스 빌드 설정 (LCM/디스패처 복사)
│       ├── PLifeCycleManagermanifest.json
│       └── sample_component/
│           ├── CMakeLists.txt  # 컴포넌트 MODULE 라이브러리 빌드
│           ├── sample_component.cc  # 핵심 소스 코드
│           └── manifests/      # 컴포넌트 매니페스트
└── config/
    └── app_manifest.json       # 앱 메타데이터/권한 선언
```

### 2.2 컴포넌트 기반 아키텍처

모든 앱은 `Component` 기반 클래스를 상속하며, 다음 생명주기 패턴을 따른다:

```cpp
class SampleComponent : Component {
    // === 생명주기 메서드 ===
    bool Initialize();          // URI 등록, 상태 초기화
    bool ProcessAEvent(Event*); // 이벤트 디스패치 (핵심 로직)
    bool Finalize();            // [선택] 정리 작업

    // === HTTP 처리 ===
    void RegisterOpenAPIURI();  // HTTP 엔드포인트 등록
    bool ParseHttpEvent(Event*); // HTTP 요청 파싱 및 응답

    // === 팩토리 ===
    static Component* CreateComponent(); // 컴포넌트 인스턴스 생성
};
```

### 2.3 이벤트 기반 비동기 통신

```cpp
bool ProcessAEvent(Event* event) {
    switch (event->GetType()) {
        case IAppDispatcher::EEventType::eHttpRequest:
            return ParseHttpEvent(event);
        case SomeInterface::EEventType::eSomeEvent:
            // 이벤트별 핸들러
            break;
        default:
            return Component::ProcessAEvent(event);
    }
    return true;
}
```

**핵심 통신 패턴:**
- `SendReplyEvent()` — 요청-응답 패턴 (비동기)
- `SendReplyEventWait()` — 동기 블로킹 요청
- `SendNoReplyEvent()` — 단방향 Fire-and-forget 이벤트

### 2.4 HTTP API 게이트웨이 패턴

모든 앱은 `OpenAppSerializable`을 통해 HTTP 요청을 처리한다:

```cpp
bool ParseHttpEvent(Event* event) {
    if (event->IsReply()) return true;

    auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
    auto path = oas->GetFCGXParam("PATH_INFO");
    auto method = oas->GetMethod();

    if (path == "/configuration" && method == "POST") {
        // JSON 바디 파싱
        JsonUtility::JsonDocument doc;
        doc.Parse(oas->GetRequestBody());
        if (doc.HasParseError()) {
            oas->SetStatusCode(400);
            oas->SetResponseBody("Bad Request");
            return false;
        }
        // 비즈니스 로직 처리
        oas->SetStatusCode(200);
        oas->SetResponseBody(responseJson);
    }
    return true;
}
```

### 2.5 JSON 직렬화 패턴 (RapidJSON)

```cpp
// 요청 파싱
JsonUtility::JsonDocument doc(JsonUtility::Type::kObjectType);
doc.Parse(body);
std::string value = doc["field"].GetString();

// 응답 빌드
JsonUtility::JsonDocument resp(JsonUtility::Type::kObjectType);
auto& alloc = resp.GetAllocator();
resp.AddMember("key", JsonUtility::JsonValue().SetString(val.c_str(), alloc), alloc);
rapidjson::StringBuffer buf;
rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
resp.Accept(writer);
std::string result = buf.GetString();
```

### 2.6 커스텀 메모리 할당

임베디드 환경을 위한 풀 기반 메모리 관리:

```cpp
// 팩토리 함수에서 할당자 설정
Component::allocator = decltype(Component::allocator)(mem_manager);
Event::allocator = decltype(Event::allocator)(mem_manager);
return new ("SampleComponent") SampleComponent();
```

### 2.7 로깅 패턴

```cpp
DebugLog("%s %s:%d <메시지>", GetObjectName(), __func__, __LINE__);
```

---

## 3. 각 샘플 애플리케이션 상세 분석

---

### 3.1 alarm_output — 알람 출력 릴레이 제어

**목적:** 카메라의 릴레이 스위치를 ON/OFF로 제어하는 알람 출력 기능을 시연한다.

**소스 파일:** `alarm_output/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IConfigurableAlarmOut::RelayRequest` — 릴레이 명령 전송
- `IAppDispatcher` — HTTP 요청 처리

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| POST | `/configuration` | 알람 파라미터 설정 (mode, channel, duration) |

**요청 파라미터:**
- `mode`: 작동 모드
- `channel`: 알람 채널 번호
- `duration`: 지속 시간 (0~15초, 유효성 검증 포함)

**의존 라이브러리:** `dispatcher_serialize`, `app_dispatcher`, `configurable_alarm_out`

---

### 3.2 analytics_input_event — 분석 이벤트 수신

**목적:** 모션 감지, 객체 감지 등 영상 분석 이벤트를 수신하여 저장하고 조회한다.

**소스 파일:** `analytics_input_event/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IAnalyticsDetector::EEventType::eNotifyEventOccured` — 분석 이벤트 콜백
- `IAppDispatcher::eHttpRequest` — HTTP 요청 처리

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| GET | `/analytics` | 수신된 분석 이벤트 JSON 배열 반환 |

**상태 관리:** `std::deque`로 최근 10개 이벤트를 관리  
**이벤트 데이터:** type, message_time, state, channel  
**채널 타입:** Single (단일 채널 전용)

---

### 3.3 deepstream_raw_image — NVIDIA DeepStream AI 영상 처리

**목적:** NVIDIA DeepStream SDK를 이용한 실시간 AI 객체 감지를 수행하고, 감지 결과를 메타데이터로 전송한다.

**소스 파일:**
- `deepstream_raw_image/app/src/sample_component/sample_component.cc`
- `deepstream_raw_image/app/src/sample_component/deepstream_handle.cc`

**사용 SDK API:**
- `IPStreamProviderManagerVideoRaw` — Raw 영상 스트림 처리
- `IMetadataManager` — 메타데이터 전송
- NVIDIA DeepStream API (`gstnvdsmeta`, `nvbufsurface`)

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| GET | `/stream` | 스트림 시작/중지/상태 (`mode=start/stop/view`) |
| GET | `/profile` | 비디오 프로필 조회 (via SUNAPI) |

**핵심 기능:**
- 1920x1080, 10fps, 1.5Mbps 비디오 프로필 지원
- 차량(class 0) 및 사람(class 2) 객체 감지
- 바운딩 박스와 신뢰도 점수 메타데이터 전송
- DMA-BUF 기반의 제로카피 프레임 전달
- URL 디코딩, 정규식 기반 쿼리 파싱 유틸리티

**요구 사항:** NVIDIA 칩셋, DeepStream 6.3+

---

### 3.4 display_image — TCP 이미지 스트리밍

**목적:** Network Manager를 통해 TCP 서버를 구동하고, 연결된 클라이언트에게 JPEG 이미지를 전송한다.

**소스 파일:** `display_image/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IAppNetworkManager` — TCP 서버 관리

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| POST | `/startserver` | TCP 서버 시작 (포트 지정) |
| POST | `/stopserver` | TCP 서버 중지 |

**핵심 기능:**
- `../res/image.jpg` 파일을 TCP 클라이언트에게 전송
- HTTP 응답 헤더 (Content-Type, Content-Length) 포함

**의존 라이브러리:** `app_network_manager`

---

### 3.5 display_image_opencv — OpenCV 이미지 처리

**목적:** OpenCV를 이용한 이미지 처리(엣지 검출)를 수행하고, 처리된 이미지를 TCP로 스트리밍한다.

**소스 파일:** `display_image_opencv/app/src/sample_component/sample_component.cc`

**사용 라이브러리:**
- OpenCV (`opencv2/opencv.hpp`)
- `IAppNetworkManager` — TCP 서버

**이미지 처리 파이프라인:**
```
원본 JPEG → 그레이스케일 변환 → 가우시안 블러 → Canny 엣지 검출 → JPEG 인코딩 → TCP 전송
```

**HTTP 엔드포인트:** `display_image`와 동일 (`/startserver`, `/stopserver`)

---

### 3.6 dockerized_app — FTP 테스트 서버

**목적:** `upload_file_output_event` 샘플의 FTP 업로드 테스트를 위한 경량 FTP 서버를 제공한다.

**구성:**
```yaml
image: delfer/alpine-ftp-server
ports: 2121:21, 21000-21010:21000-21010
user: one / 1234
```

> **참고:** 이 앱은 OpenSDK 앱이 아닌 Docker 컨테이너 기반의 지원 서비스이다.

---

### 3.7 dynamic_event — 동적 이벤트 스키마 및 메타데이터

**목적:** 커스텀 이벤트 스키마를 정의하고, 이벤트/상태 메타데이터를 전송하는 방법을 시연한다.

**소스 파일:** `dynamic_event/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `I_EventstatusCGIDispatcher` — 이벤트 스키마 등록
- 이벤트/상태 메타데이터 디스패치

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| POST | `/configuration` | mode: "event_status_change" / "dynamic_event" |

**핵심 기능:**
- XML 형식 이벤트 스키마 정의
- PROPRIETARY 및 ONVIF 형식 지원
- 소스/데이터/요소 항목 포함 메타데이터 전송

---

### 3.8 openblas_sample — OpenBLAS 크로스 컴파일

**목적:** ARM v8(64-bit) 타겟을 위한 OpenBLAS 라이브러리 크로스 컴파일 및 선형대수 연산을 시연한다.

**소스 파일:** `openblas_sample/app/src/sample_component/sample_component.cc`

**사용 라이브러리:**
- CBLAS (`cblas_ddot`, `cblas_daxpy`)

**연산 예시:**
- 벡터 내적: `(1,2,3) · (4,5,6) = 32`
- AXPY 연산: `y = α·x + y`

**빌드 특이사항:**
- CMake에서 OpenBLAS 소스 빌드 수행
- `TARGET=ARMV8`, `NO_FORTRAN=1`, `NO_SHARED=1`
- `env_file` 기반 환경 변수 관리

---

### 3.9 raw_audio — PCM 원시 오디오 처리

**목적:** 카메라 마이크에서 캡처된 RAW 오디오 프레임(PCM)을 수신하고 처리한다.

**소스 파일:** `raw_audio/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IPStreamProviderManagerAudioRaw::eAudioRawData` — 오디오 프레임 전달
- `IPAudioFrameRaw` — 오디오 프레임 인터페이스

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| GET | `/sample` | 최근 10개 오디오 프레임 정보 JSON 배열 |

**수집 정보:** timestamp, sample rate, amplitude, codec type (PCM)

---

### 3.10 run_neural_network — 신경망 추론 실행

**목적:** 신경망 모델을 로드하여 분류(classification) 추론을 실행한다.

**소스 파일:** `run_neural_network/app/src/classification/classification.cc`

**사용 SDK API:**
- `CreateNetwork` / `LoadNetwork` / `RunNetwork` — 신경망 API
- 입출력 텐서 관리
- Base64 인코딩 (메타데이터 스키마)

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| POST | `/configuration` | 모델 설정 |

**핵심 기능:**
- 비디오 Raw 프레임에서 추론 수행
- 분류 결과를 메타데이터로 전송
- `app_manifest.json`에서 AppName/AppVersion 참조

**권한:** `SDCard`, `Device`  
**채널 타입:** Single

---

### 3.11 sdcard_input_event — SD카드 이벤트 모니터링

**목적:** SD카드의 삽입/제거 이벤트를 감지하고 상태를 추적한다.

**소스 파일:** `sdcard_input_event/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IDeviceIoManager::eSdcardInserted` — SD카드 삽입 이벤트
- `IDeviceIoManager::eSdcardRemoved` — SD카드 제거 이벤트

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| GET | `/configuration` | SD카드 상태 JSON 반환 |

**상태 관리:** 다중 SD카드 인덱스별 삽입/제거 상태 추적

---

### 3.12 sdcard_output_event — SD카드 제어

**목적:** SD카드의 시작/중지/포맷과 독점(exclusive) 모드를 제어한다.

**소스 파일:** `sdcard_output_event/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IPOpenPlatformManager` — 플랫폼 통합

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| POST | `/startsdcard` | SD카드 시작 |
| POST | `/stopsdcard` | SD카드 중지 |
| POST | `/formatsdcard` | SD카드 포맷 |
| GET | `/getsdcardpath` | 앱의 SD카드 경로 조회 |
| POST | `/setexclusivemode` | 독점 모드 설정 |
| GET | `/getexclusivemodestatus` | 독점 모드 상태 조회 |
| GET | `/stopresult` | 중지 결과 조회 |
| GET | `/formatresult` | 포맷 결과 조회 |
| GET | `/getexclusivemoderesult` | 독점 모드 결과 조회 |

**권한:** `SDCard`  
**비동기 패턴:** 요청/결과 분리 — 작업 결과를 별도 엔드포인트에서 조회

---

### 3.13 send_metadata — 메타데이터 전송

**목적:** 카메라 시스템에 이벤트 메타데이터와 프레임 메타데이터를 전송한다.

**소스 파일:** `send_metadata/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `StreamMetadata` — 통합 메타데이터 컨테이너
- `EventMetadataItem` — 이벤트 메타데이터 (topic, source, data)
- `FrameMetadataItem` — 프레임 메타데이터 (객체, 해상도)
- `IPMetadataManager` — 메타데이터 전달

**HTTP 엔드포인트:**
| 메서드 | 경로 | 모드 | 설명 |
|--------|------|------|------|
| POST | `/configuration` | `event_metadata` | 이벤트 메타데이터 전송 |
| POST | `/configuration` | `frame_metadata` | 프레임 메타데이터 전송 |

**프레임 메타데이터 구조:**
- Object: ID, parent, likelihood, category (Vehicle/Human/Other)
- BoundingBox: left, top, right, bottom
- ONVIF 객체 카테고리 지원

**추가 이벤트:**
- `eRemoteAssociateCompleted` → 최대 해상도 요청
- `eInformAppInfo` → 앱 ID 추출

---

### 3.14 set_osd_message — OSD 텍스트 오버레이

**목적:** 카메라 영상 위에 OSD(On-Screen Display) 텍스트를 오버레이한다.

**소스 파일:** `set_osd_message/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IPStreamRequesterManagerVideo::MultilineOsdItem` — OSD 설정
- `IPStreamRequesterManagerVideo::eExternalMultiLineOsd` — OSD 전달

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| POST | `/configuration` | OSD 파라미터 설정 |

**설정 파라미터:**
- `enable`: 활성화 여부
- `position`: X/Y 좌표
- `font_size`: 글꼴 크기
- `color`: 텍스트 색상
- `transparency`: 투명도
- `message_text`: 표시 텍스트
- `osd_type`: "Title"

**특이사항:** `Finalize()` 구현 — 앱 종료 시 OSD 정리

---

### 3.15 setting_changed_input_event — 설정 변경 감지

**목적:** 카메라 시스템의 다양한 설정 변경 이벤트를 모니터링하고 기록한다.

**소스 파일:** `setting_changed_input_event/app/src/sample_component/sample_component.cc`

**감지하는 설정 변경 이벤트 (12+ 종류):**

| 이벤트 | 설명 |
|--------|------|
| `eNetworkInterfaceSettingChanged` | 네트워크 인터페이스 변경 |
| `eNetworkPortSettingChanged` | 네트워크 포트 변경 |
| `eVideoProfileSettingChanged` | 비디오 프로필 변경 |
| `eAudioSettingChanged` | 오디오 설정 변경 |
| `eImageSettingChanged` | 이미지 설정 변경 |
| `eVideoRotationSettingChanged` | 비디오 회전 변경 |
| `eWiseStreamSettingChanged` | WiseStream 설정 변경 |
| `eExposureAISettingChanged` | Exposure AI 변경 |
| `eRecordSettingChanged` | 녹화 설정 변경 |
| `eLanguageSettingChanged` | 언어 변경 |
| `eTimeSettingChanged` | 시간 설정 변경 |
| `eAnalyticsSettingChanged` | 분석 설정 변경 |

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| GET | `/configuration` | 마지막 변경 시간 및 채널 JSON 반환 |

---

### 3.16 snapshot_jpeg — JPEG 스냅샷 캡처

**목적:** 카메라에서 현재 프레임의 JPEG 스냅샷을 캡처하고 파일로 반환한다.

**소스 파일:** `snapshot_jpeg/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IPOpenPlatformManager::eAppSnapshotJpeg` — JPEG 인코딩 요청

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| POST | `/configuration` | JPEG 스냅샷 요청 (jpeg_path 지정) |

**처리 흐름:**
1. POST 요청으로 `jpeg_path` 수신
2. OpenPlatform에 스냅샷 요청 (`SendReplyEventWait`)
3. 인코딩된 JPEG 파일 읽기
4. `OpenAppResponseType::FILE` 타입으로 바이너리 응답

---

### 3.17 test_sunapi — SUNAPI 인터페이스 테스트

**목적:** SUNAPI Requester를 통해 카메라 내부 API를 호출하고 응답을 파싱한다.

**소스 파일:** `test_sunapi/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `ISunapiRequester` — SUNAPI 요청 인터페이스

**호출 API:** `/stw-cgi/opensdk.cgi?msubmenu=apps&action=view`

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| GET/POST | `/request` | SUNAPI 요청 트리거 |
| GET/POST | `/response` | 저장된 앱 정보 JSON 배열 반환 |

**응답 데이터:** app_id, app_name, status, installed_date, version

**특이사항:** `eReadyForUse` 이벤트 수신 시 자동으로 초기 요청 전송

---

### 3.18 upload_file_input_event — 파일 업로드 알림 수신

**목적:** 외부에서 업로드된 파일의 알림을 수신하고 기록한다.

**소스 파일:** `upload_file_input_event/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IPOpenPlatformManager::eUploadFile` — 파일 업로드 이벤트

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| GET | `/configuration` | 업로드된 파일 목록 JSON 반환 |

**동작:** SUNAPI curl을 통해 `./app` 폴더에 업로드된 파일 정보(앱 이름, 파일명) 수신

---

### 3.19 upload_file_output_event — 파일 업로드/이메일 발송

**목적:** FTP 서버로 파일을 업로드하거나, 이메일로 파일을 발송한다.

**소스 파일:** `upload_file_output_event/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `IPOpenPlatformManager::eAppFtpFileUpload` — FTP 업로드
- `IPOpenPlatformManager::eAppSendEmailNoti` — 이메일 발송

**HTTP 엔드포인트:**
| 메서드 | 경로 | 모드 | 설명 |
|--------|------|------|------|
| POST | `/configuration` | `ftp` | FTP 파일 업로드 |
| POST | `/configuration` | `email` | 이메일 발송 |

**파라미터:**
- 공통: file_type, file_path, event_info, report_name
- 이메일: + subject

**특이사항:** URL 디코딩 지원 (`%xx`, `+`)

---

### 3.20 write_event_log — 시스템 이벤트 로그 기록

**목적:** 앱 이벤트를 카메라 시스템 이벤트 로그에 영구적으로 기록한다.

**소스 파일:** `write_event_log/app/src/sample_component/sample_component.cc`

**사용 SDK API:**
- `ILogManager::eWrite` — 로그 기록
- `Log(LogType::EVENT_LOG, LogDetailType::EVENT_OPENAPP, ...)` — 로그 객체 생성

**HTTP 엔드포인트:**
| 메서드 | 경로 | 설명 |
|--------|------|------|
| POST | `/configuration` | 로그 메시지 기록 |

**추가 이벤트 리스너:** 네트워크/녹화/분석/WiseStream/프로필 설정 변경 감지

---

## 4. 빌드 시스템 분석

### 4.1 Docker 빌드 환경

**표준 패턴 (18개 앱):**
```yaml
services:
  opensdk:
    image: opensdk:${SDK_VER}
    container_name: ${APP_NAME}
    volumes:
      - ./:/opt/${APP_NAME}
    working_dir: /opt/${APP_NAME}
    command: >
      /bin/bash -c "
      cd /opt/${APP_NAME}/app/ &&
      mkdir -p build && cd build &&
      cmake -DSOC=${SOC} .. &&
      make clean && make && make install &&
      cd /opt/${APP_NAME} &&
      opensdk_packager
      "
```

**환경 변수:**
| 변수 | 용도 |
|------|------|
| `SDK_VER` | OpenSDK Docker 이미지 버전 |
| `APP_NAME` | 애플리케이션 이름 |
| `SOC` | 타겟 SoC 칩셋 (크로스 컴파일용) |

**빌드 프로세스:**  
`cmake` → `make clean && make && make install` → `opensdk_packager`

### 4.2 CMake 빌드 구조

**3단계 CMakeLists.txt 구성:**

```
app/CMakeLists.txt (루트)
 ├── toolchain.cmake 포함
 ├── SDK 경로 설정 (WISENET_PLATFORM_RELEASE_PATH, WISENET_PRODUCT_RELEASE_PATH, APP_SUPPORT_PACKAGE)
 ├── 인클루드/링크 디렉토리 설정
 └── add_subdirectory(src)

app/src/CMakeLists.txt (소스)
 ├── add_subdirectory(sample_component)
 ├── AppSupportPackage에서 app_dispatcher 복사
 ├── life_cycle_manager를 opensdk_<app_name>으로 복사/이름변경
 ├── res/, storage/ 디렉토리 생성
 └── PLifeCycleManagermanifest.json 설치

app/src/sample_component/CMakeLists.txt (컴포넌트)
 ├── add_library(sample_component MODULE sample_component.cc)
 ├── 인클루드 디렉토리 설정
 └── install(TARGETS/DIRECTORY) - 라이브러리 및 매니페스트 설치
```

### 4.3 툴체인 설정

```cmake
# toolchain.cmake
if (DEFINED SDK_PATH)
    message("SDK_PATH: ${SDK_PATH}")
else ()
    set(SDK_PATH /opt/opensdk)
endif ()
```

SDK 경로 기본값: `/opt/opensdk`

### 4.4 SDK 의존성 경로

| 경로 | 용도 |
|------|------|
| `${COMMON}/platform/${SOC}` | 플랫폼 SDK (헤더/라이브러리) |
| `${COMMON}/product/${SOC}` | 제품 SDK (헤더/라이브러리) |
| `${COMMON}/AppSupportPackage/${SOC}` | 앱 지원 패키지 (LCM, 디스패처 등) |
| `${COMMON}/lib` | 공통 라이브러리 |

### 4.5 특수 빌드 구성

| 앱 | 특이사항 |
|----|----------|
| `deepstream_raw_image` | `deepstream.cmake` 추가, 엄격한 컴파일 옵션 (`-Wall -Werror`) |
| `openblas_sample` | OpenBLAS 소스 빌드 (ARM v8), `env_file` 사용 |
| `test_sunapi` | `sunapi_requester` 전용 인클루드 경로 |
| `write_event_log` | Docker `environment` 섹션에서 SOC 명시 전달 |

---

## 5. SDK API 레퍼런스

### 5.1 핵심 인터페이스

| 인터페이스 | 사용 앱 수 | 용도 |
|------------|-----------|------|
| `IAppDispatcher` | 19 | HTTP 요청/응답 게이트웨이 |
| `OpenAppSerializable` | 19 | HTTP 요청/응답 래퍼 |
| `IPOpenPlatformManager` | 7 | 플랫폼 통합 (SD카드, 스냅샷, 업로드 등) |
| `ISunapiRequester` | 2 | SUNAPI 내부 API 호출 |
| `IMetadataManager` / `IPMetadataManager` | 3 | 메타데이터 전송 |
| `IAnalyticsDetector` | 1 | 분석 이벤트 수신 |
| `IConfigurableAlarmOut` | 1 | 알람 릴레이 제어 |
| `ILogManager` | 1 | 시스템 로그 기록 |
| `IDeviceIoManager` | 1 | 디바이스 I/O 이벤트 |
| `IAppNetworkManager` | 2 | TCP 네트워크 서버 |
| `IPStreamProviderManagerVideoRaw` | 2 | Raw 비디오 프레임 |
| `IPStreamProviderManagerAudioRaw` | 1 | Raw 오디오 프레임 |
| `IPStreamRequesterManagerVideo` | 1 | OSD 오버레이 |

### 5.2 통신 메서드

| 메서드 | 설명 | 사용 빈도 |
|--------|------|-----------|
| `SendReplyEvent()` | 비동기 요청-응답 | 높음 |
| `SendNoReplyEvent()` | 단방향 이벤트 전송 | 높음 |
| `SendReplyEventWait()` | 동기 블로킹 요청 | 중간 |

### 5.3 JSON 라이브러리

- **RapidJSON** 기반: `JsonUtility::JsonDocument`, `JsonUtility::JsonValue`
- `rapidjson::StringBuffer`, `rapidjson::Writer` — 직렬화
- `JsonDocument::Parse()`, `HasParseError()` — 파싱

---

## 6. 앱 분류 및 카테고리

### 입력(Input) 이벤트 앱

이벤트를 **수신**하여 처리하는 앱:
| 앱 | 이벤트 소스 |
|----|-----------|
| `analytics_input_event` | 영상 분석 (모션/객체 감지) |
| `sdcard_input_event` | SD카드 삽입/제거 |
| `setting_changed_input_event` | 시스템 설정 변경 (12+ 종류) |
| `upload_file_input_event` | 파일 업로드 알림 |
| `raw_audio` | PCM 오디오 프레임 |

### 출력(Output) 이벤트/제어 앱

외부 장치나 시스템을 **제어**하는 앱:
| 앱 | 제어 대상 |
|----|-----------|
| `alarm_output` | 릴레이 스위치 |
| `sdcard_output_event` | SD카드 (시작/중지/포맷) |
| `upload_file_output_event` | FTP 서버 / 이메일 |
| `write_event_log` | 시스템 이벤트 로그 |

### 영상/이미지 처리 앱

| 앱 | 처리 방식 |
|----|-----------|
| `display_image` | TCP 이미지 스트리밍 |
| `display_image_opencv` | OpenCV 엣지 검출 + TCP 스트리밍 |
| `set_osd_message` | OSD 텍스트 오버레이 |
| `snapshot_jpeg` | JPEG 스냅샷 캡처 |

### AI / 딥러닝 앱

| 앱 | 기술 |
|----|------|
| `deepstream_raw_image` | NVIDIA DeepStream 기반 객체 감지 |
| `run_neural_network` | 신경망 모델 추론 (분류) |

### 메타데이터 / API 앱

| 앱 | 기능 |
|----|------|
| `send_metadata` | 이벤트/프레임 메타데이터 전송 |
| `dynamic_event` | 동적 이벤트 스키마 정의 |
| `test_sunapi` | SUNAPI 인터페이스 호출 |

### 기타

| 앱 | 기능 |
|----|------|
| `openblas_sample` | 크로스 컴파일 라이브러리 연동 |
| `dockerized_app` | FTP 테스트 서버 인프라 |

---

## 7. 권한 모델

### app_manifest.json 스키마

```json
{
    "AppName": "<앱 이름>",
    "AppVersion": "<버전>",
    "Permissions": ["<권한1>", "<권한2>"],
    "ChannelType": "<채널 타입>"
}
```

### 권한 사용 현황

| 권한 | 사용 앱 | 설명 |
|------|---------|------|
| (없음) | 17개 앱 | 기본 권한으로 동작 |
| `SDCard` | `sdcard_output_event`, `run_neural_network` | SD카드 접근 |
| `Device` | `run_neural_network` | 디바이스 하드웨어 접근 |

### 채널 타입

| 채널 타입 | 사용 앱 | 설명 |
|-----------|---------|------|
| (미지정) | 18개 앱 | 기본값 |
| `Single` | `analytics_input_event`, `run_neural_network` | 단일 채널 전용 |

---

## 8. 결론 및 개발 가이드

### 8.1 핵심 공통 패턴 요약

1. **컴포넌트 기반 아키텍처:** 모든 앱이 `Component` 클래스를 상속하고, `Initialize()` → `ProcessAEvent()` → `Finalize()` 생명주기를 따름
2. **이벤트 드리븐:** `ProcessAEvent()` 내의 switch-case로 이벤트 타입별 디스패치
3. **HTTP API 게이트웨이:** `RegisterOpenAPIURI()`로 엔드포인트 등록, `OpenAppSerializable`로 요청/응답 처리
4. **JSON 데이터 형식:** RapidJSON 기반 요청 파싱 및 응답 직렬화
5. **컴포넌트 간 통신:** `SendReplyEvent` / `SendNoReplyEvent` / `SendReplyEventWait` 3가지 패턴
6. **Docker 빌드:** `opensdk:${SDK_VER}` 이미지 기반 cmake 크로스 컴파일 → `opensdk_packager` 패키징
7. **표준 디렉토리 구조:** `app/src/sample_component/` 내 단일 `.cc` 파일 패턴
8. **커스텀 메모리 할당:** 임베디드 환경을 위한 풀 기반 메모리 관리

### 8.2 신규 앱 개발 가이드

새로운 OpenSDK 앱을 개발할 때의 최소 체크리스트:

1. 표준 디렉토리 구조 생성 (`app/`, `config/`, `docker-compose.yml`)
2. `config/app_manifest.json`에 앱 이름, 버전, 권한 선언
3. `app/CMakeLists.txt`에서 SDK 경로 및 툴체인 설정
4. `app/src/sample_component/sample_component.cc`에서:
   - `Component` 상속
   - `CreateComponent()` 팩토리 함수 구현
   - `Initialize()`에서 OpenAPI URI 등록
   - `ProcessAEvent()`에서 이벤트 핸들링
   - (필요 시) `Finalize()`에서 정리
5. 매니페스트 JSON 파일 작성 (`PLifeCycleManagermanifest.json`, 컴포넌트 매니페스트)
6. `docker-compose.yml`로 빌드 환경 설정

### 8.3 기술 스택

| 항목 | 기술 |
|------|------|
| 언어 | C++ (C++17 features: `string_view` 등) |
| 빌드 | CMake 3.2.2+ |
| JSON | RapidJSON |
| XML | libxml2 (일부 앱) |
| 영상 처리 | OpenCV, NVIDIA DeepStream |
| 수학 연산 | OpenBLAS (CBLAS) |
| 컨테이너 | Docker Compose |
| 타겟 아키텍처 | ARM v8 (aarch64) |
| SDK | Wisenet OpenSDK |
