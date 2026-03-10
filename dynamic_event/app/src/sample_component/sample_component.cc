// Cheack Add Schema(Json):  https://<CameraIP>/stw-cgi/eventstatus.cgi?msubmenu=eventstatusschema&action=view
// Cheack Add Schema(xml):  https://<CameraIP>/stw-cgi/eventstatus.cgi?msubmenu=metadataschema&action=view
#include "includes/sample_component.h"

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "i_eventstatus_cgi_dispatcher.h"
#include "i_metadata_manager.h"
#include "i_p_metadata_manager.h"
#include "i_opensdk_cgi_dispatcher.h"
#include "life_cycle_manager_openapp.h"

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {
  schema_infos_.insert({
      {"metadata_schema", ""},
      {"event_status_schema", ""},
      {"meta_frame_schema", ""},
      {"meta_frame_capability", ""},
      {"event_status_change", ""},
      {"dynamic_event", ""},
  });
}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  bool result = true;
  switch (event->GetType()) {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest: {
      result = ParseHttpEvent(event);
      break;
    }
    case static_cast<int32_t>(I_EventstatusCGIDispatcher::EEventType::eMetadataSchema): {
      SendMetadataSchema(event);
      break;
    }
    case static_cast<int32_t>(I_EventstatusCGIDispatcher::EEventType::eEventstatusSchema): {
      SendEventStatusSchema(event);
      break;
    }
    case static_cast<int32_t>(I_EventstatusCGIDispatcher::EEventType::eEventStatusCheck): {
      // Event Notify 는 크게 2가지 경우 전송 되어야 한다.
      // - eEventStatusCheck 요청을 받은 경우 ( 초기 이벤트 상태 확인 용도 )
      // - 이벤트의 상태 변화가 발생한 경우
      NotifyChannelEvent();
      break;
    }
    case static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameSchema): {
      SendMetaFrameSchema(event);
      break;
    }
    case static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameCapability): {
      SendMetaFrameCapability(event);
      break;
    }
    case static_cast<int32_t>(LifeCycleManagerOpenApp::EEventType::eInformAppInfo): {
      auto blob = event->GetBlobArgument();
      auto base_object = blob.GetBaseObject();
      if (base_object == nullptr) {
        result = false;
        break;
      }

      auto str = *(static_cast<String*>(base_object));
      JsonUtility::JsonDocument document;
      document.Parse(str.c_str());
      if (document.HasMember("AppId") && document["AppId"].IsString()) {
        app_id_ = document["AppId"].GetString();
      }
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
      document.Parse(body);
      if (document.HasParseError()) {
        oas->SetStatusCode(400);
        oas->SetResponseBody("request body parse error");
        return false;
      }

      if(document["mode"].GetString() == std::string("") 
      || document["data"].GetString() == std::string("")){
        oas->SetStatusCode(400);
        oas->SetResponseBody("Input values error");
        return false;
      }

      std::string_view mode = document["mode"].GetString();
      std::string_view data = document["data"].GetString();
      int channel = 0;
      if (document.HasMember("channel")) {
        channel = document["channel"].GetInt();
      }

      if (mode == "event_status_change") {
        NotifyChannelEvent(event);
        NotifyEventMetadata(event);
      } else if (mode == "dynamic_event") {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        auto metadata = StringMetadata(channel, timestamp);
        metadata.Set(data);

        auto req = new ("MetadataRequest") IPMetadataManager::StringMetadataRequest();
        req->SetStringMetadata(std::move(metadata));
        req->SetAppID(app_id_);

        SendNoReplyEvent("MetadataManager", (int32_t)IMetadataManager::EEventType::eRequestRawMetadata, 0, req);
      }
// This code is for checking the registered schema in the web UI.
      JsonUtility::JsonDocument res_document(JsonUtility::Type::kObjectType);
      auto& alloc = res_document.GetAllocator();
      auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);
      for (auto& info : schema_infos_) {
        if(info.first == mode){
          auto object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
          object.AddMember("setting_type", info.first, alloc);
          object.AddMember("schema", info.second, alloc);

          array.PushBack(object, alloc);
        }
      }
      res_document.AddMember("status", array, alloc);
      rapidjson::StringBuffer strbuf;
      rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
      res_document.Accept(writer);

      oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());
