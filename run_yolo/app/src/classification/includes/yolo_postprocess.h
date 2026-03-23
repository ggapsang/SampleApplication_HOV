#pragma once

#include <vector>
#include <cstdint>

// YOLOv11 Detection 결과 구조체
struct Detection {
  float x1, y1, x2, y2;  // 원본 해상도 기준 bbox (xyxy)
  float confidence;
  int class_id;
};

// Phase 3에서 구현 예정
// YOLOv11 출력 텐서 디코딩 + NMS
// 출력 텐서 shape: [1, 5, 8400] — row0~3: cx,cy,w,h / row4: confidence
std::vector<Detection> YoloPostProcess(
    const float* output_data,
    int num_detections,
    int num_classes_plus_4,
    float conf_threshold,
    float iou_threshold,
    float scale_x,
    float scale_y,
    float pad_x,
    float pad_y,
    int orig_width,
    int orig_height);
