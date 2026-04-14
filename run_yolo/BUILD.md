# 빌드 가이드

OpenSDK 앱을 Docker 기반으로 크로스 컴파일하여 Hanwha 카메라용 `.cap` 파일을 생성하는 방법.

## 사전 요구사항

- Docker Desktop (Windows/Mac) 또는 Docker Engine (Linux)
- SDK Docker 이미지: `opensdk:25.04.09`
- 테스트 서명 키: `cert_keys/AppTest.crt`, `cert_keys/AppTest.key`
  - 분기마다 갱신 필요. [Hanwha Open Platform](https://hanwhasecurity.com) Help Desk에서 최신 버전 다운로드
  - 현재 적용 키: 2Q 2026 (2026-04-01 ~ 2026-06-30)

## 프로젝트 구조

```
run_yolo/
├── .env                        # APP_NAME, SDK_VER, SOC 환경변수
├── docker-compose.yml          # 빌드 컨테이너 정의
├── cert_keys/                  # 서명 키 (git 미추적, 볼륨 마운트)
│   ├── AppTest.crt
│   └── AppTest.key
├── config/
│   └── app_manifest.json       # AppName, Permissions
├── app/
│   ├── CMakeLists.txt
│   ├── toolchain.cmake         # SoC별 크로스 컴파일러 설정
│   ├── src/                    # C++ 소스
│   ├── res/
│   │   ├── ai_bin/
│   │   │   ├── network_binary.nb   # NPU 모델 바이너리
│   │   │   └── model_config.json   # 모델별 설정
│   │   └── cert/               # 앱 인증서 템플릿
│   ├── libs/                   # 빌드 산출물 (.so)
│   └── bin/                    # 빌드 산출물 (실행파일)
└── {APP_NAME}.cap              # 최종 패키지 산출물
```

## 빌드 절차

### 1. 환경 설정 확인

`.env`:
```
APP_NAME=object_detector
SDK_VER=25.04.09
SOC=wn9
```

`docker-compose.yml`의 볼륨 마운트 확인:
```yaml
volumes:
  - ./:/opt/${APP_NAME}
  - ./cert_keys:/opt/opensdk/signature
```

### 2. 클린 빌드

```powershell
# Windows PowerShell
Remove-Item -Recurse -Force app/build -ErrorAction SilentlyContinue
docker-compose down --remove-orphans
docker-compose up
```

```bash
# Linux/Mac
rm -rf app/build
docker-compose down --remove-orphans
docker-compose up
```

### 3. 빌드 과정

컨테이너는 자동으로 다음을 수행:

1. `cd /opt/${APP_NAME}/app/ && mkdir -p build && cd build`
2. `cmake -DSOC=${SOC} ..` — 크로스 컴파일 설정
3. `make clean && make` — 컴파일
4. `make install` — 산출물을 `libs/`, `bin/` 으로 복사
5. `opensdk_packager` — `/opt/opensdk/signature/`의 키로 서명하여 `.cap` 생성

### 4. 산출물 확인

빌드 완료 후 프로젝트 루트에:
- `{APP_NAME}.cap` — 카메라 업로드용 패키지
- `{APP_NAME}.tar.bz2` — 내부 tar 아카이브

## 카메라 배포

1. 브라우저로 카메라 웹 UI 접속
2. **설정 → Open Platform → 애플리케이션 설치**
3. `{APP_NAME}.cap` 파일 업로드
4. 설치 후 **실행(Run)** 버튼으로 앱 시작

## 트러블슈팅

### "어플리케이션 업로드 실패" (OpenSDKError 103)

**원인**: 서명 키 만료. 테스트 키는 분기별로 갱신됨.

**해결**:
1. Hanwha Help Desk에서 최신 키 다운로드 (`AppTest.key`, `AppTest.crt`)
2. `cert_keys/` 폴더에 덮어쓰기
3. 클린 빌드 후 재업로드

### 앱 설치는 되지만 실행되지 않음

**원인**: 매니페스트 불일치. 특히 `app/res/models/AppDispatcher_manifest_instance_0.json`의 `ReceiverNames`와 `GroupName`이 `Classification_manifest_instance_0.json`의 `InstanceName` 및 `app_manifest.json`의 `AppName`과 일치해야 함.

**확인**:
```bash
grep -r "InstanceName\|ReceiverNames\|GroupName\|AppName" config/ app/res/models/ app/src/classification/manifests/ app/src/PLifeCycleManagermanifest.json
```

### 이전 빌드 산출물로 인한 충돌

**증상**: 예전 `.so` 또는 실행파일이 남아서 패키징 시 혼란 발생.

**해결**:
```powershell
Remove-Item -Recurse -Force app/build -ErrorAction SilentlyContinue
Remove-Item -Force app/libs/*/lib*.so -ErrorAction SilentlyContinue
Remove-Item -Force app/bin/${APP_NAME} -ErrorAction SilentlyContinue
Remove-Item -Force *.cap, *.tar.bz2 -ErrorAction SilentlyContinue
```

### CMake 캐시 경로 불일치

**증상**: `CMakeCache.txt directory ... is different than the directory ...`

**원인**: 이전 빌드에서 다른 `APP_NAME`으로 CMake 캐시 생성됨.

**해결**: `app/build` 폴더 완전 삭제 후 재빌드.

## SoC 변경

다른 카메라 SoC용으로 빌드하려면 `.env`의 `SOC` 값 변경:

| SoC | 대상 카메라 |
|---|---|
| `wn9` | PNV-A9082RZ 등 Wisenet9 |
| `cv5` | Ambarella CV5 기반 |
| `nt9869x` | Novatek 기반 |
| `orinnx8g_jp512` | NVIDIA Orin NX |
| `mt8139p` / `nt98567` | 기타 |

변경 후 반드시 `app/build` 삭제 후 클린 빌드.

## 모델/설정 교체 (코드 수정 없이)

1. `app/res/ai_bin/network_binary.nb` 교체
2. `app/res/ai_bin/model_config.json` 수정:
   - `model.file`, `input_tensor`, `output_tensors`, `input_width/height`
   - `postprocess.anchors`, `strides`, `num_classes`
   - `classes`, `display.name`, `mqtt.topic_prefix`
3. 재빌드 후 카메라 배포
