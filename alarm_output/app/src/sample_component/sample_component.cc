#include "includes/sample_component.h"

#include "dispatcher_serialize.h"
#include "i_app_dispatcher.h"
#include "i_configurable_alarm_out.h"

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

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
    default:
      result = Component::ProcessAEvent(event);
      break;
  }
  return result;
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    IConfigurableAlarmOut::RelayRequest response;
    response.ParseRequest(event);

    RelayInstance relay_instance = response.relay_instance();
    DebugLog("%s %s:%d", GetObjectName(), __FUNCTION__, __LINE__);
    DebugLog("************************************************");
    DebugLog("I/O Port Channel: %d", relay_instance.GetChannel());
    DebugLog("Duration: %d", relay_instance.GetDuration());
    DebugLog("Action: %s", relay_instance.GetAction().c_str());
    DebugLog("State: %s", relay_instance.GetState().c_str());
    DebugLog("************************************************");
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

      RelayInstance::Builder builder;

      if(document["mode"].GetString() == std::string("") 
      || document["channel"].GetString() == std::string("")
      || document["duration"].GetString() == std::string("")){
        oas->SetStatusCode(400);
        oas->SetResponseBody("Input values error");
        return false;
      }

      std::string_view mode = document["mode"].GetString();
      std::string_view channel = document["channel"].GetString();
      int duration = std::stoi(document["duration"].GetString());

      if(duration < 0 || duration > 15){
        oas->SetStatusCode(400);
        oas->SetResponseBody("request duration error");
        return false;
      }

      builder.SetAction(mode.data()).SetChannel(atoi(channel.data())).SetDuration(duration);

      SendReplyEvent("ConfigurableAlarmOut", static_cast<int32_t>(IConfigurableAlarmOut::EEvent::eRelayRequest), 0,
                     new ("RelayRequest") IConfigurableAlarmOut::RelayRequest(builder.Build()));
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