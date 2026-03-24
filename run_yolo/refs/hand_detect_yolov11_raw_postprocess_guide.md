# hand_detect_yolov11 Raw Output 후처리 가이드

## 1. 변경 배경

기존 `hand_detect_yolov11.onnx`는 end-to-end 모델로, 내부에 DFL 디코딩, anchor 계산, Sigmoid 등 후처리 연산이 모두 포함되어 `(1, 5, 8400)` 형태의 최종 결과를 출력했습니다. 이 구조는 PC(onnxruntime)에서는 정상 동작하지만, APL_AI Compiler(Acuity)에서 DFL Softmax, MatMul, Reshape 등의 연산을 정확히 변환하지 못해 NPU 변환 시 confidence가 0으로 수렴하는 문제가 발생했습니다.

이를 해결하기 위해 후처리 연산을 그래프에서 제거한 `hand_detect_yolov11_raw.onnx`를 생성했으며, 후처리 로직은 카메라 앱 코드에서 직접 구현해야 합니다.

---

## 2. 모델 입출력 구조 변경 사항

### 2.1 입력 (변경 없음)

| 항목 | 값 |
|------|-----|
| 이름 | `images` |
| Shape | `(1, 3, 640, 640)` |
| Format | RGB, float32, 0~1 범위 (pixel / 255.0) |
| 채널 순서 | RGB (reverse_channel: false) |

### 2.2 출력 비교

**기존 모델 (end-to-end)**

| 출력 | Shape | 설명 |
|------|-------|------|
| output0 | `(1, 5, 8400)` | row 0-3: decoded bbox (cx, cy, w, h in pixels), row 4: confidence (Sigmoid 적용 완료) |

**변경 모델 (raw)**

| 출력 | Shape | 설명 |
|------|-------|------|
| cv2.0 (bbox) | `(1, 64, 80, 80)` | P3 (stride 8) bbox raw, 6400 anchors |
| cv2.1 (bbox) | `(1, 64, 40, 40)` | P4 (stride 16) bbox raw, 1600 anchors |
| cv2.2 (bbox) | `(1, 64, 20, 20)` | P5 (stride 32) bbox raw, 400 anchors |
| cv3.0 (cls) | `(1, 1, 80, 80)` | P3 class logit (Sigmoid 이전), 6400 anchors |
| cv3.1 (cls) | `(1, 1, 40, 40)` | P4 class logit (Sigmoid 이전), 1600 anchors |
| cv3.2 (cls) | `(1, 1, 20, 20)` | P5 class logit (Sigmoid 이전), 400 anchors |

총 anchor 수: 6400 + 1600 + 400 = **8400** (기존과 동일)

---

## 3. 후처리 파이프라인

전체 흐름: **NPU 출력** → **Sigmoid (cls)** → **DFL 디코딩 (bbox)** → **dist2bbox** → **스케일 변환** → **NMS** → **최종 detection**

### 3.1 Confidence Score 계산 (cls 출력)

cls 출력은 Sigmoid 이전의 raw logit 값입니다. Sigmoid를 적용해야 0~1 범위의 confidence가 됩니다.

```
confidence = sigmoid(cls_logit) = 1.0 / (1.0 + exp(-cls_logit))
```

단일 클래스(hand) 모델이므로 cls 채널은 1개입니다. 각 scale의 cls 출력을 flatten하면:
- P3: 6400개
- P4: 1600개
- P5: 400개

**최적화**: confidence threshold (예: 0.5) 이하인 anchor는 bbox 디코딩을 건너뛸 수 있습니다.

### 3.2 DFL (Distribution Focal Loss) 디코딩 (bbox 출력)

bbox 출력 64채널은 4방향 x 16 bins 구조입니다.

```
64 channels = 4 directions × 16 DFL bins
  [0:16]   → left   distance
  [16:32]  → top    distance  
  [32:48]  → right  distance
  [48:64]  → bottom distance
```

각 방향의 16개 bin에 대해 다음을 수행합니다:

