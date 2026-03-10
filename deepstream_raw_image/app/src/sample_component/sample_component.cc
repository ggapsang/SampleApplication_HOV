#include "sample_component.h"

#include "i_app_dispatcher.h"
#include "i_log_manager.h"
#include "i_network_camera.h"
#include "i_opensdk_cgi_dispatcher.h"
#include "i_p_stream_provider_manager_video_raw.h"
#include "i_pl_video_frame_raw.h"
#include "life_cycle_manager_openapp.h"
#include "sunapi_requester/includes/i_sunapi_requester.h"

SampleComponent::SampleComponent()
    : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char *name)
    : Component(id, name) {
  profile_.appid_ = "deepstream_raw_image";
  profile_.channel_ = GetChannel();
  profile_.codec_ = IPAppProfileHandler::AppProfile::eCODEC_TYPE::eH264;
  profile_.width_ = 1920;
  profile_.height_ = 1080;
  profile_.framerate_ = 10;
  profile_.bitrate_ = 1500000; 
  profile_.streamuri_ = "rtsp://127.0.0.1:" + std::to_string(app_.rtsp_port) + "/deepstream_raw_image";

  app_.metadata_sender = std::unique_ptr<MetadataSender>(new ("Metadata") MetadataSender());

  AddPart(app_.metadata_sender->GetObjectId(), app_.metadata_sender.get());
}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterURI();
  return Component::Initialize();   
}

void SampleComponent::Stop() {
  LOG(GetObjectName(), __func__, __LINE__, "@stop begin");
  StopDeepStream();
  LOG(GetObjectName(), __func__, __LINE__, "@stop end");
  Component::Stop();
}

bool SampleComponent::ProcessAEvent(Event *event) {
  switch (event->GetType()) {
  case static_cast<int>(ISunapiRequester::EEventType::eReadyForUse):
    InitializeDynamicEvent();
    break;
  case static_cast<int32_t>(IAppDispatcher::EEventType::eHttpRequest):
    HandleHttpRequest(event);
    break;
  case static_cast<int32_t>(IPStreamProviderManagerVideoRaw::EEventType::eVideoRawData):
    ProcessRawVideo(event);
    break;
  case static_cast<int32_t>(INetworkCamera::EEventType::eUpdateAnalyticsProfile):
    break;
  case static_cast<int32_t>(LifeCycleManagerOpenApp::EEventType::eInformAppInfo):
    ParseAppInfo(event);
    break;
  default:
    Component::ProcessAEvent(event);
    break;
  }
  return true;
}

bool SampleComponent::HandleHttpRequest(Event *event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable *>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  LOG(GetObjectName(), __func__, __LINE__, path_info.c_str());

  if (path_info == "/stream") {
    if (oas->GetMethod() == "GET") {
      std::string query_str =
          DecodeUrl(oas->GetFCGXParam("QUERY_STRING").c_str(), true);

      LOG(GetObjectName(), __func__, __LINE__, "[QUERY] " + query_str);

      auto query_map = GetQueryMap(query_str);
      if (query_map.count("mode")) {
        auto &mode = query_map["mode"];
        if (mode == "start") {
          if (!query_map.count("ProfileIndex") || !query_map.count("ProfileName")) {
            oas->SetResponseBody("ProfileIndex or ProfileName is required");
            oas->SetStatusCode(400);
            return true;
          }
          RunDeepStream(std::stoi(query_map["ProfileIndex"]), query_map["ProfileName"]);
          oas->SetResponseBody("OK");
          oas->SetStatusCode(200);
        }
        if (mode == "stop") {
          StopDeepStream();
          oas->SetResponseBody("OK");
          oas->SetStatusCode(200);
        }
        if (mode == "view") {
          GetFrameInfo(oas);
        }
      }
      return true;
    }
  } else if(path_info == "/profile") {
    if(oas->GetMethod() == "GET") {
      auto req = std::string("/stw-cgi/media.cgi?msubmenu=videoprofile&action=view&Channel=") + std::to_string(GetChannel());
      auto res = SendReplyEventWait("SunapiRequester", static_cast<int32_t>(ISunapiRequester::EEventType::eRequestSunapi), 0, new ("")String(req));
      if(res) {
        auto argument = static_cast<String*>(res->GetBaseObjectArgument());
        if(argument == nullptr) {
          oas->SetResponseBody("Internal Server Error");
          oas->SetStatusCode(500);
          return true;
        }

        JsonUtility::JsonDocument document;
        document.Parse(argument->c_str());

        auto value = rapidjson::Pointer("/VideoProfiles/0").Get(document);
        if (value) {
          RemoveAiProfileInfo(*value);

          rapidjson::StringBuffer strbuf;
          rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
          value->Accept(writer);

          oas->SetStatusCode(200);
          oas->SetResponseBody(strbuf.GetString());
          return true;
        }
      }
    }
  }

  oas->SetStatusCode(404);
  oas->SetResponseBody("Not Supported URL");

  return true;
}

