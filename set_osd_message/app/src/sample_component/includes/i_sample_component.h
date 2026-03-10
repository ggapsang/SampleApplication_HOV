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
    eSET_OSD_MESSAGE,
  };

  struct AppInfo {
   public:
    std::string enable;
    std::string osd_color;
    std::string transparency;
    std::string osd;

    std::string font_size;
    std::string custom_font_size;

    // Normal camera field
    std::string position_x;
    std::string position_y;
    // For BWC Camera Only field
    std::string osd_position;

    void reset() {
      enable.clear();
      osd_color.clear();
      transparency.clear();
      osd.clear();

      font_size.clear();
      custom_font_size.clear();

      // Normal camera field
      position_x.clear();
      position_y.clear();
      // For BWC Camera Only field
      osd_position.clear();
    }

    AppInfo() { reset(); }
  };
  
  struct SampleAppInfoList : public SerializableJson {
   public:
    AppInfo app_attribute_info;
    std::string attribute_version_;

   public:
    SampleAppInfoList() = delete;
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
      JsonUtility::set(app_info, "enable", app_attribute_info.enable, alloc);
      JsonUtility::set(app_info, "osd_color", app_attribute_info.osd_color, alloc);
      JsonUtility::set(app_info, "transparency", app_attribute_info.transparency, alloc);
      JsonUtility::set(app_info, "osd", app_attribute_info.osd, alloc);

      JsonUtility::set(app_info, "font_size", app_attribute_info.font_size, alloc);
      JsonUtility::set(app_info, "custom_font_size", app_attribute_info.custom_font_size, alloc);

      // Normal camera field
      JsonUtility::set(app_info, "position_x", app_attribute_info.position_x, alloc);
      JsonUtility::set(app_info, "position_y", app_attribute_info.position_y, alloc);
      // For BWC Camera Only field
      JsonUtility::set(app_info, "osd_position", app_attribute_info.osd_position, alloc);
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
            AppInfo app_info;
            JsonUtility::get(arrayItr, "enable", app_info.enable);
            JsonUtility::get(arrayItr, "osd_color", app_info.osd_color);
            JsonUtility::get(arrayItr, "transparency", app_info.transparency);
            JsonUtility::get(arrayItr, "osd", app_info.osd);

            JsonUtility::get(arrayItr, "font_size", app_info.font_size);
            JsonUtility::get(arrayItr, "custom_font_size", app_info.custom_font_size);

            // Normal camera field
            JsonUtility::get(arrayItr, "position_x", app_info.position_x);
            JsonUtility::get(arrayItr, "position_y", app_info.position_y);
            // For BWC Camera Only field
            JsonUtility::get(arrayItr, "osd_position", app_info.osd_position);
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
