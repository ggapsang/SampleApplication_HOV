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
  enum class eEventFunction {
    eJPEG_ENCODE,
  };

  struct ManifestInfo {
   public:
    std::string app_name;
    std::string version;
    std::vector<std::string> permissions;
  };

  struct AppAttributeInfo {
   public:
    std::string topic_str;
    std::string source_key;
    std::string source_value;
    std::string data_key;
    std::string data_value;
    std::string element_name;
    std::string element_descriptor_name;
    std::string element_type_name;
    std::string element_likelihood;
    std::string object_id;
    std::string parent_id;
    std::string likelihood;
    std::string category;
    std::string attr_num;
    std::string rect_sx;
    std::string rect_ex;
    std::string rect_sy;
    std::string rect_ey;

    void reset() {
      topic_str.clear();
      source_key.clear();
      source_value.clear();
      data_key.clear();
      data_value.clear();
      element_name.clear();
      element_descriptor_name.clear();
      element_type_name.clear();
      element_likelihood.clear();
      object_id.clear();
      parent_id.clear();
      likelihood.clear();
      category.clear();
      attr_num.clear();
      rect_sx.clear();
      rect_ex.clear();
      rect_sy.clear();
      rect_ey.clear();
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

      JsonUtility::set(app_info, "topic_str", app_attribute_info.topic_str, alloc);
      JsonUtility::set(app_info, "source_key", app_attribute_info.source_key, alloc);
      JsonUtility::set(app_info, "source_value", app_attribute_info.source_value, alloc);
      JsonUtility::set(app_info, "data_key", app_attribute_info.data_key, alloc);
      JsonUtility::set(app_info, "data_value", app_attribute_info.data_value, alloc);
      JsonUtility::set(app_info, "element_name", app_attribute_info.element_name, alloc);
      JsonUtility::set(app_info, "element_descriptor_name", app_attribute_info.element_descriptor_name, alloc);
      JsonUtility::set(app_info, "element_type_name", app_attribute_info.element_type_name, alloc);
      JsonUtility::set(app_info, "element_likelihood", app_attribute_info.element_likelihood, alloc);

      JsonUtility::set(app_info, "object_id", app_attribute_info.object_id, alloc);
      JsonUtility::set(app_info, "parent_id", app_attribute_info.parent_id, alloc);
      JsonUtility::set(app_info, "likelihood", app_attribute_info.likelihood, alloc);
      JsonUtility::set(app_info, "category", app_attribute_info.category, alloc);
      JsonUtility::set(app_info, "attr_num", app_attribute_info.attr_num, alloc);
      JsonUtility::set(app_info, "rect_sx", app_attribute_info.rect_sx, alloc);
      JsonUtility::set(app_info, "rect_ex", app_attribute_info.rect_ex, alloc);
      JsonUtility::set(app_info, "rect_sy", app_attribute_info.rect_sy, alloc);
      JsonUtility::set(app_info, "rect_ey", app_attribute_info.rect_ey, alloc);
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
            JsonUtility::get(arrayItr, "topic_str", app_info.topic_str);
            JsonUtility::get(arrayItr, "source_key", app_info.source_key);
            JsonUtility::get(arrayItr, "source_value", app_info.source_value);
            JsonUtility::get(arrayItr, "data_key", app_info.data_key);
            JsonUtility::get(arrayItr, "data_value", app_info.data_value);
            JsonUtility::get(arrayItr, "element_name", app_info.element_name);
            JsonUtility::get(arrayItr, "element_descriptor_name", app_info.element_descriptor_name);
            JsonUtility::get(arrayItr, "element_type_name", app_info.element_type_name);
            JsonUtility::get(arrayItr, "element_likelihood", app_info.element_likelihood);

            JsonUtility::get(arrayItr, "object_id", app_info.object_id);
            JsonUtility::get(arrayItr, "parent_id", app_info.parent_id);
            JsonUtility::get(arrayItr, "likelihood", app_info.likelihood);
            JsonUtility::get(arrayItr, "category", app_info.category);
            JsonUtility::get(arrayItr, "attr_num", app_info.attr_num);
            JsonUtility::get(arrayItr, "rect_sx", app_info.rect_sx);
            JsonUtility::get(arrayItr, "rect_ex", app_info.rect_ex);
            JsonUtility::get(arrayItr, "rect_sy", app_info.rect_sy);
            JsonUtility::get(arrayItr, "rect_ey", app_info.rect_ey);
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