void SampleComponent::GetFrameInfo(OpenAppSerializable *oas) {
  auto document = JsonUtility::JsonDocument(JsonUtility::Type::kObjectType);
  auto &alloc = document.GetAllocator();

  auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);

  for (auto &info : GetApp().metadata_sender->GetFrameMetaInfoRef()) {
    auto object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
    object.AddMember("Sequence", info.sequence, alloc);
    object.AddMember("Timestamp", info.timestamp, alloc);
    object.AddMember("ParentID", info.parent_id, alloc);
    object.AddMember("ObjectID", info.object_id, alloc);
    object.AddMember("Category", info.category, alloc);
    object.AddMember("SX", info.sx, alloc);
    object.AddMember("SY", info.sy, alloc);
    object.AddMember("EX", info.ex, alloc);
    object.AddMember("EY", info.ey, alloc);
    array.PushBack(object, alloc);
  }
  document.AddMember("info", array, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);

  document.Accept(writer);

  oas->SetStatusCode(200);
  oas->SetResponseBody(strbuf.GetString());
}

void SampleComponent::RemoveAiProfileInfo(JsonUtility::ValueType &parent) {
  auto req = std::string("/stw-cgi/media.cgi?msubmenu=videoprofilepolicy&action=view&Channel=") + std::to_string(GetChannel());
  auto res = SendReplyEventWait("SunapiRequester", static_cast<int32_t>(ISunapiRequester::EEventType::eRequestSunapi), 0, new ("") String(req));
  if (res) {
    auto argument = static_cast<String *>(res->GetBaseObjectArgument());
    if (argument == nullptr) {
      return;
    }

    JsonUtility::JsonDocument document;
    document.Parse(argument->c_str());

    auto arr = document["VideoProfilePolicies"].GetArray();
    for (auto it = arr.Begin(); it != arr.End(); ++it) {
      auto obj = it->GetObject();
      if (obj.HasMember("Channel") && obj["Channel"].GetInt() == GetChannel()) {
        if (obj.HasMember("AIProfile")) {
          int profile = obj["AIProfile"].GetInt();
          auto &profiles = parent["Profiles"];
          for (auto pit = profiles.Begin(); pit != profiles.End(); ++pit) {
            if ((*pit)["Profile"].GetInt() == profile) {
              profiles.Erase(pit);
            }
          }
        }
      }
    }
  }
}

