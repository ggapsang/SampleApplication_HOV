# OpenSDK 디버깅 교훈록 — Phase 3/4 (NPU 추론 파이프라인)

> 작성일: 2026-03-23
> 대상: hand_detector (PNV-A9082RZ, WN9, SDK 25.04.09)
> 목적: NPU 추론 파이프라인 구축 과정에서 발생한 실패와 교훈 기록

---

## 전체 요약

Phase 3/4는 Raw Video 수신 → PreProcess → NPU Execute → PostProcess(YOLO decode + NMS)
파이프라인을 구축하는 단계였다.

**총 소요 이슈**: 8건 이상
**핵심 교훈**: "외주 개발자가 제공한 파라미터를 믿기 전에, 텐서 shape 로그와 raw 값을 먼저 찍어라."

---

## 1. 텐서 레이아웃 오해 — SoA vs AoS

### 증상
- `dequant_bbox[0] = 319.844, 319.844, -268304512.000, 319.844`
- w(너비) 값만 -268M으로 튀어나옴
- cx/cy/h는 모두 319.844로 동일

### 원인
코드는 **SoA(Structure of Arrays)** 레이아웃 `[5, 8400]`을 가정:
```cpp
// SoA 가정: row 먼저, anchor 나중
raw[row * 8400 + i]  // ← WRONG
```
실제 텐서는 `LoadNeuralNetwork` 로그에서 확인한 것처럼 **AoS(Array of Structures)** `[8400, 5, 1]`:
```
output_tensor shape=[8400,5,1]
```
각 앵커의 5개 값이 연속 배치됨:
```cpp
// AoS 정답: anchor 먼저, channel 나중
raw[i * 5 + channel]  // ← CORRECT
```

### 교훈
- **텐서 shape은 반드시 로딩 직후 Length(0,1,2)로 로그 찍어 확인할 것**
- 모델 출력 형식은 YOLO 표준(`[1, 5, 8400]` SoA)과 다를 수 있음
- APL_AI 컴파일러는 AoS로 출력할 수도 있다

---

## 2. force_float32: true의 실제 의미

### 혼동 포인트
- `force_float32: true` = "NPU가 dequantize까지 해서 float32 출력" 이라고 생각
- 실제로는 **INT8 값을 float32 컨테이너에 그대로 담을 뿐**, scale/zero_point 변환 없음

### 결과
```
raw_bbox[0..2] = -0.000000, -0.000000, -0.000000
                 ↑ INT8 값 0이 float32 0.0f로 저장됨
```
→ 수동 dequantize가 필요: `real = (raw - zero_point) * scale`

### 교훈
- `force_float32: true`는 **타입 캐스트**이지, dequantize가 아님
- 외주 개발자가 "float32로 나온다"고 해도, 수동 dequantize가 필요한지 별도 확인할 것

---

## 3. Confidence 채널 오버플로우 — INT_MAX 표시

### 증상
```
Det(2): [479,269-1439,809 c=2147483647%] [0,0-1920,1080 c=2147483647%]
```
`c=2147483647%` = INT_MAX → float → int 변환 오버플로우

### 원인 체인
1. `add_postproc_node: false` → sigmoid 미적용 → raw logit 출력
2. logit 값이 수백만 규모 (-25920888, 12495, 2e37 등)
3. `(int)(logit * 100)` → 정수 오버플로우 → ARM FPU saturate → INT_MAX

### 해결
```cpp
conf_data[i] = 1.0f / (1.0f + std::exp(-raw[i * 5 + 4]));  // sigmoid
```

### 교훈
- `add_postproc_node: false`이면 confidence 출력은 sigmoid 전 logit값임
- YOLO confidence를 직접 사용하려면 **add_postproc_node: true** 또는 **수동 sigmoid** 필요
- `(int)(float * 100)` 표시 시 오버플로우 가드를 넣거나, float를 그대로 출력할 것

---

## 4. 외주 개발자 제공 dequantize 파라미터 오류

