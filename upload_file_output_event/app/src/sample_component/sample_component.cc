#include "sample_component.h"

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

      DebugLog(body.c_str());

      document.Parse(body);
      if (document.HasParseError()) {
        oas->SetStatusCode(400);
        oas->SetResponseBody("request body parse error");
        return false;
      }

      if (document["mode"].GetString() == std::string_view("ftp")) {

        if(document["file_type"].GetString() != std::string("")){info_list.app_attribute_info.file_type = document["file_type"].GetString();}
        if(document["file_path"].GetString() != std::string("")){info_list.app_attribute_info.file_path = document["file_path"].GetString();}
        if(document["event_info"].GetString() != std::string("")){info_list.app_attribute_info.event_info = document["event_info"].GetString();}
        if(document["report_name"].GetString() != std::string("")){info_list.app_attribute_info.report_name = document["report_name"].GetString();}

        WriteAttributes(&info_list, GetObjectName());
        
        document.AddMember("app_name", manifest_.app_name, alloc);
        document["file_type"].SetString(info_list.app_attribute_info.file_type, alloc);
        document["file_path"].SetString(info_list.app_attribute_info.file_path, alloc);
        document["event_info"].SetString(info_list.app_attribute_info.event_info, alloc);
        document["report_name"].SetString(info_list.app_attribute_info.report_name, alloc);

        rapidjson::StringBuffer strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
        document.Accept(writer);

        SendNoReplyEvent((uint64_t)Component::EReceivers::eOpenPlatformManager, (int32_t)IPOpenPlatformManager::EAppEventType::eAppFtpFileUpload, 0,
                     new ("Query") SerializableString(strbuf.GetString()));
      } else if (document["mode"].GetString() == std::string_view("email")) {

        if(document["file_type"].GetString() != std::string("")){info_list.app_attribute_info.file_type = document["file_type"].GetString();}
        if(document["file_path"].GetString() != std::string("")){info_list.app_attribute_info.file_path = document["file_path"].GetString();}
        if(document["subject"].GetString() != std::string("")){info_list.app_attribute_info.subject = document["subject"].GetString();}
        if(document["event_info"].GetString() != std::string("")){info_list.app_attribute_info.event_info = document["event_info"].GetString();}
        if(document["report_name"].GetString() != std::string("")){info_list.app_attribute_info.report_name = document["report_name"].GetString();}

        WriteAttributes(&info_list, GetObjectName());
        
        document.AddMember("app_name", manifest_.app_name, alloc);
        document["file_type"].SetString(info_list.app_attribute_info.file_type, alloc);
        document["file_path"].SetString(info_list.app_attribute_info.file_path, alloc);
        document["subject"].SetString(info_list.app_attribute_info.subject, alloc);
        document["event_info"].SetString(info_list.app_attribute_info.event_info, alloc);
        document["report_name"].SetString(info_list.app_attribute_info.report_name, alloc);

        rapidjson::StringBuffer strbuf;
        rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
        document.Accept(writer);
    
        SendNoReplyEvent((uint64_t)Component::EReceivers::eOpenPlatformManager, (int32_t)IPOpenPlatformManager::EAppEventType::eAppSendEmailNoti, 0,
                     new ("Query") SerializableString(strbuf.GetString()));
      } else {
        oas->SetStatusCode(400);
        oas->SetResponseBody("requested mode is not supported"); 
        return false;
      }
      return true;
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