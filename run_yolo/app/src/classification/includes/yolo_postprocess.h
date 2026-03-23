#pragma once

#include <vector>
#include <cstdint>

struct Detection {
  float x1, y1, x2, y2;  // 원본 해상도 기준 bbox (xyxy)
  float confidence;
  int class_id;
};

// bbox_data: [4, 8400] — row0~3: cx, cy, w, h (0~640)
// conf_data: [8400]    — confidence (0~1)
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
    int orig_height);
