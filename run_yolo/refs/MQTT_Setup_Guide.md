# MQTT 실시간 로그 수신 가이드

> 카메라(192.168.2.60) → PC(192.168.2.80) MQTT 브로커 → mosquitto_sub

---

## 실행 방법 (매번)

### 1. 브로커 실행 (관리자 PowerShell)

```powershell
net stop mosquitto
cd "C:\Program Files\Mosquitto"
.\mosquitto -v -c mosquitto.conf
```

> 이 창은 닫지 마세요. "포트 사용 중" 오류 → `net stop mosquitto` 다시 실행.

### 2. 구독 (새 PowerShell 창)

```powershell
cd "C:\Program Files\Mosquitto"
.\mosquitto_sub -h localhost -t "hand/#" -v
```

> 카메라 앱 Start 누르면 메시지 수신 시작.
> 중지: `Ctrl+C` / 재시작: 같은 명령 다시 입력.
> PowerShell 클릭해서 멈추면 Enter 키로 풀림.

---

## 수신 토픽

| 토픽 | 주기 | 예시 |
|------|------|------|
| `hand/status` | Start/Stop 시 | `{"state":"running","model":"network_binary.nb"}` |
| `hand/detection` | 매 프레임 | `{"frame":100,"det":3,"conf":100}` |
| `hand/debug` | 10프레임마다 | `{"pre":3,"inf":26,"post":4,"det":3}` |

---

## 브로커 IP 변경

카메라 앱에 하드코딩되어 있음. 변경하려면:

**파일**: `app/src/classification/includes/classification.h`
```cpp
std::string mqtt_broker_host = "192.168.1.78";  // ← PC IP 변경
int mqtt_broker_port = 1883;
```

변경 후 빌드 → 재설치 필요.

---

## 최초 1회 설정 (이미 완료됨)

### mosquitto.conf 생성

```powershell
# 관리자 PowerShell
Set-Content -Path "C:\Program Files\Mosquitto\mosquitto.conf" -Value @"
listener 1883 0.0.0.0
allow_anonymous true
"@ -Encoding ASCII
```

### 방화벽 허용

```powershell
# 관리자 PowerShell
netsh advfirewall firewall add rule name="MQTT Broker 1883" dir=in action=allow protocol=tcp localport=1883
```

---

## 트러블슈팅

| 증상 | 해결 |
|------|------|
| 브로커 "포트 사용 중" | `net stop mosquitto` 후 재실행 |
| 앱 로그 "MQTT: FAILED" | PC IP 확인 + 방화벽 확인 + 브로커 실행 확인 |
| mosquitto_sub 아무것도 안 뜸 | 앱에서 Start 눌렀는지 확인 |
| PowerShell 출력 멈춤 | Enter 키 (QuickEdit 모드) |