```
(1) 16개 bin에 Softmax 적용
    softmax[i] = exp(bin[i]) / sum(exp(bin[0..15]))

(2) 가중 합산 (expected value 계산)
    distance = sum(i * softmax[i]) for i = 0..15
```

이 과정을 4방향 각각에 적용하면 `(lt, tb, rt, bb)` = `(left, top, right, bottom)` 거리값이 나옵니다.

C++ 의사 코드:

```cpp
// bbox_raw: 64채널, 한 anchor 위치에 대한 값
// 결과: distances[4] = {left, top, right, bottom}
float distances[4];
for (int dir = 0; dir < 4; dir++) {
    float bins[16];
    float max_val = -FLT_MAX;
    
    // 16개 bin 추출
    for (int i = 0; i < 16; i++) {
        bins[i] = bbox_raw[dir * 16 + i];
        if (bins[i] > max_val) max_val = bins[i];
    }
    
    // Softmax
    float sum_exp = 0.0f;
    for (int i = 0; i < 16; i++) {
        bins[i] = expf(bins[i] - max_val);  // 수치 안정성
        sum_exp += bins[i];
    }
    
    // 가중 합산
    float dist = 0.0f;
    for (int i = 0; i < 16; i++) {
        dist += i * (bins[i] / sum_exp);
    }
    distances[dir] = dist;
}
```

### 3.3 dist2bbox 변환 (거리 → 좌표)

DFL로 얻은 `(left, top, right, bottom)` 거리를 anchor 중심 기준으로 bbox 좌표로 변환합니다.

각 scale의 anchor 중심점 좌표:

```
P3 (80x80, stride=8):   anchor_cx = (col + 0.5) * 8,   anchor_cy = (row + 0.5) * 8
P4 (40x40, stride=16):  anchor_cx = (col + 0.5) * 16,  anchor_cy = (row + 0.5) * 16
P5 (20x20, stride=32):  anchor_cx = (col + 0.5) * 32,  anchor_cy = (row + 0.5) * 32
```

변환 공식:

```
x1 = anchor_cx - left  * stride
y1 = anchor_cy - top   * stride
x2 = anchor_cx + right * stride
y2 = anchor_cy + bottom * stride

// xyxy → xywh (center format)
cx = (x1 + x2) / 2.0
cy = (y1 + y2) / 2.0
w  = x2 - x1
h  = y2 - y1
```

결과 좌표는 640x640 입력 이미지 기준 **픽셀 단위**입니다.

### 3.4 NMS (Non-Maximum Suppression)

confidence threshold 이상인 detection에 대해 NMS를 적용합니다.

```
입력: 각 detection의 (cx, cy, w, h, confidence)
파라미터:
  - confidence_threshold: 0.5 (조정 가능)
  - iou_threshold: 0.45 (조정 가능)

알고리즘:
  1. confidence < threshold인 detection 제거
  2. confidence 내림차순 정렬
  3. 가장 높은 confidence의 detection 선택
  4. 선택된 detection과 나머지의 IoU 계산
  5. IoU > iou_threshold인 detection 제거
  6. 3-5 반복
```

### 3.5 원본 이미지 좌표로 변환

모델 입력이 640x640이므로, 원본 이미지 크기에 맞게 스케일링합니다.

```
scale_x = original_width  / 640.0
scale_y = original_height / 640.0

original_cx = cx * scale_x
original_cy = cy * scale_y
original_w  = w  * scale_x
original_h  = h  * scale_y
```

letterbox padding을 사용한 경우 padding offset도 보정해야 합니다.

---

## 4. NPU 출력 텐서 매핑

NPU에서 `network_binary.nb`를 로드한 후 출력 텐서 인덱스는 ONNX 출력 순서를 따릅니다.

| 인덱스 | ONNX 출력 이름 | 용도 | Shape |
|--------|---------------|------|-------|
| 0 | cv2.0 | P3 bbox raw | (1, 64, 80, 80) |
| 1 | cv2.1 | P4 bbox raw | (1, 64, 40, 40) |
| 2 | cv2.2 | P5 bbox raw | (1, 64, 20, 20) |
| 3 | cv3.0 | P3 cls logit | (1, 1, 80, 80) |
| 4 | cv3.1 | P4 cls logit | (1, 1, 40, 40) |
| 5 | cv3.2 | P5 cls logit | (1, 1, 20, 20) |

