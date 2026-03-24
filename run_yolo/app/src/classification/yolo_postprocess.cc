#include "yolo_postprocess.h"
#include <algorithm>
#include <cmath>

static constexpr int DFL_BINS = 16;

// DFL 디코딩: 16-bin softmax → expected value (가중 합산)
static float DflDecode(const float* bins)
{
  float max_val = bins[0];
  for (int i = 1; i < DFL_BINS; i++) {
    if (bins[i] > max_val) max_val = bins[i];
  }

  float sum_exp = 0.0f;
  float exp_vals[DFL_BINS];
  for (int i = 0; i < DFL_BINS; i++) {
    exp_vals[i] = expf(bins[i] - max_val);
    sum_exp += exp_vals[i];
  }

  float dist = 0.0f;
  for (int i = 0; i < DFL_BINS; i++) {
    dist += i * (exp_vals[i] / sum_exp);
  }
  return dist;
}

static float ComputeIoU(const Detection& a, const Detection& b)
{
  float inter_x1 = std::max(a.x1, b.x1);
  float inter_y1 = std::max(a.y1, b.y1);
  float inter_x2 = std::min(a.x2, b.x2);
  float inter_y2 = std::min(a.y2, b.y2);

  float inter_area = std::max(0.0f, inter_x2 - inter_x1) *
                     std::max(0.0f, inter_y2 - inter_y1);

  float area_a = (a.x2 - a.x1) * (a.y2 - a.y1);
  float area_b = (b.x2 - b.x1) * (b.y2 - b.y1);
  float union_area = area_a + area_b - inter_area;

  if (union_area <= 0.0f) return 0.0f;
  return inter_area / union_area;
}

// 단일 scale 처리: DFL 디코딩 + dist2bbox (640x640 기준 xyxy 출력)
static void ProcessScale(
    const float* bbox_data,  // (64, H, W) NCHW, batch dim 제외
    const float* cls_data,   // (1, H, W) NCHW, batch dim 제외
    int grid_h, int grid_w,
    int stride,
    float conf_threshold,
    std::vector<Detection>& detections)
{
  const int grid_size = grid_h * grid_w;

  for (int row = 0; row < grid_h; row++) {
    for (int col = 0; col < grid_w; col++) {
      int idx = row * grid_w + col;

      // 1. Sigmoid(cls logit) → confidence
      float logit = cls_data[idx];
      float conf = 1.0f / (1.0f + expf(-logit));
      if (conf < conf_threshold) continue;

      // 2. DFL 디코딩: 64ch → 4방향 거리
      float bins[DFL_BINS];
      float distances[4];  // left, top, right, bottom
      for (int dir = 0; dir < 4; dir++) {
        for (int b = 0; b < DFL_BINS; b++) {
          bins[b] = bbox_data[(dir * DFL_BINS + b) * grid_size + idx];
        }
        distances[dir] = DflDecode(bins);
      }

      // 3. dist2bbox: anchor 중심 + 거리 → xyxy (640x640 좌표)
      float anchor_cx = (col + 0.5f) * stride;
      float anchor_cy = (row + 0.5f) * stride;

      float x1 = anchor_cx - distances[0] * stride;
      float y1 = anchor_cy - distances[1] * stride;
      float x2 = anchor_cx + distances[2] * stride;
      float y2 = anchor_cy + distances[3] * stride;

      Detection det;
      det.x1 = x1;
      det.y1 = y1;
      det.x2 = x2;
      det.y2 = y2;
      det.confidence = conf;
      det.class_id = 0;
      detections.push_back(det);
    }
  }
}

std::vector<Detection> YoloRawPostProcess(
    const float* bbox_p3, const float* bbox_p4, const float* bbox_p5,
    const float* cls_p3,  const float* cls_p4,  const float* cls_p5,
    float conf_threshold,
    float iou_threshold,
    float letterbox_inv_scale,
    float pad_x, float pad_y,
    int orig_width, int orig_height)
{
  std::vector<Detection> detections;
  detections.reserve(256);

  // 3개 scale 처리 (P3: 80x80 stride 8, P4: 40x40 stride 16, P5: 20x20 stride 32)
  ProcessScale(bbox_p3, cls_p3, 80, 80,  8, conf_threshold, detections);
  ProcessScale(bbox_p4, cls_p4, 40, 40, 16, conf_threshold, detections);
  ProcessScale(bbox_p5, cls_p5, 20, 20, 32, conf_threshold, detections);

  // Letterbox 역변환: 640x640 → 원본 해상도
  for (auto& d : detections) {
    d.x1 = (d.x1 - pad_x) * letterbox_inv_scale;
    d.y1 = (d.y1 - pad_y) * letterbox_inv_scale;
    d.x2 = (d.x2 - pad_x) * letterbox_inv_scale;
    d.y2 = (d.y2 - pad_y) * letterbox_inv_scale;

    // 원본 프레임 범위로 클리핑
    d.x1 = std::max(0.0f, std::min(d.x1, static_cast<float>(orig_width)));
    d.y1 = std::max(0.0f, std::min(d.y1, static_cast<float>(orig_height)));
    d.x2 = std::max(0.0f, std::min(d.x2, static_cast<float>(orig_width)));
    d.y2 = std::max(0.0f, std::min(d.y2, static_cast<float>(orig_height)));
  }

  // 유효하지 않은 bbox 제거
  detections.erase(
      std::remove_if(detections.begin(), detections.end(),
                     [](const Detection& d) { return d.x2 <= d.x1 || d.y2 <= d.y1; }),
      detections.end());

  // NMS: confidence 내림차순 정렬 → greedy suppression
  std::sort(detections.begin(), detections.end(),
            [](const Detection& a, const Detection& b) {
              return a.confidence > b.confidence;
            });

  std::vector<Detection> results;
  std::vector<bool> suppressed(detections.size(), false);

  for (size_t i = 0; i < detections.size(); i++) {
    if (suppressed[i]) continue;
    results.push_back(detections[i]);
    if (results.size() >= 100) break;

    for (size_t j = i + 1; j < detections.size(); j++) {
      if (suppressed[j]) continue;
      if (ComputeIoU(detections[i], detections[j]) > iou_threshold) {
        suppressed[j] = true;
      }
    }
  }

  return results;
}
