
#include "sample_component.h"

#include <dispatcher_serialize.h>
#include <i_app_dispatcher.h>
#include <i_app_network_manager.h>

#include <fstream>

#include "typedef_application.h"

constexpr ClassID _SampleComponent_Id = GET_CLASS_UID(_ELayer_Application::_eSampleComponent);

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name), client_id_(-1) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  this->RegisterOpenAPIURI();
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    case static_cast<int32_t>(IAppDispatcher::EEventType::eHttpRequest): {
      ParseHttpEvent(event);
      break;
    }
    case static_cast<int32_t>(IAppNetworkManager::EEventType::eNewClientConnected): {
      int arg = event->GetArgument();
      std::cout << "Client ID number " << arg << " is connected" << std::endl;
      this->client_id_ = arg;
      this->SendImage();
      break;
    }
    case static_cast<int>(IAppNetworkManager::EEventType::eClientDisconnected): {
      int arg = event->GetArgument();
      std::cout << "Client ID number " << arg << " is disconnected" << std::endl;
      break;
    }
    case static_cast<int>(IAppNetworkManager::EEventType::eServerData): {
      auto* data = reinterpret_cast<NetworkBufferData*>(event->GetBaseObjectArgument());
      if (data == nullptr) {
        std::cout << "data is nullptr" << std::endl;
        break;
      }
      std::cout << "buffer: " << data->buffer() << std::endl;
      std::cout << "buffer size:" << data->buffer_size() << std::endl;
      break;
    }
    case static_cast<int>(IAppNetworkManager::EEventType::eClientData): {
      auto* data = reinterpret_cast<NetworkBufferData*>(event->GetBaseObjectArgument());
      if (data == nullptr) {
        std::cout << "data is nullptr" << std::endl;
        break;
      }
      std::cout << "client id:" << data->client_id() << std::endl;
      std::cout << "buffer: " << data->buffer() << std::endl;
      std::cout << "buffer size:" << data->buffer_size() << std::endl;
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
  get_puts.push_back("POST");

  auto* start_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/startserver"), GetInstanceName(), get_puts);
  auto* stop_request = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/stopserver"), GetInstanceName(), get_puts);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, start_request);
  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, stop_request);
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");
  auto body = oas->GetRequestBody();

  if (path_info == "/startserver") {
    StartServer(body);
  } else if (path_info == "/stopserver") {
    StopServer();
  }

  return true;
}

void SampleComponent::StartServer(const std::string& request) {
  std::cout << "Start Server!" << std::endl;
  JsonUtility::JsonDocument doc;
  doc.Parse(request);

  if (doc.HasMember("app_id") && doc.HasMember("port")) {
    int port = std::stoi(doc["port"].GetString());
    std::cout << "Port:" << port << std::endl;
    auto* arg = new ("NetworkConfig") NetworkConfig((int)IAppNetworkManager::EServiceType::eServer, (int)IAppNetworkManager::ESocketType::eTCP, "", port);
    SendNoReplyEvent("AppNetworkManager", (int)IAppNetworkManager::EEventType::eStartService, 0, arg);
  }
}

void SampleComponent::StopServer() {
  std::cout << "Stop Server!" << std::endl;
  SendNoReplyEvent("AppNetworkManager", (int)IAppNetworkManager::EEventType::eStopService);
}

void SampleComponent::SendImage() {
  std::cout << "Send Image!" << std::endl;

  std::string image_path = "../res/image.jpg";
  std::string response = GetHttpResponse(image_path);

  std::cout << "response: " << response.c_str() << std::endl;

  if (this->client_id_ > 0) {
    auto* arg = new ("NetworkBufferData") NetworkBufferData(response.c_str(), response.length(), this->client_id_);
    SendNoReplyEvent("AppNetworkManager", (int)IAppNetworkManager::EEventType::eSendData, 0, arg);
  }
}

std::string SampleComponent::GetHttpResponse(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file) {
    return "HTTP/1.1 404 Not Found\r\n\r\n";
  }

  std::ostringstream response;
  response << "HTTP/1.1 200 OK\r\n";
  response << "Content-Length: " << file.seekg(0, std::ios::end).tellg() << "\r\n";
  response << "Content-Type: image/jpeg\r\n\r\n";
  file.seekg(0, std::ios::beg);
  response << file.rdbuf();
  file.close();

  return response.str();
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}