#include "sample_component.h"

#include <dispatcher_serialize.h>
#include <i_app_dispatcher.h>
#include <i_device_io_manager.h>

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
    case static_cast<int32_t>(IDeviceIoManager::EEventType::eSdcardInserted): {
      DebugLog("@@@@@@@@@@@@@@@@@@@SampleComponent eSdcardInserted@@@@@@@@@@@@@@@@@@");
      auto ret = eventToArgumentBuffer(event);
      SerializableString param;
      param.DeserializeBaseObject(&param, ret);
      DebugLog(param.GetString().c_str());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

      HandleSDCardStatus(SDCardEventType::kInserted, param.GetString());
      break;
    }
    case static_cast<int32_t>(IDeviceIoManager::EEventType::eSdcardRemoved): {
      DebugLog("@@@@@@@@@@@@@@@@@@@SampleComponent eSdcardRemoved@@@@@@@@@@@@@@@@@@");
      auto ret = eventToArgumentBuffer(event);
      SerializableString param;
      param.DeserializeBaseObject(&param, ret);
      DebugLog(param.GetString().c_str());
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

      HandleSDCardStatus(SDCardEventType::kRemoved, param.GetString());
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

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/configuration"), GetInstanceName(), get_puts);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, uriRequest);
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  LOG(GetObjectName(), __func__, __LINE__, path_info);

  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto& alloc = document.GetAllocator();

  auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);
  for (auto& state : sdcard_status_) {
    auto object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
    object.AddMember("id", state.first, alloc);
    if (state.second.inserted) {
      object.AddMember("state", "inserted", alloc);
    } else {
      object.AddMember("state", "removed", alloc);
    }

    array.PushBack(object, alloc);
  }

  document.AddMember("sdcard", array, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

void SampleComponent::HandleSDCardStatus(SDCardEventType event, std::string_view param) {
  JsonUtility::JsonDocument document;
  document.Parse(param.data());
  if (document.HasParseError()) {
    return;
  }

  if (document.HasMember("sd_index")) {
    int sd_index = document["sd_index"].GetInt();
    sdcard_status_[sd_index] = SDCardState{.inserted = (event == SDCardEventType::kInserted) ? true : false};
  }
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}