//--------------------------------------------------------------
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

void SampleComponent::SendMetadataSchema(Event* event) {
  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto alloc = document.GetAllocator();

  std::string meta_schema =
      "<tns1:OpenApp><dynamic_event>"
      "<TestSingleEvent wstop:topic=\"true\"><tt:MessageDescription IsProperty=\"true\">"
      "<tt:Source>"
      "<tt:SimpleItemDescription Name=\"VideoSourceToken\" Type=\"tt:ReferenceToken\"/>"
      "<tt:SimpleItemDescription Name=\"RuleName\" Type=\"xsd:string\"/>"
      "</tt:Source>"
      "<tt:Data>"
      "<tt:SimpleItemDescription Name=\"State\" Type=\"xsd:boolean\"/>"
      "</tt:Data>"
      "</tt:MessageDescription></TestSingleEvent></dynamic_event></tns1:OpenApp>";

  rapidjson::Value prop(rapidjson::kObjectType);
  prop.AddMember("EventName", "OpenSDK.dynamic_event.TestSingleEvent", alloc);        // "OpenSDK." + <AppName> + "." + <EventName>
  prop.AddMember("EventTopic", "tns1:OpenApp/dynamic_event/TestSingleEvent", alloc);  // "tns1:OpenApp/" + <AppName> + "/" + <EventName>
  prop.AddMember("EventSchema", meta_schema, alloc);

  rapidjson::Value onvif(rapidjson::kObjectType);
  onvif.AddMember("EventName", "OpenSDK.dynamic_event.TestSingleEvent", alloc);
  onvif.AddMember("EventTopic", "tns1:OpenApp/dynamic_event/TestSingleEvent", alloc);
  onvif.AddMember("EventSchema", meta_schema, alloc);

  document.AddMember("PROPRIETARY", prop, alloc);
  document.AddMember("ONVIF", onvif, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);
  schema_infos_["metadata_schema"] = strbuf.GetString();
  SendNoReplyEvent("OpenEventDispatcher", event->GetType(), 0, new ("MetadataSchema") String(strbuf.GetString()));
}

void SampleComponent::SendEventStatusSchema(Event* event) {
  auto query = new ("Schema") String(
      "{"
      "\"JSON\": {"
      "\"type\": \"object\","
      "\"properties\": {"
      "\"Time\": {"
      "\"type\": \"string\""
      "},"
      "\"EventName\": {"
      "\"enum\": ["
      "\"OpenSDK.dynamic_event.TestSingleEvent\""
      "]"
      "},"
      "\"Source\": {"
      "\"type\": \"object\","
      "\"properties\": {"
      "\"Channel\": {"
      "\"type\": \"number\""
      "},"
      "\"AppName\": {"
      "\"type\": \"string\""
      "},"
      "\"AppEvent\": {"
      "\"type\": \"string\""
      "},"
      "\"AppID\": {"
      "\"type\": \"string\""
      "},"
      "\"Type\": {"
      "\"enum\": ["
      "\"Event\""
      "]"
      "},"
      "\"RuleIndex\": {"
      "\"type\": \"number\""
      "},"
      "\"VideoSourceToken\": {"
      "\"type\": \"string\""
      "},"
      "\"RuleName\": {"
      "\"type\": \"string\""
      "}"
      "}"
      "},"
      "\"Data\": {"
      "\"type\": \"object\","
      "\"properties\": {"
      "\"State\": {"
      "\"type\": \"boolean\""
      "}"
      "}"
      "}"
      "}"
      "},"
      "\"TEXT\": {"
      "\"SCHEME\": "
      "\"Name=OpenSDK.dynamic_event.TestSingleEvent\\nSchema.1.Name=Channel.<int>.OpenSDK.dynamic_event.TestSingleEvent\\nSchema.1.Value=<boolean>\\nSchema.2.Name=Channel.<"
      "int>.OpenSDK.dynamic_event.TestSingleEvent.<int>.VideoSourceToken\\nSchema.2.Value=<string>\\nSchema.3.Name=Channel.<int>.OpenSDK.dynamic_event.TestSingleEvent.<int>."
      "RuleName\\nSchema.3.Value=<string>\\nSchema.4.Name=Channel.<int>.OpenSDK.dynamic_event.TestSingleEvent.<int>.State\\nSchema.4.Value=<boolean>\""
      "}"
      "}");
  schema_infos_["event_status_schema"] = query->GetString();
  SendNoReplyEvent("OpenEventDispatcher", event->GetType(), 0, query);
}

