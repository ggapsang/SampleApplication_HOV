
#include "sample_component.h"

#include <dispatcher_serialize.h>
#include <i_sunapi_requester.h>
#include <i_app_dispatcher.h>

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    case static_cast<int>(ISunapiRequester::EEventType::eReadyForUse): {
      this->SendSunapiRequest();
      break;
    }
    case static_cast<int>(ISunapiRequester::EEventType::eRequestSunapiDone): {
      this->HandleResponse(event);
      break;
    }
    case static_cast<int32_t>(IAppDispatcher::EEventType::eHttpRequest): {
      ParseHttpEvent(event);
      break;
    }
    default:
      Component::ProcessAEvent(event);
      break;
  }
  return true;
}

void SampleComponent::SendSunapiRequest(void) {
  std::string sunapi_request = "/stw-cgi/opensdk.cgi?msubmenu=apps&action=view";
  SendReplyEvent("SunapiRequester",
                (int32_t)ISunapiRequester::EEventType::eRequestSunapi,
                0,
                new ("")String(sunapi_request),
                (int32_t)ISunapiRequester::EEventType::eRequestSunapiDone
                );
}

void SampleComponent::HandleResponse(Event* event) {
  if(event->IsReply()) {
    std::string response(reinterpret_cast<String*>(event->GetBaseObjectArgument())->c_str());
    DebugLog("----------------------Response----------------------");
    DebugLog(response.c_str());
    DebugLog("----------------------------------------------------");

    JsonUtility::JsonDocument doc;
    doc.Parse(response.c_str());

    if (doc.HasMember("Apps")) {
      if (doc["Apps"].IsArray()) {
        app_infos_.clear();
        for (auto &arrayItr : doc["Apps"].GetArray()) {
          AppInfo app_info;
          JsonUtility::get(arrayItr, "AppID", app_info.app_id);
          JsonUtility::get(arrayItr, "AppName", app_info.app_name);
          JsonUtility::get(arrayItr, "Status", app_info.status);
          JsonUtility::get(arrayItr, "InstalledDate", app_info.installed_date);
          JsonUtility::get(arrayItr, "Version", app_info.version);

          app_infos_.push_back(app_info);
        }
      }
    }
  }
}

void SampleComponent::RegisterOpenAPIURI() {
  auto get_puts = Vector<String>{};
  get_puts.push_back("GET");
  get_puts.push_back("POST");

  auto request_url = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/request"), GetInstanceName(), get_puts);
  auto response_url = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/response"), GetInstanceName(), get_puts);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, request_url);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, response_url);
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  DebugLog("%s %s:%d, %s", GetObjectName(), __func__, __LINE__, path_info.c_str());

  if (path_info == "/response") {
    JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
    auto &alloc = document.GetAllocator();

    auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);

    for (auto &info : this->app_infos_) {
      auto object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
      object.AddMember("app_id", info.app_id, alloc);
      object.AddMember("app_name", info.app_name, alloc);
      object.AddMember("status", info.status, alloc);
      object.AddMember("installed_date", info.installed_date, alloc);
      object.AddMember("version", info.version, alloc);

      array.PushBack(object, alloc);
    }

    document.AddMember("infos", array, alloc);

    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    document.Accept(writer);

    oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());
  } else if (path_info == "/request") {
    SendSunapiRequest();
  }

  return true;
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}