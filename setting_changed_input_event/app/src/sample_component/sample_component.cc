#include "sample_component.h"

#include <dispatcher_serialize.h>
#include <i_app_dispatcher.h>
#include <i_p_open_platform_manager.h>
#include <open_platform_define.h>

#include <iomanip>

using namespace openplatform;

namespace {
auto eventToArgumentBuffer = [](Event* event) {
  auto blob = event->GetBlobArgument();
  std::pair<std::variant<BaseObject*, char*>, uint64_t> ret((char*)blob.GetRawData(),  // variant
                                                            blob.GetSize());           // size
  return ret;
};
}

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {
  setting_changed_info_.insert({
      {"Network Interface", std::make_pair("", "")},
      {"Network Port", std::make_pair("", "")},
      {"Video Profile", std::make_pair("", "")},
      {"Audio", std::make_pair("", "")},
      {"Image", std::make_pair("", "")},
      {"Video Rotation", std::make_pair("", "")},
      {"WiseStream", std::make_pair("", "")},
      {"Exposure AI", std::make_pair("", "")},
      {"Record", std::make_pair("", "")},
      {"Language", std::make_pair("", "")},
      {"Time", std::make_pair("", "")},
      {"Analytics", std::make_pair("", "")},
  });
}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

void SampleComponent::Start() {
  Component::Start();
  RegisterOpenAPIURI();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eNetworkSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eNetworkSettingChanged@@@@@@@@@@@@@@@@");
      setting_changed_info_["Network Interface"] = std::make_pair(GetCurrentTimeToString(), "None");
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eNetworkPortChanged): {
      DebugLog("@@@@@@@@@@@@@@@@@SampleComponent eNetworkPortChanged@@@@@@@@@@@@@@@@@");
      setting_changed_info_["Network Port"] = std::make_pair(GetCurrentTimeToString(), "None");
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eProfileSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eProfileSettingChanged@@@@@@@@@@@@@@@@");
      DebugLog("Channel %d profile setting is changed!", event->GetChannel());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      setting_changed_info_["Video Profile"] = std::make_pair(GetCurrentTimeToString(), std::to_string(event->GetChannel()));
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eAudioSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@@@SampleComponent eAudioSettingChanged@@@@@@@@@@@@@@@@");
      DebugLog("Channel %d audio setting is changed!", event->GetChannel());
      setting_changed_info_["Audio"] = std::make_pair(GetCurrentTimeToString(), std::to_string(event->GetChannel()));
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eImageSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@@@SampleComponent eImageSettingChanged@@@@@@@@@@@@@@@@");
      DebugLog("Channel %d image setting is changed!", event->GetChannel());
      setting_changed_info_["Image"] = std::make_pair(GetCurrentTimeToString(), std::to_string(event->GetChannel()));
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eUpdateVideoRotation): {
      DebugLog("@@@@@@@@@@@@@@@@@SampleComponent eUpdateVideoRotation@@@@@@@@@@@@@@@@");
      DebugLog("Channel %d video rotation setting is changed!", event->GetChannel());
      setting_changed_info_["Video Rotation"] = std::make_pair(GetCurrentTimeToString(), std::to_string(event->GetChannel()));
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eWiseStreamSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eWiseStreamSettingChanged@@@@@@@@@@@@@@");
      DebugLog("Channel %d wise stream setting is changed!", event->GetChannel());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      setting_changed_info_["WiseStream"] = std::make_pair(GetCurrentTimeToString(), std::to_string(event->GetChannel()));
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eExposureAISettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eWiseStreamSettingChanged@@@@@@@@@@@@@@");
      DebugLog("Channel %d exposure AI setting is changed!", event->GetChannel());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      setting_changed_info_["Exposure AI"] = std::make_pair(GetCurrentTimeToString(), std::to_string(event->GetChannel()));
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eRecordSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@@SampleComponent eRecordSettingChanged@@@@@@@@@@@@@@@@");
      DebugLog("Channel %d record setting is changed!", event->GetChannel());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      setting_changed_info_["Record"] = std::make_pair(GetCurrentTimeToString(), std::to_string(event->GetChannel()));
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eUpdateSystemSetting): {
      DebugLog("@@@@@@@@@@@@@@@@SampleComponent eUpdateSystemSetting@@@@@@@@@@@@@@@@@");
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      setting_changed_info_["Language"] = std::make_pair(GetCurrentTimeToString(), "None");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eTimeSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@@@SampleComponent eTimeSettingChanged@@@@@@@@@@@@@@@@@");
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      setting_changed_info_["Time"] = std::make_pair(GetCurrentTimeToString(), "None");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eAnalyticsSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eAnalyticsSettingChanged@@@@@@@@@@@@@@");
      HandleSettingChange(event);
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
#if 0
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eAIBasedExposureSettingChanged): {
      DebugLog("@@@@@@@@@@@@SampleComponent eAIBasedExposureSettingChanged@@@@@@@@@@@");
      DebugLog("Channel %d exposure setting is changed!", event->GetChannel());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
#endif
    case static_cast<int32_t>(IAppDispatcher::EEventType::eHttpRequest): {
      ParseHttpEvent(event);
      break;
    }
    default:
      Component::ProcessAEvent(event);
  }
  return true;
}

void SampleComponent::HandleSettingChange(Event* event) {
  auto ret = eventToArgumentBuffer(event);
  SerializableString s;
  s.DeserializeBaseObject(&s, ret);
  DebugLog(s.GetString().c_str());

  rapidjson::Document doc;
  doc.Parse(s.GetString());

  if (doc.HasMember("Channel") && doc.HasMember("EventType")) {
    setting_changed_info_["Analytics"] = std::make_pair(GetCurrentTimeToString(), std::to_string(doc["Channel"].GetInt()));
  }
}

void SampleComponent::RegisterOpenAPIURI() {
  auto get_puts = Vector<String>{};
  get_puts.push_back("GET");

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/configuration"), GetInstanceName(), get_puts);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, uriRequest);
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  DebugLog("%s, %s:%d, %s", GetObjectName(), __func__, __LINE__, path_info.c_str());

  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto& alloc = document.GetAllocator();

  auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);

  for (auto& info : setting_changed_info_) {
    auto object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
    object.AddMember("setting_type", info.first, alloc);
    object.AddMember("latest_changed", info.second.first, alloc);
    object.AddMember("channel", info.second.second, alloc);

    array.PushBack(object, alloc);
  }

  document.AddMember("status", array, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

std::string SampleComponent::GetCurrentTimeToString() {
  auto now = std::chrono::system_clock::now();

  auto now_time_t = std::chrono::system_clock::to_time_t(now);
  auto now_tm = ::gmtime(&now_time_t);

  std::stringstream ss;
  ss << std::put_time(now_tm, "%FT%T");
  return ss.str();
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}
