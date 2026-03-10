#include "sample_component.h"

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name), Sampleapp_info_list(nullptr) {
  Sampleapp_info_list = std::make_shared<SampleAppInfoList>(GetStringComponentVersion());
}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Finalize() {
  auto item = IPStreamRequesterManagerVideo::MultilineOsdItem();

  item.Enable = false;
  item.Index = 101;
  item.PositionX = 0;
  item.PositionY = 0;
  item.OSDType = "Title";
  item.FontSize = "Small";
  item.OSDColor = "White";
  item.Transparency = "Off";
  item.OSD = "";

  IPStreamRequesterManagerVideo::MultilineExternalOsdDeliveryObject* osd =
          new ("MultilineOSD") IPStreamRequesterManagerVideo::MultilineExternalOsdDeliveryObject();
  osd->vMultilineOsd.push_back(std::move(item));
  SendNoReplyEvent("SRMgrVideo", (int32_t)IPStreamRequesterManagerVideo::EEventType::eExternalMultiLineOsd, 0, osd);
  return Component::Finalize();
}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  PrepareAttributes(Sampleapp_info_list.get(), this->GetObjectName());
  return Component::Initialize();
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

      std::cout << body << std::endl;

      document.Parse(body);
      if (document.HasParseError()) {
        oas->SetStatusCode(400);
        oas->SetResponseBody("request body parse error");
        return false;
      }

      IPStreamRequesterManagerVideo::MultilineExternalOsdDeliveryObject* osd =
          new ("MultilineOSD") IPStreamRequesterManagerVideo::MultilineExternalOsdDeliveryObject();

      std::string enable = document["enable"].GetString();
      std::string font_size = document["font_size"].GetString();
      std::string custom_font_size = document["custom_font_size"].GetString();
      std::string osd_color = document["osd_color"].GetString();
      std::string transparency = document["transparency"].GetString();
      std::string osd_message = document["osd"].GetString();

      // Normal Camera field
      auto object = document["position"].GetObject();
      std::string position_x = object["x"].GetString();
      std::string position_y = object["y"].GetString();
      // For BWC Camera Only field
      std::string osd_position = document["osd_position"].GetString();

      if(!enable.empty()){Sampleapp_info_list->app_attribute_info.enable = enable;}
      if(!font_size.empty()){Sampleapp_info_list->app_attribute_info.font_size = font_size;}
      if(!custom_font_size.empty()){Sampleapp_info_list->app_attribute_info.custom_font_size = custom_font_size;}
      if(!osd_color.empty()){Sampleapp_info_list->app_attribute_info.osd_color = osd_color;}
      if(!transparency.empty()){Sampleapp_info_list->app_attribute_info.transparency = transparency;}
      if(!osd_message.empty()){Sampleapp_info_list->app_attribute_info.osd = osd_message;}

      // Normal Camera field
      if(!position_x.empty()){Sampleapp_info_list->app_attribute_info.position_x = position_x;}
      if(!position_y.empty()){Sampleapp_info_list->app_attribute_info.position_y = position_y;}
      // For BWC Camera Only field
      if(!osd_position.empty()){Sampleapp_info_list->app_attribute_info.osd_position = osd_position;}

      WriteAttributes(Sampleapp_info_list.get(), GetObjectName());

      auto& info = Sampleapp_info_list->app_attribute_info;
      auto item = IPStreamRequesterManagerVideo::MultilineOsdItem();

      item.Enable = std::stoi(info.enable) == 0 ? false : true;
      item.Index = 101;
      item.OSDType = "Title";
      item.OSDColor = info.osd_color;
      item.Transparency = info.transparency;
      item.OSD = info.osd;

      // FontSize
      item.FontSize = info.font_size;
      // CustomFontSize
      item.CustomFontSize = std::stoi(info.custom_font_size);

      // PositionX, PositionY
      item.PositionX = std::stoi(info.position_x);
      item.PositionY = std::stoi(info.position_y);
      // OSDPosition
      item.OSDPosition = info.osd_position;

      std::cout << "SetOsdMessage [" << std::endl;
      std::cout << "\tEnable : " << item.Enable << std::endl;
      std::cout << "\tIndex : " << item.Index << std::endl;
      std::cout << "\tPosition X : " << item.PositionX << std::endl;
      std::cout << "\tPosition Y : " << item.PositionY << std::endl;
      std::cout << "\tOSD Position : " << item.OSDPosition << std::endl;
      std::cout << "\tOSD Type : " << item.OSDType << std::endl;
      std::cout << "\tFont Size : " << item.FontSize << std::endl;
      std::cout << "\tCustom Font Size : " << item.CustomFontSize << std::endl;
      std::cout << "\tOSD Color : " << item.OSDColor << std::endl;
      std::cout << "\tTransparency : " << item.Transparency << std::endl;
      std::cout << "\tOSD Message : " << item.OSD << std::endl;
      
      osd->vMultilineOsd.push_back(std::move(item));

      SendNoReplyEvent("SRMgrVideo", (int32_t)IPStreamRequesterManagerVideo::EEventType::eExternalMultiLineOsd, 0, osd);
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