### 경위
외주 개발자가 제공한 conf dequantize 파라미터:

| 시점 | zero_point | 비고 |
|------|-----------|------|
| 1차 제공 | 28 | 틀림 |
| 2차 제공 | 84 | 틀림 |
| 실제 정답 | — | dequantize 자체가 불필요했음 (sigmoid가 해답) |

### 교훈
- 외주 개발자 파라미터를 적용하기 전, **raw 값을 로그로 먼저 찍어 범위 확인**
- INT8-in-float32라면 raw 값이 반드시 -128~127 범위여야 함
- 범위를 벗어나면 파라미터가 아닌 **포맷 자체가 틀린 것**임

---

## 5. 잘못된 텐서 이름으로 인한 침묵의 실패

### 경위
외주 개발자가 출력 텐서를 2개로 분리했다고 했고, 다음 이름을 사용:
```cpp
network->CreateOutputTensor("model.23/Mul_2_output_0");   // bbox
network->CreateOutputTensor("model.23/Sigmoid_output_0"); // conf
```
→ 두 tensor 모두 생성 실패 (이름이 실제 모델과 불일치)
→ `LoadNeuralNetwork()` 반환 `false`
→ `npu_load_info_.model_name_` 세팅 안 됨 (빈 문자열)
→ `GetNetwork("")` → null
→ `PreProcess()` 실패: `"FAIL: PreProcess #1"`

### 문제점
FAIL 로그가 찍혔지만 원인이 한참 아래(LoadNeuralNetwork)에 있어서 연결을 못 찾음.

### 교훈
- **LoadNeuralNetwork 실패 시 Start()에서 명확한 경고 로그**를 남겨야 함
- `npu_load_info_.model_name_`이 비어있으면 이후 모든 단계가 조용히 실패함
- 외주 개발자가 텐서 이름을 바꿨다고 하면, **실제 .nb 파일에서 이름 확인 후 적용**할 것

---

## 6. static 지역 변수로 인한 로그 소실

### 증상
```cpp
static bool pp_logged = false;
if (!pp_logged) {
    pp_logged = true;
    AppendLog(...);  // 프로세스 수명 동안 딱 1회만 출력
}
```
컨테이너 재시작 없이 mode=clear + mode=start 반복 시 초기 진단 로그가 다시 찍히지 않음.

### 교훈
- **진단용 one-shot 로그는 static bool 대신 멤버 변수** (`bool first_postprocess_ = true;`)를 사용
- Start()에서 멤버 변수를 reset하면 inference 재시작마다 로그가 다시 찍힘
- 디버깅 중에는 N회 반복 출력 (`static int cnt = 0; if (++cnt <= 3)`)이 현실적

---

## 7. raw 샘플 로그가 잘못된 인덱스를 읽고 있었던 문제

### 경위
AoS로 dequantize 코드를 수정했지만, 진단용 raw 샘플 로그는 **여전히 SoA 인덱스**를 읽고 있었음:
```cpp
// AoS로 수정 후에도 로그는 SoA 인덱스:
AppendLog("raw_conf[0..2]=" +
    std::to_string(raw[4 * N + 0]) + ...);  // ← SoA 인덱스, 실제 conf 아님
```
→ 진단 로그만 믿고 분석하면 잘못된 결론 도출

### 교훈
- 코드 로직(dequantize)과 **진단 로그 인덱스를 항상 함께 수정**할 것
- 로그를 추가할 때는 "이 로그가 실제로 의도한 값을 찍고 있는가"를 코드 레벨에서 확인

---

## 8. Tensor::Size() 미존재 — 존재하지 않는 API 호출

### 증상
```
error: 'class Tensor' has no member named 'Size'
```

### 원인
SDK 문서 없이 "있을 것 같아서" 사용. `Tensor`의 실제 API는 `Length(dim)` 뿐.

