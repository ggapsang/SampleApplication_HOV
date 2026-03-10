#include "sample_component.h"

#include <dispatcher_serialize.h>
#include <i_app_dispatcher.h>
#include <i_p_open_platform_manager.h>
#include <open_platform_define.h>

using namespace openplatform;

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
    case static_cast<int32_t>(IPOpenPlatformManager::EAppEventType::eUploadFile): {
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      HandleUploadFile(event);
      DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
      break;
    }
    case static_cast<int32_t>(IAppDispatcher::EEventType::eHttpRequest): {
      ParseHttpEvent(event);
      break;
    }
    default:
      Component::ProcessAEvent(event);
  }
  return true;
}

bool SampleComponent::HandleUploadFile(Event* event) {
  if (event == nullptr) {
    DebugLog("event is nullptr!");
    return false;
  }

  auto ret = eventToArgumentBuffer(event);
  AppMsgParam param;
  param.DeserializeBaseObject(&param, ret);
  auto app_name = param.GetAppName();
  auto message = param.GetMessage();

  DebugLog("app name : %s", app_name.c_str());
  DebugLog("upload file name : %s", message.c_str());
  file_info_[app_name] = message;
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

  DebugLog("%s %s:%d, %s", GetObjectName(), __func__, __LINE__, path_info.c_str());

  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto& alloc = document.GetAllocator();

  auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);

  for (auto& info : file_info_) {
    auto object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
    object.AddMember("app", info.first, alloc);
    object.AddMember("filename", info.second, alloc);

    array.PushBack(object, alloc);
  }

  document.AddMember("infos", array, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

std::string SampleComponent::DecodeUrl(const char* source, bool isSupportPlusEncoding) {
  std::stringstream stream;
  int num = 0, i = 0;
  int retval = 0;
  while (*source) {
    if (*source == '%') {
      num = 0;
      retval = 0;
      for (i = 0; i < 2; i++) {
        source++;
        if (*(source) < ':') {
          num = *(source)-48;
        } else if (*(source) > '@' && *(source) < '[') {
          num = (*(source) - 'A') + 10;
        } else {
          num = (*(source) - 'a') + 10;
        }
        if ((16 * (1 - i)) != 0) {
          num = (num * 16);
        }
        retval += num;
      }
      stream.put(retval);
    } else if (isSupportPlusEncoding && (*source == '+')) {
      stream.put(' ');
    } else {
      stream.put(*source);
    }
    source++;
  }
  return stream.str();
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}