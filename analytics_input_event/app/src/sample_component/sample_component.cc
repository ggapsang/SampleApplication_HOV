#include "sample_component.h"

#include <dispatcher_serialize.h>
#include <i_analytics_detector.h>
#include <i_app_dispatcher.h>
#include <i_configurable_alarm_in.h>
#include <i_configurable_alarm_out.h>
#include <i_p_metadata_manager.h>

namespace {
auto eventToArgumentBuffer = [](Event* event) {
  auto blob = event->GetBlobArgument();
  std::pair<std::variant<BaseObject*, char*>, uint64_t> ret((char*)blob.GetRawData(),  // variant
                                                            blob.GetSize());           // size
  return ret;
};
}

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
  case static_cast<int32_t>(IAnalyticsDetector::EEventType::eNotifyEventOccured): {
    DebugLog("---------------- Analytics eNotifyEventOccured ---------------");
    auto ret = eventToArgumentBuffer(event);
    SerializableString s;
    s.DeserializeBaseObject(&s, ret);

    EventInfo event_info;
    JsonUtility::JsonDocument doc;
    doc.Parse(s.GetString());

    if (doc.HasMember("EventType")) {
      event_info.event_type = doc["EventType"].GetString();
      printf("EventType : %s\n", doc["EventType"].GetString());
    }
    if (doc.HasMember("MessageTime")) {
      event_info.message_time = doc["MessageTime"].GetString();
      printf("MessageTime : %s\n", doc["MessageTime"].GetString());
    }
    if (doc.HasMember("State")) {
      event_info.state = doc["State"].GetBool();
      printf("State : %d\n", doc["State"].GetBool());
    }
    if (doc.HasMember("Channel")) {
      event_info.channel = std::to_string(doc["Channel"].GetInt());
      printf("Channel : %d\n", doc["Channel"].GetInt());
    } else {
      event_info.channel = "None";
    }

    if (this->event_infos_.size() > 9) {
      this->event_infos_.pop_front();
    }
    this->event_infos_.push_back(event_info);
    DebugLog("--------------------------------------------------------------");
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

void SampleComponent::RegisterOpenAPIURI() {
  auto get_puts = Vector<String>{};
  get_puts.push_back("GET");

  auto analytics_req = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/analytics"), GetInstanceName(), get_puts);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, analytics_req);
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  DebugLog("%s %s:%d, %s", GetObjectName(), __func__, __LINE__, path_info.c_str());

  std::string response;
  if (path_info == "/analytics") {
    response = HandleAnalyticsRequest();
  }

  oas->SetResponseBody(response.c_str(), response.size());

  return true;
}

std::string SampleComponent::HandleAnalyticsRequest() {
  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto& alloc = document.GetAllocator();

  auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);

  for (auto& info : this->event_infos_) {
    auto object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
    object.AddMember("event_type", info.event_type, alloc);
    object.AddMember("message_time", info.message_time, alloc);
    object.AddMember("state", info.state, alloc);
    object.AddMember("channel", info.channel, alloc);

    array.PushBack(object, alloc);
  }

  document.AddMember("infos", array, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);

  return strbuf.GetString();
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}