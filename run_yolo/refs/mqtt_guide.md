# MQTT 로그 수신 가이드

카메라 앱이 MQTT 브로커로 추론 로그를 발행. PC의 mosquitto_sub 또는 Python 클라이언트로 수신.

## 개요

| 발행 주체 | 카메라 앱 |
|---|---|
| 기본 브로커 | `192.168.2.80:1883` (PC) |
| 토픽 프리픽스 | `hand` (설정 가능) |
| 발행 주기 | 5초 (debug), 감지 시 즉시 (detection), 상태 변경 시 (status) |
| QoS | 0 (fire-and-forget) |

## 토픽 및 페이로드

| 토픽 | 주기 | 페이로드 예시 |
|---|---|---|
| `hand/detection` | 감지 시 즉시 | `{"frame":142,"det":2,"conf":85}` |
| `hand/debug` | 5초마다 | `{"pre":55,"inf":57,"post":2,"det":0}` |
| `hand/status` | Start/Stop 시 | `{"state":"running","model":"network_binary.nb"}` |

필드:
- `pre/inf/post`: 전처리/추론/후처리 시간 (ms)
- `det`: 감지된 객체 수
- `conf`: 최대 confidence (0~100 정수)
- `frame`: 프레임 번호

## 설정 방법

MQTT 설정은 두 곳에서 관리됨:

### 1. 브로커 IP/포트 (런타임 변경 가능)

`app/src/classification/manifests/Classification_default_attribute_0.json`:
```json
{
  "mqtt_broker_host": "192.168.2.80",
  "mqtt_broker_port": 1883
}
```

또는 웹 UI의 `mode=config` API로 런타임 변경.

### 2. 토픽 프리픽스 / 클라이언트 ID (빌드 타임)

`app/res/ai_bin/model_config.json`:
```json
{
  "mqtt": {
    "topic_prefix": "hand",
    "client_id": "hand_detector"
  }
}
```

모델 교체 시 `topic_prefix`를 변경하면 모든 토픽이 `{prefix}/detection`, `{prefix}/debug`, `{prefix}/status` 형태로 바뀜.

## PC 설정 (최초 1회)

### Mosquitto 브로커 설치

Windows:
```powershell
winget install EclipseFoundation.Mosquitto
```

Linux / WSL:
```bash
sudo apt install mosquitto mosquitto-clients
```

Docker:
```bash
docker run -d --name mosquitto -p 1883:1883 eclipse-mosquitto:2
```

### 브로커 설정 (외부 접속 허용)

`mosquitto.conf`:
```
listener 1883 0.0.0.0
allow_anonymous true
```

Windows 설정 파일 생성:
```powershell
# 관리자 PowerShell
Set-Content -Path "C:\Program Files\Mosquitto\mosquitto.conf" -Value @"
listener 1883 0.0.0.0
allow_anonymous true
"@ -Encoding ASCII
```

### 방화벽 허용 (Windows)

```powershell
# 관리자 PowerShell
netsh advfirewall firewall add rule name="MQTT Broker 1883" dir=in action=allow protocol=tcp localport=1883
```

## 실행 방법

### 브로커 실행 (관리자 PowerShell)

```powershell
net stop mosquitto
cd "C:\Program Files\Mosquitto"
.\mosquitto -v -c mosquitto.conf
```

창을 닫지 말 것. "포트 사용 중" 오류 시 `net stop mosquitto` 후 재실행.

### 구독 (새 PowerShell 창)

전체 토픽:
```powershell
cd "C:\Program Files\Mosquitto"
.\mosquitto_sub -h localhost -t "hand/#" -v
```

Linux/Mac:
```bash
mosquitto_sub -h <브로커IP> -t "hand/#" -v
```

토픽별:
```bash
mosquitto_sub -h <브로커IP> -t "hand/detection" -v
mosquitto_sub -h <브로커IP> -t "hand/debug" -v
mosquitto_sub -h <브로커IP> -t "hand/status" -v
```

### Python으로 수신

```python
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    print(f"{msg.topic}: {msg.payload.decode()}")

client = mqtt.Client()
client.on_message = on_message
client.connect("192.168.2.80", 1883)
client.subscribe("hand/#")
client.loop_forever()
```

```bash
pip install paho-mqtt
```

## 트러블슈팅

| 증상 | 원인 | 해결 |
|---|---|---|
| 앱 로그 `MQTT: FAILED to connect` | 브로커 미실행 또는 IP 불일치 | 브로커 실행 확인, PC IP 확인 |
| 연결은 되는데 메시지 없음 | 앱이 inference 시작 안 함 | 웹 UI에서 Start 클릭 |
| 브로커 "포트 사용 중" | 서비스 중복 실행 | `net stop mosquitto` 후 재실행 |
| PowerShell 출력 멈춤 | QuickEdit 모드 클릭 선택 상태 | Enter 키 입력 |
| 간헐적 끊김 | keepalive 타임아웃 (60초) | 브로커 로그 확인 |
| 방화벽 차단 | 1883 포트 차단 | 위 방화벽 설정 확인 |
