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

constexpr unsigned long HashStr (const char* str, int h=0){ return !str[h] ? 55 : ( HashStr(str, h+1) *33) + (unsigned char)(str[h]); }

class Classification : public Component {
  struct ManifestInfo {
   public:
    std::string app_name;
    std::string version;
    std::vector<std::string> permissions;
  };
  struct AppAttributeInfo {
   public:
    std::string model_name;
    std::string input_tensor_names;
    std::string output_tensor_names;

    std::string get_index_input_name;
    std::string get_index_output_name;
    std::string get_name_input_index;
    std::string get_name_output_index;

    void reset() {
      model_name.clear();
      input_tensor_names.clear();
      output_tensor_names.clear();
      get_index_input_name.clear();
      get_index_output_name.clear();
      get_name_input_index.clear();
      get_name_output_index.clear();
    }

    AppAttributeInfo() { reset(); }
  };

  struct RunNeuralNetworkInfoList : public SerializableJson {
   public:
    AppAttributeInfo app_attribute_info;
    std::string attribute_version_;

   public:
    RunNeuralNetworkInfoList() = delete;
    explicit RunNeuralNetworkInfoList(const std::string& version) {
      sizeOfThis = sizeof(*this);
      app_attribute_info.reset();
      attribute_version_ = version;
    }

    std::string Serialize(const std::string& groupName, std::map<std::string, SerializerAttribute>& _keyValueMap, std::string version) override {
      JsonUtility::JsonDocument document;
      JsonUtility::JsonDocument::AllocatorType& alloc = document.GetAllocator();
      document.SetObject();

      JsonUtility::set(document, "Version", attribute_version_, alloc);
      JsonUtility::ValueType attributes_info(rapidjson::kArrayType);
      attributes_info.SetArray();

      JsonUtility::ValueType app_info;
      app_info.SetObject();

      JsonUtility::set(app_info, "model_name", app_attribute_info.model_name, alloc);
      JsonUtility::set(app_info, "input_tensor_names", app_attribute_info.input_tensor_names, alloc);
      JsonUtility::set(app_info, "output_tensor_names", app_attribute_info.output_tensor_names, alloc);

      JsonUtility::set(app_info, "get_index_input_name", app_attribute_info.get_index_input_name, alloc);
      JsonUtility::set(app_info, "get_index_output_name", app_attribute_info.get_index_output_name, alloc);
      JsonUtility::set(app_info, "get_name_input_index", app_attribute_info.get_name_input_index, alloc);
      JsonUtility::set(app_info, "get_name_output_index", app_attribute_info.get_name_output_index, alloc);

      attributes_info.PushBack(app_info, alloc);

      document.AddMember(JsonUtility::ValueType("Attributes", alloc), attributes_info, alloc);

      rapidjson::StringBuffer strbuf;
      rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
      document.Accept(writer);

      return strbuf.GetString();
    }

    bool Deserialize(const std::string& inputString, const std::string& groupName, std::map<std::string, SerializerAttribute>& _keyValueMap) override {
      try {
        JsonUtility::JsonDocument document;
        document.SetObject();

        rapidjson::ParseResult json_parse_result = document.Parse(inputString);
        if (!json_parse_result) {
          Log::Print("Attribute file parsing failed at %s Error Code : %d\n", groupName.c_str(), json_parse_result.Code());
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

            JsonUtility::get(arrayItr, "model_name", app_info.model_name);
            JsonUtility::get(arrayItr, "input_tensor_names", app_info.input_tensor_names);
            JsonUtility::get(arrayItr, "output_tensor_names", app_info.output_tensor_names);

            JsonUtility::get(arrayItr, "get_index_input_name", app_info.get_index_input_name);
            JsonUtility::get(arrayItr, "get_name_input_index", app_info.get_name_input_index);
            JsonUtility::get(arrayItr, "get_index_output_name", app_info.get_index_output_name);
            JsonUtility::get(arrayItr, "get_name_output_index", app_info.get_name_output_index);

            app_attribute_info = app_info;
          }
        }
      } catch (const Exception& e) {
        Log::Print("\033[1;31m Exception ID (%" PRIu64 ") -> %s\n, %s:%d \033[0m\n", e.GetClassId(), e.what(), __PRETTY_FUNCTION__, __LINE__);
        throw e;
      } catch (const std::exception& e) {
        Log::Print("\033[1;31m Exception -> %s\n, %s:%d \033[0m\n", e.what(), __PRETTY_FUNCTION__, __LINE__);
        throw e;
      }

