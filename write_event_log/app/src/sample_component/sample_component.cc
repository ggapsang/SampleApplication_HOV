#include "sample_component.h"

#include <i_p_open_platform_manager.h>
#include <open_platform_define.h>
#include <unistd.h>

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "life_cycle_manager_interface.h"

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  PrepareAttributes(&info_list, GetObjectName());
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  bool result = true;
  switch (event->GetType()) {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest: {
      result = ParseHttpEvent(event);
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eNetworkSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eNetworkSettingChanged@@@@@@@@@@@@@@@@");
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eRecordSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@@SampleComponent eRecordSettingChanged@@@@@@@@@@@@@@@@");
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eAnalyticsSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eAnalyticsSettingChanged@@@@@@@@@@@@@@");
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eWiseStreamSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eWiseStreamSettingChanged@@@@@@@@@@@@@@");
      DebugLog("Channel %d wise stream setting is changed!", event->GetChannel());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eProfileSettingChanged): {
      DebugLog("@@@@@@@@@@@@@@@SampleComponent eProfileSettingChanged@@@@@@@@@@@@@@");
      DebugLog("Channel %d profile setting is changed!", event->GetChannel());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    default:
      result = Component::ProcessAEvent(event);
      break;
  }
  return result;
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  if (path_info == "/configuration") {
    if (oas->GetMethod() == "POST") {
      auto body = oas->GetRequestBody();
      JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);

      std::cout << body << std::endl;

      document.Parse(body);
      if (document.HasParseError()) {
        oas->SetStatusCode(400);
        oas->SetResponseBody("request body parse error");
        return false;
      }

      if(document["log"].GetString() != std::string("")){info_list.app_attribute_info.log = document["log"].GetString();}
      WriteAttributes(&info_list, GetObjectName());

      DebugLog("[Check] Write OpenApp Event log[%s]", info_list.app_attribute_info.log.c_str());

      auto* log = new Log(Log::LogType::EVENT_LOG, Log::LogDetailType::EVENT_OPENAPP, 0, time(NULL), Platform_Std_Refine::String(info_list.app_attribute_info.log.c_str()));
      SendNoReplyEvent("LogManager", (int32_t)ILogManager::EEvent::eWrite, 0, log);
      return true;
    }
    oas->SetStatusCode(400);
    oas->SetResponseBody("requested method is not supported");
    return false;
  }
  oas->SetStatusCode(400);
  oas->SetResponseBody("requested path is not supported");
  return false;
}

void SampleComponent::RegisterOpenAPIURI() {
  Vector<String> methods;
  methods.push_back("POST");

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/configuration"), GetInstanceName(), methods);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, uriRequest);
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}