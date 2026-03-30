# Hand Detector 빌드 및 배포

## 빌드

```bash
cd c:\opensdk\SampleApplication\run_yolo

# 빌드 캐시 제거 (CMake 캐시 문제 시)
docker compose run --rm opensdk rm -rf /opt/hand_detector/app/build

# 빌드 + 패키징 (hand_detector.cap 생성)
docker compose up

# 컨테이너 정리
docker compose down --remove-orphans
```

빌드 결과물: `hand_detector.cap`

## 카메라 배포

```bash
opensdk_install -a hand_detector -i 192.168.2.60 -u admin -w {비밀번호}
```

## 디버그 콘솔 접속

브라우저에서:
```
http://192.168.5.60/opensdk/hand_detector_Czyc3/configuration
```

- `Start` 버튼으로 추론 시작
- `Stop` 버튼으로 추론 중지
- `Info` 버튼으로 상태 확인

---

# MQTT 로그 수신 가이드

## 개요

카메라 앱(hand_detector)이 MQTT 브로커로 로그를 발행합니다.
- **Heartbeat**: 5초마다 1회 (Timing 정보)
- **Detection**: 손 감지 시 즉시

## 1. MQTT 브로커 설치 (PC)

### Windows
```
winget install EclipseFoundation.Mosquitto
```
또는 https://mosquitto.org/download/ 에서 설치

### Linux / WSL
```bash
sudo apt install mosquitto mosquitto-clients
sudo systemctl start mosquitto
```

### Docker
```bash
docker run -d --name mosquitto -p 1883:1883 eclipse-mosquitto:2
```

> mosquitto 2.x는 기본 설정이 localhost만 허용합니다.
> 외부 접속을 위해 설정 파일 수정 필요:
```
# mosquitto.conf
listener 1883 0.0.0.0
allow_anonymous true
```

## 2. 카메라 앱 설정

브로커 주소는 앱 attribute에서 설정됩니다:
- `mqtt_broker_host`: 브로커 IP (기본값: `192.168.2.80`)
- `mqtt_broker_port`: 포트 (기본값: `1883`)

웹 UI(`/mode=config`)에서 변경하거나, `Classification_default_attribute_0.json`에서 직접 수정 가능.

## 3. 토픽 구독

### 전체 로그 수신
```bash
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 192.168.2.80 -t "hand/#" -v
mosquitto_sub -h <브로커IP> -t "hand/#" -v
```

### 토픽별 수신
```bash
# 감지 결과만
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 192.168.2.80 -t "hand/detection" -v
mosquitto_sub -h <브로커IP> -t "hand/detection" -v

# Timing (heartbeat)
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 192.168.2.80 -t "hand/debug" -v
mosquitto_sub -h <브로커IP> -t "hand/debug" -v

# 상태 변경
& "C:\Program Files\mosquitto\mosquitto_sub.exe" -h 192.168.2.80 -t "hand/status" -v
mosquitto_sub -h <브로커IP> -t "hand/status" -v
```

## 4. 토픽 및 페이로드 형식

| 토픽 | 발행 주기 | 페이로드 |
|------|-----------|----------|
| `hand/debug` | 5초마다 | `{"pre":55,"inf":57,"post":2,"det":0}` |
| `hand/detection` | 감지 시 즉시 | `{"frame":142,"det":2,"conf":85}` |
| `hand/status` | 시작/종료 시 | `{"state":"initialized","model":"network_binary.nb"}` |

### 필드 설명
- `pre/inf/post`: 전처리/추론/후처리 시간 (ms)
- `det`: 감지된 객체 수
- `conf`: 최대 confidence (0~100 정수)
- `frame`: 프레임 번호

## 5. Python으로 수신

```python
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    print(f"{msg.topic}: {msg.payload.decode()}")

client = mqtt.Client()
client.on_message = on_message
client.connect("192.168.9.199", 1883)
client.subscribe("hand/#")
client.loop_forever()
```

```bash
pip install paho-mqtt
python mqtt_recv.py
```

## 6. 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| `MQTT: FAILED to connect` | 브로커 미실행 또는 IP 불일치 | 브로커 실행 확인, IP 확인 |
| 연결은 되는데 메시지 없음 | 앱이 아직 inference 시작 안 함 | `/mode=start` 호출 |
| 간헐적 끊김 | keepalive 타임아웃 (60초) | 브로커 로그 확인 |
| 방화벽 차단 | 1883 포트 차단 | `sudo ufw allow 1883` 또는 Windows 방화벽 규칙 추가 |
