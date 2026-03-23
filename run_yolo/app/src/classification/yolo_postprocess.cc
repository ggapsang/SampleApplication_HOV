#include "yolo_postprocess.h"
#include <algorithm>
#include <cmath>

static float ComputeIoU(const Detection& a, const Detection& b)
{
  float inter_x1 = std::max(a.x1, b.x1);
  float inter_y1 = std::max(a.y1, b.y1);
  float inter_x2 = std::min(a.x2, b.x2);
  float inter_y2 = std::min(a.y2, b.y2);

  float inter_w = std::max(0.0f, inter_x2 - inter_x1);
  float inter_h = std::max(0.0f, inter_y2 - inter_y1);
  float inter_area = inter_w * inter_h;

  float area_a = (a.x2 - a.x1) * (a.y2 - a.y1);
  float area_b = (b.x2 - b.x1) * (b.y2 - b.y1);
  float union_area = area_a + area_b - inter_area;

  if (union_area <= 0.0f) return 0.0f;
  return inter_area / union_area;
}

std::vector<Detection> YoloPostProcess(
    const float* bbox_data,
    const float* conf_data,
    int num_detections,
    float conf_threshold,
    float iou_threshold,
    float scale_x,
    float scale_y,
    float pad_x,
    float pad_y,
    int orig_width,
    int orig_height)
{
  std::vector<Detection> candidates;
  candidates.reserve(256);

  for (int i = 0; i < num_detections; i++) {
    float conf = conf_data[i];
    if (conf < conf_threshold) continue;

    float cx = bbox_data[0 * num_detections + i];
    float cy = bbox_data[1 * num_detections + i];
    float w  = bbox_data[2 * num_detections + i];
    float h  = bbox_data[3 * num_detections + i];

    float x1 = (cx - w * 0.5f) * scale_x - pad_x;
    float y1 = (cy - h * 0.5f) * scale_y - pad_y;
    float x2 = (cx + w * 0.5f) * scale_x - pad_x;
    float y2 = (cy + h * 0.5f) * scale_y - pad_y;

    x1 = std::max(0.0f, std::min(x1, static_cast<float>(orig_width)));
    y1 = std::max(0.0f, std::min(y1, static_cast<float>(orig_height)));
    x2 = std::max(0.0f, std::min(x2, static_cast<float>(orig_width)));
    y2 = std::max(0.0f, std::min(y2, static_cast<float>(orig_height)));

    if (x2 <= x1 || y2 <= y1) continue;

    Detection d;
    d.x1 = x1; d.y1 = y1; d.x2 = x2; d.y2 = y2;
    d.confidence = conf;
    d.class_id = 0;
    candidates.push_back(d);
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const Detection& a, const Detection& b) {
              return a.confidence > b.confidence;
            });

  std::vector<Detection> results;
  std::vector<bool> suppressed(candidates.size(), false);

  for (size_t i = 0; i < candidates.size(); i++) {
    if (suppressed[i]) continue;
    results.push_back(candidates[i]);
    if (results.size() >= 100) break;

    for (size_t j = i + 1; j < candidates.size(); j++) {
      if (suppressed[j]) continue;
      if (ComputeIoU(candidates[i], candidates[j]) > iou_threshold) {
        suppressed[j] = true;
      }
    }
  }

  return results;
}