void SampleComponent::SendMetaFrameSchema(Event* event) {
  if (!event->IsReply()) {
    std::string meta_capa =
        "{"
        "\"AppID\": \"dynamic_event\","
        "\"Schema\": \"\","
        "\"Encoding\": \"base64\""
        "}";
    schema_infos_["meta_frame_schema"] = meta_capa;
    SendNoReplyEvent("Stub::Dispatcher::OpenSDK", (int32_t)I_OpenSDKCGIDispatcher::EEventType::eMetaFrameSchema, 0,
                     new ("") SerializableString(meta_capa.c_str()));
  }
}

void SampleComponent::SendMetaFrameCapability(Event* event) {
  if (!event->IsReply()) {
    std::string meta_capa =
      "{"
      "\"AppID\": \"dynamic_event\","
      "\"Capabilities\": ["
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Class/tt:Type\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Face\",\"Human\",\"Vehicle\",\"LicensePlate\",\"Head\",\"Unknown\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Class/tt:Type/@Likelihood\","
      "\"type\": \"xs:float\","
      "\"minimum\": 0.0,"
      "\"maximum\": 1.0"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Color/tt:ColorCluster/tt:ColorString\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Yellow\",\"White\",\"Red\",\"Purple\",\"Orange\",\"Gray\",\"Green\",\"Blue\",\"Black\",\"Other\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:VehicleInfo/tt:Type\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Bicycle\",\"Car\",\"Motorcycle\",\"Bus\",\"Truck\",\"Train\",\"Unknown\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:VehicleInfo/tt:Type/@Likelihood\","
      "\"type\": \"xs:float\","
      "\"minimum\": 0.0,"
      "\"maximum\": 1.0"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:VehicleInfo/tt:Color/tt:ColorCluster/tt:ColorString\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Yellow\",\"White\",\"Red\",\"Purple\",\"Orange\",\"Gray\",\"Green\",\"Blue\",\"Black\",\"Other\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:LicensePlateInfo/tt:PlateNumber\","
      "\"type\": \"xs:string\""
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanFace/fc:Gender\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Male\",\"Female\",\"Unknown\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanFace/fc:AgeType\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Child,Young\",\"Middle\",\"Old\",\"Unknown\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanFace/fc:Accessory/fc:Opticals/fc:Wear\","
      "\"type\": \"xs:boolean\""
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanFace/fc:Accessory/fc:Mask/fc:Wear\","
      "\"type\": \"xs:boolean\""
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanFace/fc:Accessory/fc:Hat/fc:Wear\","
      "\"type\": \"xs:boolean\""
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanBody/bd:Gender\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Male\",\"Female\",\"Unknown\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanBody/bd:Clothing/bd:Hat/bd:Wear\","
      "\"type\": \"xs:boolean\""
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanBody/bd:Clothing/bd:Tops/bd:Color/tt:ColorCluster/tt:ColorString\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Yellow\",\"White\",\"Red\",\"Purple\",\"Orange\",\"Gray\",\"Green\",\"Blue\",\"Black\",\"Other\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanBody/bd:Clothing/bd:Tops/bd:Length\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Short\",\"Long\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanBody/bd:Clothing/bd:Bottoms/bd:Color/tt:ColorCluster/tt:ColorString\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Yellow\",\"White\",\"Red\",\"Purple\",\"Orange\",\"Gray\",\"Green\",\"Blue\",\"Black\",\"Other\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanBody/bd:Clothing/bd:Bottoms/bd:Length\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Short\",\"Long\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:HumanBody/bd:Belonging/bd:Bag/bd:Category\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Bag\"]"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:ProximateObjects/tt:ProximateObject/@Id\","
      "\"type\": \"xs:integer\","
      "\"minimum\": 0,"
      "\"maximum\": 2147483647"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:ProximateObjects/tt:ProximateObject/@Distance\","
      "\"type\": \"xs:float\","
      "\"minimum\": 0.0,"
      "\"maximum\": 1000.0"
      "}"
      "]"
      "}";
    schema_infos_["meta_frame_capability"] = meta_capa;
    SendNoReplyEvent("Stub::Dispatcher::OpenSDK", (int32_t)I_OpenSDKCGIDispatcher::EEventType::eMetaFrameCapability, 0,
                   new ("") SerializableString(meta_capa.c_str()));
  }
}

