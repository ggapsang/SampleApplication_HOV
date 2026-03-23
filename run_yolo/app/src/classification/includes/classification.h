#pragma once

#include "i_pl_video_frame_raw.h"
#include <mutex>
#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "neural_network.h"
#include "tensor.h"

#include "i_analytics_detector.h"
#include "typedef_analytics_detector.h"
#include "i_log_manager.h"

constexpr ClassID kComponentId =
    static_cast<ClassID>(_ELayer_Analytics_Detector::_eObjectDetectorAI);

constexpr unsigned long HashStr(const char* str, int h = 0) {
  return !str[h] ? 55 : (HashStr(str, h + 1) * 33) + (unsigned char)(str[h]);
}

class HandDetector : public Component {
  struct ManifestInfo {
   public:
    std::string app_name;
    std::string version;
    std::vector<std::string> permissions;
  };

  struct AppAttributeInfo {
   public:
    float confidence_threshold = 0.5f;
    float nms_iou_threshold = 0.45f;
    int skip_frames = 0;
    std::string mqtt_broker_host;
    int mqtt_broker_port = 1883;
    int alarm_on_threshold = 5;
    int alarm_off_threshold = 30;
    int alarm_cooldown_sec = 10;

    void reset() {
      confidence_threshold = 0.5f;
      nms_iou_threshold = 0.45f;
      skip_frames = 0;
      mqtt_broker_host.clear();
      mqtt_broker_port = 1883;
      alarm_on_threshold = 5;
      alarm_off_threshold = 30;
      alarm_cooldown_sec = 10;
    }

    AppAttributeInfo() { reset(); }
  };

  struct HandDetectorInfoList : public SerializableJson {
   public:
    AppAttributeInfo app_attribute_info;
    std::string attribute_version_;

   public:
    HandDetectorInfoList() = delete;
    explicit HandDetectorInfoList(const std::string& version) {
      sizeOfThis = sizeof(*this);
      app_attribute_info.reset();
      attribute_version_ = version;
    }

    std::string Serialize(const std::string& groupName,
                          std::map<std::string, SerializerAttribute>& _keyValueMap,
                          std::string version) override {
      JsonUtility::JsonDocument document;
      JsonUtility::JsonDocument::AllocatorType& alloc = document.GetAllocator();
      document.SetObject();

      JsonUtility::set(document, "Version", attribute_version_, alloc);
      JsonUtility::ValueType attributes_info(rapidjson::kArrayType);
      attributes_info.SetArray();

      JsonUtility::ValueType app_info;
      app_info.SetObject();

      JsonUtility::set(app_info, "confidence_threshold", app_attribute_info.confidence_threshold, alloc);
      JsonUtility::set(app_info, "nms_iou_threshold", app_attribute_info.nms_iou_threshold, alloc);
      JsonUtility::set(app_info, "skip_frames", app_attribute_info.skip_frames, alloc);
      JsonUtility::set(app_info, "mqtt_broker_host", app_attribute_info.mqtt_broker_host, alloc);
      JsonUtility::set(app_info, "mqtt_broker_port", app_attribute_info.mqtt_broker_port, alloc);
      JsonUtility::set(app_info, "alarm_on_threshold", app_attribute_info.alarm_on_threshold, alloc);
      JsonUtility::set(app_info, "alarm_off_threshold", app_attribute_info.alarm_off_threshold, alloc);
      JsonUtility::set(app_info, "alarm_cooldown_sec", app_attribute_info.alarm_cooldown_sec, alloc);

      attributes_info.PushBack(app_info, alloc);
      document.AddMember(JsonUtility::ValueType("Attributes", alloc), attributes_info, alloc);

      rapidjson::StringBuffer strbuf;
      rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
      document.Accept(writer);

      return strbuf.GetString();
    }

