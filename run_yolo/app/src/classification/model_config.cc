#include "model_config.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <rapidjson/document.h>

static bool GetString(const rapidjson::Value& obj, const char* key, std::string& out)
{
  if (obj.HasMember(key) && obj[key].IsString()) {
    out = obj[key].GetString();
    return true;
  }
  return false;
}

static bool GetInt(const rapidjson::Value& obj, const char* key, int& out)
{
  if (obj.HasMember(key) && obj[key].IsInt()) {
    out = obj[key].GetInt();
    return true;
  }
  return false;
}

static bool GetFloat(const rapidjson::Value& obj, const char* key, float& out)
{
  if (obj.HasMember(key) && obj[key].IsNumber()) {
    out = static_cast<float>(obj[key].GetDouble());
    return true;
  }
  return false;
}

static bool GetFloatArray(const rapidjson::Value& obj, const char* key, std::vector<float>& out)
{
  if (!obj.HasMember(key) || !obj[key].IsArray()) return false;
  out.clear();
  for (auto& v : obj[key].GetArray()) {
    if (v.IsNumber()) out.push_back(static_cast<float>(v.GetDouble()));
  }
  return true;
}

static bool GetStringArray(const rapidjson::Value& obj, const char* key, std::vector<std::string>& out)
{
  if (!obj.HasMember(key) || !obj[key].IsArray()) return false;
  out.clear();
  for (auto& v : obj[key].GetArray()) {
    if (v.IsString()) out.push_back(v.GetString());
  }
  return true;
}

static bool GetIntArray(const rapidjson::Value& obj, const char* key, std::vector<int>& out)
{
  if (!obj.HasMember(key) || !obj[key].IsArray()) return false;
  out.clear();
  for (auto& v : obj[key].GetArray()) {
    if (v.IsInt()) out.push_back(v.GetInt());
  }
  return true;
}

bool LoadModelConfig(const std::string& path, ModelConfig& cfg)
{
  cfg = ModelConfig{};

  std::ifstream file(path);
  if (!file.is_open()) {
    std::cout << "[ModelConfig] File not found: " << path
              << " — using defaults" << std::endl;
    return false;
  }

  std::stringstream ss;
  ss << file.rdbuf();
  std::string json_str = ss.str();

  rapidjson::Document doc;
  doc.Parse(json_str.c_str());
  if (doc.HasParseError()) {
    std::cout << "[ModelConfig] JSON parse error at offset "
              << doc.GetErrorOffset() << " — using defaults" << std::endl;
    cfg = ModelConfig{};
    return false;
  }

  // model
  if (doc.HasMember("model") && doc["model"].IsObject()) {
    const auto& m = doc["model"];
    GetString(m, "file", cfg.model.file);
    GetInt(m, "input_width", cfg.model.input_width);
    GetInt(m, "input_height", cfg.model.input_height);
    GetString(m, "input_tensor", cfg.model.input_tensor);
    GetStringArray(m, "output_tensors", cfg.model.output_tensors);
    GetFloatArray(m, "mean", cfg.model.mean);
    GetFloatArray(m, "scale", cfg.model.scale);
  }

  // postprocess
  if (doc.HasMember("postprocess") && doc["postprocess"].IsObject()) {
    const auto& pp = doc["postprocess"];
    GetInt(pp, "num_classes", cfg.postprocess.num_classes);
    GetInt(pp, "max_detections", cfg.postprocess.max_detections);
    GetInt(pp, "num_anchors_per_level", cfg.postprocess.num_anchors_per_level);
    GetIntArray(pp, "strides", cfg.postprocess.strides);

    if (pp.HasMember("anchors") && pp["anchors"].IsArray()) {
      cfg.postprocess.anchors.clear();
      for (auto& level : pp["anchors"].GetArray()) {
        if (level.IsArray()) {
          std::vector<float> anchor_level;
          for (auto& v : level.GetArray()) {
            if (v.IsNumber()) anchor_level.push_back(static_cast<float>(v.GetDouble()));
          }
          cfg.postprocess.anchors.push_back(std::move(anchor_level));
        }
      }
    }
  }

  // classes
  GetStringArray(doc, "classes", cfg.classes);

  // display
  if (doc.HasMember("display") && doc["display"].IsObject()) {
    const auto& d = doc["display"];
    GetString(d, "name", cfg.display.name);
    GetString(d, "osd_format", cfg.display.osd_format);
    GetString(d, "osd_no_detection", cfg.display.osd_no_detection);
  }

  // mqtt
  if (doc.HasMember("mqtt") && doc["mqtt"].IsObject()) {
    const auto& mq = doc["mqtt"];
    GetString(mq, "topic_prefix", cfg.mqtt.topic_prefix);
    GetString(mq, "client_id", cfg.mqtt.client_id);
  }

  std::cout << "[ModelConfig] Loaded: file=" << cfg.model.file
            << " input=" << cfg.model.input_width << "x" << cfg.model.input_height
            << " classes=[";
  for (size_t i = 0; i < cfg.classes.size(); i++) {
    if (i > 0) std::cout << ",";
    std::cout << cfg.classes[i];
  }
  std::cout << "]" << std::endl;

  return true;
}