      return true;
    }

    /**
     * @fn    WriteFile()
     * @brief override WriteFile function.
     *        because it doesn't have std::ofstream::trunc option which would
     * erase the existing file contents.
     */
    bool WriteFile(const std::string& filename, const std::string& groupName, std::string& outputString) override {
      std::ofstream output_file(filename.c_str(), std::ofstream::trunc);
      if (output_file.is_open() == false) {
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
  Classification();
  Classification(ClassID id, const char* name);
  virtual ~Classification();
  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;
  void Start() override;
  bool Finalize() override;

  bool UnloadNetwork(const std::string& model_path);
  bool PreProcess(const std::string& model_path, std::shared_ptr<Tensor> rgb);
  void HandleRequest(Event* event);
  bool Execute(const std::string& model_path);
  bool PostProcess(const std::string& model_path, const std::shared_ptr<Tensor>& img);

  virtual void RegisterOpenAPIURI();
  bool ParseNpuEvent(const std::string& event_body);
  bool InsertNpuLoadInfo(std::string& target, std::string recv_value);
  bool ParseManifest(const std::string& manifest_path, ManifestInfo& info);
  void SetMetaFrameSchema();
  void SetMetaFrameCapabilitySchema();
  void SendMetadata();  
  std::string GetXml(const int* max_id, const float* max_val, const int64_t timestamp);
  Vector<String> Split(String line, char seperator);

  bool CreateNetwork(NeuralNetwork* network, JsonUtility::JsonDocument& document);
  bool CreateTensor(NeuralNetwork* network, JsonUtility::JsonDocument& document, const std::string& request);
  bool LoadNetwork(NeuralNetwork* network);
  bool RunNetwork(NeuralNetwork* network);
  bool CheckParseResult(NeuralNetwork* network);
  bool UnloadNetwork(NeuralNetwork* network);
  bool GetTensorName(NeuralNetwork* network, JsonUtility::JsonDocument& document, const std::string& request);
  bool GetTensorIndex(NeuralNetwork* network, JsonUtility::JsonDocument& document, const std::string& request);
  bool GetAllTensor(NeuralNetwork* network, const std::string& mod);
  bool GetTensorCount(NeuralNetwork* network, const std::string& mod);
  std::string TimePointToString(uint64_t timestamp) const;

 private:
  void Inference(std::shared_ptr<RawImage> img);
  bool ParseResult(float* data, int out_width);
  void ProcessRawVideo(Event* event);
  void DebugLog(const char* format, ...)
  {
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
                    arg
                    );
    std::cout << buffer << std::endl;
  }
 private:
  struct NnLodeInfo {
    std::string model_name_;
    std::string input_tensor_names_;
    std::string output_tensor_names_;
  } npu_load_info;

  std::string relative_model_path;

  int max_id[5];
  float max_val[5];
  uint64_t raw_pts = 0;
  bool parse_result = true;

  bool run_flag = 0;
  std::shared_ptr<RunNeuralNetworkInfoList> run_neural_network_info_list;
  ManifestInfo manifest_;

protected:
  NeuralNetwork* GetOrCreateNetwork(const std::string& name);
  NeuralNetwork* GetNetwork(const std::string& name);
  NeuralNeworkMap& GetAllNetworks();
  void RemoveNetwork(const std::string& name);

  NeuralNeworkMap nn_map_;
};