postprocess_file.yml에서 각 출력의 `add_postproc_node: true` 및 `force_float32: true`를 설정하여 dequantize된 float32 값을 받아야 합니다.

---

## 5. C++ 후처리 참조 구현

```cpp
#include <cmath>
#include <vector>
#include <algorithm>

struct Detection {
    float cx, cy, w, h;
    float confidence;
};

// DFL 디코딩: 16-bin softmax → expected value
float dfl_decode(const float* bins) {
    float max_val = *std::max_element(bins, bins + 16);
    float sum_exp = 0.0f;
    float exp_vals[16];
    for (int i = 0; i < 16; i++) {
        exp_vals[i] = expf(bins[i] - max_val);
        sum_exp += exp_vals[i];
    }
    float dist = 0.0f;
    for (int i = 0; i < 16; i++) {
        dist += i * (exp_vals[i] / sum_exp);
    }
    return dist;
}

// 단일 scale 처리
void process_scale(
    const float* bbox_data,  // (64, H, W)
    const float* cls_data,   // (1, H, W)
    int grid_h, int grid_w,
    int stride,
    float conf_threshold,
    std::vector<Detection>& detections)
{
    int grid_size = grid_h * grid_w;
    
    for (int row = 0; row < grid_h; row++) {
        for (int col = 0; col < grid_w; col++) {
            int idx = row * grid_w + col;
            
            // 1. Confidence (Sigmoid)
            float logit = cls_data[idx];
            float conf = 1.0f / (1.0f + expf(-logit));
            if (conf < conf_threshold) continue;
            
            // 2. DFL 디코딩
            float bins[16];
            float distances[4];  // left, top, right, bottom
            for (int dir = 0; dir < 4; dir++) {
                for (int b = 0; b < 16; b++) {
                    bins[b] = bbox_data[(dir * 16 + b) * grid_size + idx];
                }
                distances[dir] = dfl_decode(bins);
            }
            
            // 3. dist2bbox
            float anchor_cx = (col + 0.5f) * stride;
            float anchor_cy = (row + 0.5f) * stride;
            
            float x1 = anchor_cx - distances[0] * stride;
            float y1 = anchor_cy - distances[1] * stride;
            float x2 = anchor_cx + distances[2] * stride;
            float y2 = anchor_cy + distances[3] * stride;
            
            Detection det;
            det.cx = (x1 + x2) / 2.0f;
            det.cy = (y1 + y2) / 2.0f;
            det.w  = x2 - x1;
            det.h  = y2 - y1;
            det.confidence = conf;
            detections.push_back(det);
        }
    }
}

// 전체 후처리
std::vector<Detection> postprocess(
    const float* bbox_p3, const float* bbox_p4, const float* bbox_p5,
    const float* cls_p3,  const float* cls_p4,  const float* cls_p5,
    float conf_threshold = 0.5f,
    float iou_threshold = 0.45f)
{
    std::vector<Detection> detections;
    
    process_scale(bbox_p3, cls_p3, 80, 80, 8,  conf_threshold, detections);
    process_scale(bbox_p4, cls_p4, 40, 40, 16, conf_threshold, detections);
    process_scale(bbox_p5, cls_p5, 20, 20, 32, conf_threshold, detections);
    
    // NMS
    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) {
                  return a.confidence > b.confidence;
              });
    
    std::vector<Detection> result;
    std::vector<bool> suppressed(detections.size(), false);
    
    for (size_t i = 0; i < detections.size(); i++) {
        if (suppressed[i]) continue;
        result.push_back(detections[i]);
        
        for (size_t j = i + 1; j < detections.size(); j++) {
            if (suppressed[j]) continue;
            
            // IoU 계산
            float x1 = std::max(detections[i].cx - detections[i].w/2,
                                detections[j].cx - detections[j].w/2);
            float y1 = std::max(detections[i].cy - detections[i].h/2,
                                detections[j].cy - detections[j].h/2);
            float x2 = std::min(detections[i].cx + detections[i].w/2,
                                detections[j].cx + detections[j].w/2);
            float y2 = std::min(detections[i].cy + detections[i].h/2,
                                detections[j].cy + detections[j].h/2);
            
            float inter = std::max(0.0f, x2-x1) * std::max(0.0f, y2-y1);
            float area_i = detections[i].w * detections[i].h;
            float area_j = detections[j].w * detections[j].h;
            float iou = inter / (area_i + area_j - inter);
            
            if (iou > iou_threshold) suppressed[j] = true;
        }
    }
    
    return result;
}
```

