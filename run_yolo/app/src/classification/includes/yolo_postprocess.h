#pragma once

#include <vector>
#include <cstdint>

struct Detection {
  float x1, y1, x2, y2;  // 원본 해상도 기준 bbox (xyxy)
  float confidence;
  int class_id;
};

// Raw YOLOv11 후처리: 6개 텐서(bbox P3/P4/P5, cls P3/P4/P5)로부터
// DFL 디코딩 + dist2bbox + NMS + letterbox 역변환 수행
//
// bbox 텐서: (1, 64, H, W) NCHW, cls 텐서: (1, 1, H, W) NCHW
// letterbox_inv_scale = 1.0/ratio, pad_x/pad_y = letterbox padding
std::vector<Detection> YoloRawPostProcess(
    const float* bbox_p3, const float* bbox_p4, const float* bbox_p5,
    const float* cls_p3,  const float* cls_p4,  const float* cls_p5,
    float conf_threshold,
    float iou_threshold,
    float letterbox_inv_scale,
    float pad_x, float pad_y,
    int orig_width, int orig_height);