### 교훈
- SDK 클래스의 메서드는 **실제 사용 예제(run_neural_network 샘플)에서 확인된 것만 사용**
- 없는 메서드를 추측해서 넣으면 컴파일 에러 → 빌드 주기 낭비
- Tensor 검증된 API: `Length(0)`, `Length(1)`, `Length(2)`, `VirtAddr()`, `Allocate()`, `Resize()`

---

## 9. add_preproc_node / add_postproc_node 플래그 영향 정리

APL_AI 컴파일 설정이 NPU 출력 포맷을 결정함. 외주 개발자가 이를 바꿀 때마다 코드 대응이 달라짐:

| 설정 | NPU 전처리 | 출력 포맷 | C++ 대응 |
|------|-----------|----------|---------|
| `add_preproc_node: true` | NPU가 정규화 수행 | mean=0, scale=1 | LoadNetwork scale=[1,1,1] |
| `add_preproc_node: false` | NPU 미수행 | raw pixel | LoadNetwork scale=[1/255,...] |
| `add_postproc_node: true` | sigmoid 적용 | conf 0~1 | conf 직접 사용 |
| `add_postproc_node: false` | sigmoid 미적용 | conf = raw logit | 수동 sigmoid 필요 |
| `force_float32: true` | INT8→float 타입캐스트 | INT8값 float 컨테이너 | 수동 dequantize 필요 |

### 교훈
- **외주 개발자가 컴파일 설정을 바꿀 때마다 5가지 플래그 전체 상태를 공유받을 것**
- 플래그가 뭔지 모르면 scale 값, dequantize 필요 여부를 알 수 없음
- 이상적으로는 `add_preproc_node: true`, `add_postproc_node: true`, `force_float32: false`로 통일 → 코드가 가장 단순해짐

---

## 10. 외주 개발자 의존 리스크

이번 Phase에서 외주 개발자가 모델을 최소 5회 재컴파일했고, 매번 코드 수정이 필요했다:

- 출력 텐서 1개 → 2개 분리 → 다시 1개로 복귀
- dequantize 파라미터 변경 (zp: 28 → 84 → 의미 없음)
- 컴파일 설정 변경 (add_preproc_node, add_postproc_node 온오프)

### 외주 개발자에게 모델 변경 시 요구해야 할 정보

```
1. 컴파일 설정 5종 전체 (add_preproc_node, add_postproc_node, force_float32 등)
2. 출력 텐서 이름 (CreateOutputTensor에 넣을 문자열)
3. 출력 텐서 shape (예: [8400, 5, 1])
4. 각 채널의 dequantize 파라미터 (scale, zero_point)
5. 각 채널이 post-sigmoid인지 raw logit인지
```

이 5가지를 받지 않고 코드를 수정하면 항상 실패한다.

---

## 11. 디버깅 효율을 높이는 로그 설계 원칙

이번 Phase 경험으로 정립된 로그 설계 원칙:

1. **LoadNeuralNetwork 직후**: 텐서 shape, 포인터 null 여부 확인
2. **PostProcess 최초 실행 시**: raw 값 샘플 5개 (bbox 채널, conf 채널 별도), 텐서 shape
3. **PostProcess 매 N프레임**: conf range (min/max), threshold 값
4. **FAIL 로그에는 원인 포함**: `"FAIL: PreProcess"` 가 아니라 `"FAIL: GetNetwork(\"%s\") null"` 처럼
5. **Inference 최초 실행 시**: 멀티플레인 선택 정보, 해상도, letterbox scale

---

## 참고: 현재 확정된 모델 출력 포맷 (2026-03-23 기준)

```
텐서 이름  : output0
텐서 shape : [8400, 5, 1]  (AoS)
레이아웃   : raw[i*5+0]=cx, [i*5+1]=cy, [i*5+2]=w, [i*5+3]=h, [i*5+4]=conf_logit
bbox       : INT8-in-float32, dequantize: (raw + 128) × 2.4988 → 0~640px
conf       : raw logit (pre-sigmoid), sigmoid 적용 후 0~1
scale      : [1.0, 1.0, 1.0]  (add_preproc_node: true)
```