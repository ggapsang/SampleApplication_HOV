#include "yolo_postprocess.h"

// Phase 3에서 구현 예정
// 현재는 빈 스텁 — Phase 1 빌드 통과용

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
    int orig_height)
{
  return {};
}
