#pragma once

#include <typedef_base.h>

#include <set>
#include <unordered_map>

#include "json_utility.h"
#include "serializable_json.h"

enum class _EPTestComponent {
  _eBegin = GET_LAYER_UID(_ELayer::_ePApplication),
  _eSampleComponent,
  _eEnd,
};

constexpr ClassID _SampleComponent_Id = GET_CLASS_UID(_EPTestComponent::_eSampleComponent);

class ISampleComponent {
 public:
  enum class EEventType { eBegin = _SampleComponent_Id, eRequestSunapi, eRequestSunapiDone, eEnd };

  struct ManifestInfo {
   public:
    std::string app_name;
    std::string version;
    std::vector<std::string> permissions;
  };

  struct AppAttributeInfo {
   public:
    std::string jpeg_path;
    std::string channel;

    void reset() {
      jpeg_path.clear();
      channel.clear();
    }

    AppAttributeInfo() { reset(); }
  };

  struct SampleAppInfoList : public SerializableJson {
   public:
    AppAttributeInfo app_attribute_info;
    std::string attribute_version_;

   public:
    SampleAppInfoList() = default;
    explicit SampleAppInfoList(const std::string& version) {
      sizeOfThis = sizeof(*this);
      app_attribute_info.reset();
      attribute_version_ = version;
    }

    std::string Serialize(const std::string& groupName, std::map<std::string, SerializerAttribute>& _keyValueMap, std::string version) override {
      JsonUtility::JsonDocument document;
      JsonUtility::JsonDocument::AllocatorType& alloc = document.GetAllocator();
      document.SetObject();

      JsonUtility::set(document, "Version", attribute_version_, alloc);
      JsonUtility::set(document, "Name", groupName, alloc);
      JsonUtility::ValueType attributes_info(rapidjson::kArrayType);
      attributes_info.SetArray();

      JsonUtility::ValueType app_info;
      app_info.SetObject();

      JsonUtility::set(app_info, "jpeg_path", app_attribute_info.jpeg_path, alloc);
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
        JsonUtility::JsonDocument::MemberIterator attributes = document.FindMember("Attributes");
        if (attributes == document.MemberEnd()) {
          Log::Print("Attribute Field is not found at %s", groupName.c_str());
          return false;
        }
        if (attributes->value.IsArray()) {
          for (JsonUtility::ValueType& arrayItr : attributes->value.GetArray()) {
            AppAttributeInfo app_info;

            JsonUtility::get(arrayItr, "jpeg_path", app_info.jpeg_path);
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
};