void SampleComponent::NotifyChannelEvent() {
  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto& alloc = document.GetAllocator();

  auto channel_event_object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
  JsonUtility::set(channel_event_object, "Channel", 0, alloc);
  JsonUtility::set(channel_event_object, "State", true, alloc);
  {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    auto time_point = std::chrono::high_resolution_clock::time_point(ms);
    auto time = std::chrono::high_resolution_clock::to_time_t(time_point);
    auto conv_time = localtime(&time);
    if (conv_time == nullptr) {
      return;
    }
    time_t time_val = mktime(conv_time);

    std::stringstream ss("");
    ss << std::put_time(gmtime(&time_val), "%FT%T.") << std::setfill('0') << std::setw(3) << ms.count() % 1000 << std::put_time(conv_time, "%z");

    auto time_str = ss.str();
    time_str.insert(time_str.size() - 2, ":");
    JsonUtility::set(channel_event_object, "Time", time_str, alloc);
  }

  JsonUtility::set(channel_event_object, "EventName", "OpenSDK.dynamic_event.TestSingleEvent", alloc);

  auto source_object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
  {
    JsonUtility::set(source_object, "Channel", 0, alloc);
    JsonUtility::set(source_object, "AppName", "dynamic_event", alloc);
    JsonUtility::set(source_object, "AppID", "dynamic_event", alloc);
    JsonUtility::set(source_object, "AppEvent", "TestSingleEvent", alloc);
    JsonUtility::set(source_object, "Type", "Event", alloc);
    JsonUtility::set(source_object, "RuleIndex", 1, alloc);
    std::string video_source_token = "vs-" + std::to_string(0);
    JsonUtility::set(source_object, "VideoSourceToken", video_source_token, alloc);
    JsonUtility::set(source_object, "RuleName", "Rule-1", alloc);
  }
  channel_event_object.AddMember("Source", source_object, alloc);

  auto data_object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
  { JsonUtility::set(data_object, "State", true, alloc); }
  channel_event_object.AddMember("Data", data_object, alloc);

  document.AddMember("ChannelEvent", channel_event_object, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);
  std::string str_out = strbuf.GetString();
  try {
    SendNoReplyEvent("OpenEventDispatcher", (int32_t)I_EventstatusCGIDispatcher::EEventType::eEventStatusCheck, 0, new ("") String(str_out.c_str()));
  } catch (Exception& e) {
  }
}

