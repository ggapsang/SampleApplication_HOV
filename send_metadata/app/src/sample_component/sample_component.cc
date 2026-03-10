#include "includes/sample_component.h"

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "i_sunapi_requester.h"
#include <libxml/parser.h>
#include "life_cycle_manager_openapp.h"

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name), 
max_width(0), max_height(0) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  std::string manifest_path = "../../config/app_manifest.json";
  ParseManifest(manifest_path, manifest_);
  PrepareAttributes(&attributes_info, GetObjectName());
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  bool result = true;
  switch (event->GetType()) {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest: {
      result = ParseHttpEvent(event);
      break;
    }
    case static_cast<int32_t>(ComponentInterface::EEventType::eRemoteAssociateCompleted): {
      RequestMaxResolution();
      break;
    }
    case static_cast<int32_t>(ISunapiRequester::EEventType::eRequestSunapiDone): {
      if(event->IsReply()) {
        std::string response(reinterpret_cast<String*>(event->GetBaseObjectArgument())->c_str());
        ParseMaxResolution(response);
      }
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

  auto oas = reinterpret_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  std::string path_info = oas->GetFCGXParam("PATH_INFO").c_str();

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
      std::string mode = document["mode"].GetString();
      if (mode == "event_metadata") {
        if(document["topic_str"].GetString() != std::string("")){attributes_info.app_attribute_info.topic_str = document["topic_str"].GetString();}
        if(document["source_key"].GetString() != std::string("")){attributes_info.app_attribute_info.source_key = document["source_key"].GetString();}
        if(document["source_value"].GetString() != std::string("")){attributes_info.app_attribute_info.source_value = document["source_value"].GetString();}
        if(document["data_key"].GetString() != std::string("")){attributes_info.app_attribute_info.data_key = document["data_key"].GetString();}
        if(document["data_value"].GetString() != std::string("")){attributes_info.app_attribute_info.data_value = document["data_value"].GetString();}
        if(document["element_name"].GetString() != std::string("")){attributes_info.app_attribute_info.element_name = document["element_name"].GetString();}
        if(document["element_descriptor_name"].GetString() != std::string("")){attributes_info.app_attribute_info.element_descriptor_name = document["element_descriptor_name"].GetString();}
        if(document["element_type_name"].GetString() != std::string("")){attributes_info.app_attribute_info.element_type_name = document["element_type_name"].GetString();}
        if(document["element_likelihood"].GetString() != std::string("")){attributes_info.app_attribute_info.element_likelihood = document["element_likelihood"].GetString();}

        WriteAttributes(&attributes_info, GetObjectName());

        std::vector<SampleAppInfoList> info_list;
        info_list.push_back(attributes_info);

        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto event_metadata = EventMetadataItem();

        for(auto info_temp : info_list){
          event_metadata.set_topic(info_temp.app_attribute_info.topic_str.c_str());
          event_metadata.set_timestamp(timestamp);

          DebugLog("[CHECK] metadata_source[ %s : %s ]", info_temp.app_attribute_info.source_key.c_str(), info_temp.app_attribute_info.source_value.c_str());
          event_metadata.add_source({info_temp.app_attribute_info.source_key, info_temp.app_attribute_info.source_value});
          DebugLog("[CHECK] metadata_data[ %s : %s ]", info_temp.app_attribute_info.data_key.c_str(), info_temp.app_attribute_info.data_value.c_str());
          event_metadata.add_data({info_temp.app_attribute_info.data_key, info_temp.app_attribute_info.data_value});

          bool element_flag = std::stoi(document["element_flag"].GetString());
          // Input datas to Element_item
          if (element_flag) {
            EventMetadataItem::ElementItem element_item;
            DebugLog("[CHECK] Element Item[ %s : %s ]", info_temp.app_attribute_info.element_name.c_str(), info_temp.app_attribute_info.element_descriptor_name.c_str());
            element_item.element_name = info_temp.app_attribute_info.element_name;
            element_item.descriptor_name = info_temp.app_attribute_info.element_descriptor_name;
            EventMetadataItem::Candidate candidate;
            DebugLog("[CHECK] Candidate[ %s : %s ]", info_temp.app_attribute_info.element_type_name.c_str(), info_temp.app_attribute_info.element_likelihood.c_str());
            candidate.type_name = info_temp.app_attribute_info.element_type_name;
            candidate.likelihood = stof(info_temp.app_attribute_info.element_likelihood);
            element_item.candidate.push_back(candidate);
            event_metadata.add_element(std::move(element_item));
          }

          auto metadata = StreamMetadata(GetChannel(), timestamp);
          metadata.add_event_metadata(std::move(event_metadata));
          metadata.NeedBroadcasting(true);
          SendMetadata(std::move(metadata));
        }
        
      } else if (mode == "frame_metadata") {
        if(document["object_id"].GetString() != std::string("")){attributes_info.app_attribute_info.object_id = document["object_id"].GetString();}
        if(document["parent_id"].GetString() != std::string("")){attributes_info.app_attribute_info.parent_id = document["parent_id"].GetString();}
        if(document["likelihood"].GetString() != std::string("")){attributes_info.app_attribute_info.likelihood = document["likelihood"].GetString();}
        if(document["category"].GetString() != std::string("")){attributes_info.app_attribute_info.category = document["category"].GetString();}
        if(document["rect_sx"].GetString() != std::string("")){attributes_info.app_attribute_info.rect_sx = document["rect_sx"].GetString();}
        if(document["rect_ex"].GetString() != std::string("")){attributes_info.app_attribute_info.rect_ex = document["rect_ex"].GetString();}
        if(document["rect_sy"].GetString() != std::string("")){attributes_info.app_attribute_info.rect_sy = document["rect_sy"].GetString();}
        if(document["rect_ey"].GetString() != std::string("")){attributes_info.app_attribute_info.rect_ey = document["rect_ey"].GetString();}

        WriteAttributes(&attributes_info, GetObjectName());

        std::vector<SampleAppInfoList> info_list;
        info_list.push_back(attributes_info);

        for(int index = 0 ; index < info_list.size(); index ++){
          auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
          auto frame_metadata = FrameMetadataItem();
          frame_metadata.set_resolution(this->max_width, this->max_height);
          frame_metadata.set_timestamp(timestamp);

          metadata::common::Object send_object;

          send_object.object_id = std::stoi(info_list[index].app_attribute_info.object_id);
          send_object.parent_id = std::stoi(info_list[index].app_attribute_info.parent_id);
          send_object.timestamp = timestamp;
          send_object.likelihood = std::stof(info_list[index].app_attribute_info.likelihood);
          send_object.category = (metadata::common::ObjectCategory)std::stoi(info_list[index].app_attribute_info.category);
          send_object.attr_num = info_list.size();
          send_object.rect.sx = std::stoi(info_list[index].app_attribute_info.rect_sx);
          send_object.rect.ex = std::stoi(info_list[index].app_attribute_info.rect_ex);
          send_object.rect.sy = std::stoi(info_list[index].app_attribute_info.rect_sy);
          send_object.rect.ey = std::stoi(info_list[index].app_attribute_info.rect_ey);
          frame_metadata.add_object(std::move(send_object));


          DebugLog("[CHECK] frame metadata ObjectID[%d], ParentID[%d], likehood[%f] attr_num[%d], category[%d], rect[x:%d,%d y:%d,%d]", 
                send_object.object_id, send_object.parent_id, send_object.likelihood, send_object.attr_num, send_object.category,
                send_object.rect.sx, send_object.rect.ex, send_object.rect.sy, send_object.rect.ey);

          auto metadata = StreamMetadata(GetChannel(), timestamp, Metadata::MetadataType::kAI);
          metadata.set_frame_metadata(std::move(frame_metadata));
          SendMetadata(std::move(metadata));
        }
      }
      else {
        oas->SetStatusCode(400);
        oas->SetResponseBody("requested mode is not supported"); 
        return false;
      }
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

void SampleComponent::SendMetadata(class StreamMetadata&& metadata) {
  auto builder = IPMetadataManager::StreamMetadataRequest::builder();

  builder.set_app_id(app_id_);
  builder.set_stream_metadata(std::forward<class StreamMetadata>(metadata));
  auto metadata_request = reinterpret_cast<IPMetadataManager::StreamMetadataRequest*>(builder.build());
  SendNoReplyEvent("MetadataManager", static_cast<int32_t>(IMetadataManager::EEventType::eRequestMetadata), 0, metadata_request);
}

void SampleComponent::RegisterOpenAPIURI() {
  Vector<String> methods;
  methods.push_back("POST");

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/configuration"), GetInstanceName(), methods);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, uriRequest);
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

void SampleComponent::RequestMaxResolution() {
  std::string sunapi_request = "/stw-cgi/attributes.cgi/attributes/Media/Limit";
  SendReplyEvent("SunapiRequester",
              (int32_t)ISunapiRequester::EEventType::eRequestSunapi,
              0,
              new ("")String(sunapi_request),
              (int32_t)ISunapiRequester::EEventType::eRequestSunapiDone
              );
}
  
void SampleComponent::ParseMaxResolution(const std::string& response) {
  uint32_t channel = 0, width = 0, height = 0;
  xmlDocPtr xml_doc;
  do {
    xmlNodePtr xml_cur;
    xml_doc = xmlParseMemory(response.c_str(), strlen(response.c_str()));
    if (xml_doc == NULL) {
      std::stringstream ss;
      ss << "Fail to parse attributes document: " << stderr;
      break;
    }
    xml_cur = xmlDocGetRootElement(xml_doc);
    if (xml_cur == NULL) {
      xmlFreeDoc(xml_doc);
      std::stringstream ss;
      ss << "Empty attributes document: " << stderr;
      break;
    }
    if (xml_cur != nullptr) {
      xml_cur = xml_cur->xmlChildrenNode;
      while (xml_cur != nullptr) {
        if ((!xmlStrcmp(xml_cur->name, (const xmlChar*)"channel"))) {
          xmlChar* channel_number = xmlGetProp(xml_cur, (const xmlChar*)"number");
          if (channel_number != nullptr) {
            channel = (uint32_t)atoi((const char*)channel_number);
          }
          xmlFree(channel_number);

          xmlNodePtr xml_ch_ptr = xml_cur->xmlChildrenNode;
          while (xml_ch_ptr != nullptr) {
            if ((!xmlStrcmp(xml_ch_ptr->name, (const xmlChar*)"attribute"))) {
              xmlChar* name = xmlGetProp(xml_ch_ptr, (const xmlChar*)"name");
              if (name != nullptr && !xmlStrcmp(name, (const xmlChar*)"MaxResolution")) {
                xmlChar* value = xmlGetProp(xml_ch_ptr, (const xmlChar*)"value");
                if (value != nullptr){
                  char resolution_buf[64] = {
                      0,
                  };
                  snprintf(resolution_buf, sizeof(resolution_buf), "%s", value);
                  char* tok = strtok(resolution_buf, "x");
                  if (tok != NULL) {
                    width = atoi(tok);
                    tok = strtok(NULL, "x");
                    if (tok != NULL) {
                      height = atoi(tok);
                    }
                  }
                }
                xmlFree(value);
              }
              xmlFree(name);
            }
            xml_ch_ptr = xml_ch_ptr->next;
          }

          this->max_width = width;
          this->max_height = height;
          DebugLog("Channel: %d, (%d x %d)", channel, this->max_width, this->max_height);
        }
        xml_cur = xml_cur->next;
      }
    }
  } while (0);
  if (xml_doc != NULL) 
  {
    xmlFreeDoc(xml_doc);
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
