#include "sample_component.h"

#include <unistd.h>

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "i_p_open_platform_manager.h"

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  std::string manifest_path = "../../config/app_manifest.json";
  ParseManifest(manifest_path, manifest_);
  PrepareAttributes(&info_list, GetObjectName());
  return Component::Initialize();
}

bool SampleComponent::ParseManifest(const std::string& manifest_path, ManifestInfo& info) {
  std::ifstream manifest_stream(manifest_path.c_str());
  if (!manifest_stream.is_open()) {
    DebugLog("Fail to parse the app manifest file of open app!(%s)", info.app_name.c_str());
    return false;
  }

  std::stringstream ss;
  JsonUtility::JsonDocument doc = PrepareJson();

  ss << manifest_stream.rdbuf();
  doc.Parse(ss.str());

  if (doc.HasMember("Permission")) {
    if (doc["Permission"].IsArray()) {
      info.permissions.clear();
      for (auto& p : doc["Permission"].GetArray()) {
        info.permissions.push_back(p.GetString());
      }
    }
  }

  JsonUtility::get(doc, "AppName", info.app_name);
  JsonUtility::get(doc, "AppVersion", info.version);

  return true;
}

bool SampleComponent::ProcessAEvent(Event* event) {
  bool result = true;
  switch (event->GetType()) {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest: {
      result = ParseHttpEvent(event);
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
      auto& alloc = document.GetAllocator();

      document.Parse(body);
      if (document.HasParseError()) {
        oas->SetStatusCode(400);
        oas->SetResponseBody("request body parse error");
        return false;
      }

      if (document["jpeg_path"].GetString() == std::string("")) {
        document["jpeg_path"].SetString(info_list.app_attribute_info.jpeg_path, alloc);
      } else {
        info_list.app_attribute_info.jpeg_path = document["jpeg_path"].GetString();
      }
      WriteAttributes(&info_list, GetObjectName());

      document.AddMember("channel", "0", alloc);
      document.AddMember("app_name", manifest_.app_name, alloc);
      rapidjson::StringBuffer strbuf;
      rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
      document.Accept(writer);

      auto res = SendReplyEventWait((uint64_t)Component::EReceivers::eOpenPlatformManager, (int32_t)IPOpenPlatformManager::EAppEventType::eAppSnapshotJpeg, 0,
                     new ("Query") SerializableString(strbuf.GetString()));

      if(res) {
        std::ifstream ifs(info_list.app_attribute_info.jpeg_path);
        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string entireFile = oss.str();

        oas->SetStatusCode(200);
        oas->SetResponseBody(entireFile, OpenAppResponseType::FILE);
        
        return true;
      }

      oas->SetStatusCode(400);
      oas->SetResponseBody("snapshot jpeg encode failed");
      return false;
    }
    oas->SetStatusCode(400);
    oas->SetResponseBody("requested path is not supported");
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