---

## 6. 검증 기준값

PC에서 동일 이미지로 확인한 reference 값입니다. 카메라 앱 후처리 구현 후 이 값과 비교하여 정합성을 검증할 수 있습니다.

| 항목 | 값 |
|------|-----|
| confidence > 0.5 개수 | 10개 |
| 최고 confidence | 0.9309 |
| confidence top-5 | 0.9309, 0.9304, 0.9299, 0.9282, 0.9276 |
| 탐지 대상 | 단일 클래스 (hand) |
| P3 (80x80) conf 최대 | ~0.00003 (해당 이미지에서는 P3에서 검출 거의 없음) |
| P4 (40x40) conf 최대 | ~0.00002 (해당 이미지에서는 P4에서 검출 거의 없음) |
| P5 (20x20) conf 최대 | 0.9309 (고해상도 손은 P5에서 주로 검출) |

---

## 7. postprocess_file.yml 설정 예시

```yaml
postprocess:
  acuity_postprocs:
    print_topn:
      topn: 5
    dump_results:
      file_type: TENSOR
  app_postprocs:
  - lid: attach_model.23/cv2.0/cv2.0.2/Conv_output_0/out0_0
    postproc_params:
      add_postproc_node: true
      force_float32: true
  - lid: attach_model.23/cv2.1/cv2.1.2/Conv_output_0/out0_1
    postproc_params:
      add_postproc_node: true
      force_float32: true
  - lid: attach_model.23/cv2.2/cv2.2.2/Conv_output_0/out0_2
    postproc_params:
      add_postproc_node: true
      force_float32: true
  - lid: attach_model.23/cv3.0/cv3.0.2/Conv_output_0/out0_3
    postproc_params:
      add_postproc_node: true
      force_float32: true
  - lid: attach_model.23/cv3.1/cv3.1.2/Conv_output_0/out0_4
    postproc_params:
      add_postproc_node: true
      force_float32: true
  - lid: attach_model.23/cv3.2/cv3.2.2/Conv_output_0/out0_5
    postproc_params:
      add_postproc_node: true
      force_float32: true
```

참고: 위 lid 이름은 `hand_detect_yolov11_raw.onnx`를 import한 결과 기준입니다. ONNX 파일이 변경되면 import 후 `.json` 파일에서 lid를 재확인해야 합니다.

---

## 8. 요약 체크리스트

- [ ] `hand_detect_yolov11_raw.onnx`를 컨테이너에 복사
- [ ] `./import hand_detect_yolov11` 실행
- [ ] `inputmeta.yml` 수정: `reverse_channel: false`, `scale: 0.00392156862745`
- [ ] `postprocess_file.yml` 수정: 6개 출력 모두 `add_postproc_node: true`, `force_float32: true`
- [ ] `./quantize hand_detect_yolov11 pcq 500` (또는 1000)
- [ ] `./inference hand_detect_yolov11 float 5`로 검증
- [ ] `./inference hand_detect_yolov11 pcq 5`로 양자화 품질 검증
- [ ] `./export_nb hand_detect_yolov11 pcq WN9`
- [ ] 카메라 앱 코드에 위 후처리 로직 구현
- [ ] PC reference 값과 비교 검증