void SampleComponent::RunDeepStream(int profile_index, const std::string& profile_name) {
  StartPipeline(GetApp(), GetAppProfile());

  GetAppProfile().appid_ = app_id_;
  GetAppProfile().profile_ = profile_index;
  GetAppProfile().name_ = profile_name;

  auto app_profile = new ("Profile") IPAppProfileHandler::AppProfile(GetAppProfile());

  LOG(GetObjectName(), __func__, __LINE__, "app_profile->appid_ : " + app_profile->appid_);
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->profile_ : " + std::to_string(app_profile->profile_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->name_ : " + app_profile->name_);
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->channel_ : " + std::to_string(app_profile->channel_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->codec_ : " + std::to_string(app_profile->codec_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->width_ : " + std::to_string(app_profile->width_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->height_ : " + std::to_string(app_profile->height_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->framerate_ : " + std::to_string(app_profile->framerate_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->bitrate_ : " + std::to_string(app_profile->bitrate_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->streamuri_ : " + app_profile->streamuri_);

  SendNoReplyEvent("AppProfileHandler", static_cast<int32_t>(IPAppProfileHandler::EEventType::eRegisterAppProfile), 0, app_profile);
}

void SampleComponent::StopDeepStream() {
  auto app_profile = new ("Profile") IPAppProfileHandler::AppProfile(GetAppProfile());

  LOG(GetObjectName(), __func__, __LINE__, "app_profile->appid_ : " + app_profile->appid_);
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->profile_ : " + std::to_string(app_profile->profile_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->name_ : " + app_profile->name_);
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->channel_ : " + std::to_string(app_profile->channel_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->codec_ : " + std::to_string(app_profile->codec_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->width_ : " + std::to_string(app_profile->width_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->height_ : " + std::to_string(app_profile->height_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->framerate_ : " + std::to_string(app_profile->framerate_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->bitrate_ : " + std::to_string(app_profile->bitrate_));
  LOG(GetObjectName(), __func__, __LINE__, "app_profile->streamuri_ : " + app_profile->streamuri_);
  auto res = SendReplyEventWait("AppProfileHandler", static_cast<int32_t>(IPAppProfileHandler::EEventType::eUnregisterAppProfile), 0, app_profile);
  
  if(res) {
    StopPipeline(GetApp());
  }
}

void SampleComponent::RegisterURI() {
  auto methods = Vector<String>{};
  methods.push_back("GET");

  SendNoReplyEvent(
      "AppDispatcher",
      static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0,
      new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(
          String("/stream"), GetInstanceName(), methods));

  SendNoReplyEvent(
      "AppDispatcher",
      static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0,
      new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(
          String("/profile"), GetInstanceName(), methods));
}

void SampleComponent::ProcessRawVideo(Event* event) {
  if (event == nullptr || event->IsReply()) {
    return;
  }

  auto blob = event->GetBlobArgument();
  std::pair<std::variant<BaseObject*, char*>, uint64_t> ret((char*)blob.GetRawData(), blob.GetSize());

  IPLVideoFrameRaw raw_frame;
  raw_frame.DeserializeBaseObject(&raw_frame, ret);

  auto& shared_raw = raw_frame.GetShared();
  if(shared_raw == nullptr) {
    return;
  }

  if(!GetApp().is_playing) {
    return;
  }

  for (auto ptr = shared_raw.get(); ptr; ptr = ptr->next) {
    if (ptr->format == raw_format_type::RAW_FMT_NV12 && ptr->dmabuf_fd != -1) {
      FeedFrame(GetApp(), ptr->pts, ptr->dmabuf_fd);
      break;
    } 
  }
}

void SampleComponent::ParseAppInfo(Event *event) {
  if (event == nullptr || event->IsReply()) {
    return;
  }

  auto param = static_cast<String *>(event->GetBaseObjectArgument());
  if (param == nullptr) {
    return;
  }

  LOG(GetObjectName(), __func__, __LINE__, param->c_str());

  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);

  document.Parse(param->c_str());

  if (document.HasMember("AppName") && document["AppName"].IsString()) {
    app_id_ = document["AppName"].GetString();
  } else {
    app_id_ = "deepstream_raw_image";
  }

  app_.metadata_sender->set_source(app_id_);
}

std::string SampleComponent::DecodeUrl(const char *source,
                                       bool bSupportPlusEncoding) {
  std::stringstream ss;
  int num = 0;
  int retval = 0;
  while (*source) {
    if (*source == '%') {
      num = 0;
      retval = 0;
      for (int i = 0; i < 2; i++) {
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
      ss.put(retval);
    } else if (bSupportPlusEncoding && (*source == '+')) {
      ss.put(' ');
    } else {
      ss.put(*source);
    }
    source++;
  }
  return ss.str();
}

std::vector<std::string> SampleComponent::SplitString(std::string_view target,
                                                      char c) {
  std::string temp;
  std::stringstream stringstream{target.data()};
  std::vector<std::string> result;

  while (std::getline(stringstream, temp, c)) {
    result.push_back(temp);
  }

  return result;
}

std::unordered_map<std::string, std::string>
SampleComponent::GetQueryMap(std::string_view query) {
  std::regex re(R"(([A-Za-z0-9]+)=([A-Za-z0-9.-_]+))", std::regex::optimize);
  std::smatch match;
  auto ret = std::unordered_map<std::string, std::string>{};
  for (auto &s : SplitString(query, '&')) {
    if (std::regex_match(s, match, re)) {
      ret.insert({match[1].str(), match[2].str()});
    }
  }

  return ret;
}


void SampleComponent::InitializeDynamicEvent() {
  std::string app_id = "deepstream_raw_image";
  std::string capabilities =
      "{"
      "\"xpath\": "
      "\"//tt:VideoAnalytics/tt:Frame/tt:Object/"
      "tt:Appearance/tt:Class/tt:Type\","
      "\"type\": \"xs:int\","
      "\"minimum\": 0,"
      "\"maximum\": 999"
      "},"
      "{"
      "\"xpath\": "
      "\"//tt:VideoAnalytics/tt:Frame/tt:Object/"
      "tt:Appearance/tt:Class/tt:Type/@Likelihood\","
      "\"type\": \"xs:float\","
      "\"minimum\": 0.0,"
      "\"maximum\": 1.0"
      "}";

  std::string str_out = "{\"AppID\": \"" + app_id + "\",\"Capabilities\": [" + capabilities + "]}";

  SendNoReplyEvent("Stub::Dispatcher::OpenSDK", static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameCapability), 0,
                   new ("Schema") Platform_Std_Refine::SerializableString(str_out.c_str()));

  std::string schema =
      "<xs:complexType "
      "name=\"MetadataStream\"><xs:sequence><xs:element><xs:complexType "
      "name=\"VideoAnalytics\"><xs:sequence><xs:element><xs:complexType "
      "name=\"Frame\"><xs:sequence><xs:element><xs:complexType "
      "name=\"Object\"><xs:sequence><xs:element><xs:complexType "
      "name=\"Appearance\"><xs:sequence><xs:element><xs:complexType "
      "name=\"Class\"><xs:sequence><xs:element><xs:complexType "
      "name=\"Type\"><xs:simpleContent><xs:extension "
      "base=\"xs:string\"><xs:attribute name=\"Likelihood\" "
      "type=\"xs:float\"/></xs:extension></xs:simpleContent></xs:complexType></"
      "xs:element></xs:sequence></xs:complexType></xs:element></xs:sequence></"
      "xs:complexType></xs:element></xs:sequence><xs:attribute "
      "name=\"ObjectId\" type=\"xs:integer\"/><xs:attribute name=\"Parent\" "
      "type=\"xs:integer\"/></xs:complexType></xs:element></"
      "xs:sequence><xs:attribute name=\"UtcTime\" type=\"xs:dateTime\" "
      "use=\"required\"/></xs:complexType></xs:element></xs:sequence></"
      "xs:complexType></xs:element></xs:sequence></xs:complexType>";
  std::string encoding = "base64";  // base64, UTF-8

  auto e = Base64::encode(schema.c_str(), schema.size());
  std::string e_s(e.first.get(), e.second);

  auto d = Base64::decode(e_s.c_str(), e_s.size());
  std::string d_s(d.first.get(), d.second);

  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto alloc = document.GetAllocator();

  document.AddMember("AppID", app_id, alloc);
  document.AddMember("Schema", e_s, alloc);
  document.AddMember("Encoding", encoding, alloc);

  str_out.clear();
  getJsonString(document, str_out);

  SendNoReplyEvent("Stub::Dispatcher::OpenSDK", static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameSchema), 0,
                   new ("Schema") Platform_Std_Refine::SerializableString(str_out.c_str()));
}

extern "C" {
SampleComponent *create_component(void *mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);

  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent *ptr) { delete ptr; }
}
