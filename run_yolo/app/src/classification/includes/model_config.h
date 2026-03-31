#pragma once

#include <string>
#include <vector>

struct ModelConfig {
  struct ModelInfo {
    std::string file = "network_binary.nb";
    int input_width = 640;
    int input_height = 640;
    std::string input_tensor = "images_0";
    std::vector<std::string> output_tensors = {"attach_output0/out0"};
    std::vector<float> mean = {0.0f, 0.0f, 0.0f};
    std::vector<float> scale = {1.0f, 1.0f, 1.0f};
  } model;

  struct PostprocessConfig {
    int num_classes = 1;
    int max_detections = 100;
    // anchors[level] = {w0,h0, w1,h1, w2,h2}
    std::vector<std::vector<float>> anchors = {
        {12,16, 19,36, 40,28},
        {36,75, 76,55, 72,146},
        {142,110, 192,243, 459,401}
    };
    std::vector<int> strides = {8, 16, 32};
    int num_anchors_per_level = 3;
  } postprocess;

  std::vector<std::string> classes = {"object"};

  struct DisplayConfig {
    std::string name = "Object";
    std::string osd_format = "{name}: {count} | Conf: {conf}%";
    std::string osd_no_detection = "{name}: 0";
  } display;

  struct MqttConfig {
    std::string topic_prefix = "object";
    std::string client_id = "object_detector";
  } mqtt;
};

bool LoadModelConfig(const std::string& path, ModelConfig& cfg);