void SampleComponent::NotifyChannelEvent(Event* event) {
  if (event == nullptr || event->IsReply()) {
    return;
  }

  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto& alloc = document.GetAllocator();

  auto channel_event_object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
  JsonUtility::set(channel_event_object, "Channel", 0, alloc);
  JsonUtility::set(channel_event_object, "State", true, alloc);
  {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    auto time_point = std::chrono::high_resolution_clock::time_point(ms);
    auto time = std::chrono::high_resolution_clock::to_time_t(time_point);
    auto conv_time = localtime(&time);
    if (conv_time == nullptr) {
      return;
    }
    time_t time_val = mktime(conv_time);

    std::stringstream ss("");
    ss << std::put_time(gmtime(&time_val), "%FT%T.") << std::setfill('0') << std::setw(3) << ms.count() % 1000 << std::put_time(conv_time, "%z");

    auto time_str = ss.str();
    time_str.insert(time_str.size() - 2, ":");
    JsonUtility::set(channel_event_object, "Time", time_str, alloc);
  }

  JsonUtility::set(channel_event_object, "EventName", "OpenSDK.dynamic_event.TestSingleEvent", alloc);

  auto source_object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
  {
    JsonUtility::set(source_object, "Channel", 0, alloc);
    JsonUtility::set(source_object, "AppName", "dynamic_event", alloc);
    JsonUtility::set(source_object, "AppID", "dynamic_event", alloc);
    JsonUtility::set(source_object, "AppEvent", "TestSingleEvent", alloc);
    JsonUtility::set(source_object, "Type", "Event", alloc);
    JsonUtility::set(source_object, "RuleIndex", 1, alloc);
    std::string video_source_token = "vs-" + std::to_string(0);
    JsonUtility::set(source_object, "VideoSourceToken", video_source_token, alloc);
    JsonUtility::set(source_object, "RuleName", "Rule-1", alloc);
  }
  channel_event_object.AddMember("Source", source_object, alloc);

  auto data_object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
  if(metadata_send_count % 2 == 0){
    { JsonUtility::set(data_object, "State", false, alloc); }  
  }else {
    { JsonUtility::set(data_object, "State", true, alloc); }
  }
  channel_event_object.AddMember("Data", data_object, alloc);

  document.AddMember("ChannelEvent", channel_event_object, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);
  std::string str_out = strbuf.GetString();
  try {
    SendNoReplyEvent("OpenEventDispatcher", (int32_t)I_EventstatusCGIDispatcher::EEventType::eEventStatusChanged , 0, new ("") String(str_out.c_str()));
  } catch (Exception& e) {
  }
}

void SampleComponent::NotifyEventMetadata(Event* event) {
  uint64_t timestmap = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  int32_t state = true;
  if(metadata_send_count % 2 == 0){
    state = false;
  }

  auto metadata = StreamMetadata(GetChannel(), timestmap);

  auto event_metadata = EventMetadataItem();
  event_metadata.set_topic("tns1:OpenApp/dynamic_event/TestSingleEvent");
  event_metadata.set_timestamp(timestmap);
  event_metadata.set_property("");
  const std::string token = "vs-" + std::to_string(GetChannel());
  event_metadata.add_source({"VideoSourceToken", token});
  event_metadata.add_source({"RuleName", "Rule-1"});
  event_metadata.add_data({"State", state ? "true" : "false"});

  metadata.add_event_metadata(std::move(event_metadata));
  metadata.NeedBroadcasting(true);

  metadata_send_count++;

  SendMetadata(std::move(metadata));
}

void SampleComponent::SendMetadata(StreamMetadata&& metadata) {
  auto builder = IPMetadataManager::StreamMetadataRequest::builder();

  builder.set_app_id(app_id_);
  builder.set_stream_metadata(std::forward<StreamMetadata>(metadata));

  auto metadata_request = static_cast<IPMetadataManager::StreamMetadataRequest*>(builder.build());
  if (metadata_request == nullptr) {
    std::cout << __PRETTY_FUNCTION__ << " MetadataRequest alloc failed\n";
    return;
  }

  try {
    SendNoReplyEvent("MetadataManager", static_cast<int32_t>(IMetadataManager::EEventType::eRequestMetadata), 0, metadata_request);
  } catch (...) {
    std::cout << __FUNCTION__ << " metadata send error\n";
  }
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