    bool Deserialize(const std::string& inputString,
                     const std::string& groupName,
                     std::map<std::string, SerializerAttribute>& _keyValueMap) override {
      try {
        JsonUtility::JsonDocument document;
        document.SetObject();

        rapidjson::ParseResult json_parse_result = document.Parse(inputString);
        if (!json_parse_result) {
          Log::Print("Attribute file parsing failed at %s Error Code : %d\n",
                     groupName.c_str(), json_parse_result.Code());
          return false;
        }

        if (document.HasMember("Version")) {
          attribute_version_ = document["Version"].GetString();
        }

        app_attribute_info.reset();
        JsonUtility::JsonDocument::MemberIterator attributes_info = document.FindMember("Attributes");
        if (attributes_info == document.MemberEnd()) {
          Log::Print("Attribute Field is not found at %s", groupName.c_str());
          return false;
        }

        if (attributes_info->value.IsArray()) {
          for (JsonUtility::ValueType& arrayItr : attributes_info->value.GetArray()) {
            AppAttributeInfo app_info;
            JsonUtility::get(arrayItr, "confidence_threshold", app_info.confidence_threshold);
            JsonUtility::get(arrayItr, "nms_iou_threshold", app_info.nms_iou_threshold);
            JsonUtility::get(arrayItr, "skip_frames", app_info.skip_frames);
            JsonUtility::get(arrayItr, "mqtt_broker_host", app_info.mqtt_broker_host);
            JsonUtility::get(arrayItr, "mqtt_broker_port", app_info.mqtt_broker_port);
            JsonUtility::get(arrayItr, "alarm_on_threshold", app_info.alarm_on_threshold);
            JsonUtility::get(arrayItr, "alarm_off_threshold", app_info.alarm_off_threshold);
            JsonUtility::get(arrayItr, "alarm_cooldown_sec", app_info.alarm_cooldown_sec);
            app_attribute_info = app_info;
          }
        }
      } catch (const Exception& e) {
        Log::Print("\033[1;31m Exception ID (%" PRIu64 ") -> %s\n, %s:%d \033[0m\n",
                   e.GetClassId(), e.what(), __PRETTY_FUNCTION__, __LINE__);
        throw e;
      } catch (const std::exception& e) {
        Log::Print("\033[1;31m Exception -> %s\n, %s:%d \033[0m\n",
                   e.what(), __PRETTY_FUNCTION__, __LINE__);
        throw e;
      }
      return true;
    }

    bool WriteFile(const std::string& filename,
                   const std::string& groupName,
                   std::string& outputString) override {
      std::ofstream output_file(filename.c_str(), std::ofstream::trunc);
      if (!output_file.is_open()) {
        output_file.close();
        return false;
      }
      output_file << outputString;
      output_file.close();
      return true;
    }
  };

 public:
  using base = Component;
  using NeuralNeworkMap = std::unordered_map<std::string, std::unique_ptr<NeuralNetwork>>;

 public:
  HandDetector();
  HandDetector(ClassID id, const char* name);
  virtual ~HandDetector();
  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;
  void Start() override;
  bool Finalize() override;

  bool ParseManifest(const std::string& manifest_path, ManifestInfo& info);
  virtual void RegisterOpenAPIURI();
  bool HandleStreamRequest(OpenAppSerializable* param, const std::string& body);

  NeuralNetwork* GetOrCreateNetwork(const std::string& name);
  NeuralNetwork* GetNetwork(const std::string& name);
  NeuralNeworkMap& GetAllNetworks();
  void RemoveNetwork(const std::string& name);

 private:
  void DebugLog(const char* format, ...) {
    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    auto* arg = new ("") Platform_Std_Refine::SerializableString(buffer);
    SendTargetEvents(
        ILogManager::remote_debug_message_group,
        static_cast<int32_t>(ILogManager::EEvent::eRemoteDebugMessage),
        0,
        arg);
    std::cout << buffer << std::endl;
  }

 private:
  struct NnLoadInfo {
    std::string model_name_;
    std::string input_tensor_names_;
    std::string output_tensor_names_;
  } npu_load_info_;

  NeuralNeworkMap nn_map_;
  bool run_flag_ = false;
  uint64_t raw_pts_ = 0;
  std::shared_ptr<HandDetectorInfoList> info_list_;
  ManifestInfo manifest_;

 protected:
  NeuralNeworkMap nn_map_protected_;  // Phase 2에서 사용
};
