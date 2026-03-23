#include "classification.h"

#include <chrono>
#include <memory>

#include "data_context.h"
#include "i_data_manager.h"
#include "i_exception_manager.h"

#include "pl_video_frame_raw.h"
#include "i_p_metadata_manager.h"
#include "i_metadata_manager.h"
#include "i_opensdk_cgi_dispatcher.h"
#include "i_p_stream_provider_manager_video_raw.h"
#include "i_p_video_frame_raw.h"

using namespace std;
using namespace chrono;

HandDetector::HandDetector()
  : HandDetector(kComponentId, "HandDetector")
{
}

HandDetector::HandDetector(ClassID id, const char* name)
  : base(id, name)
{
}

HandDetector::~HandDetector()
{
}

bool HandDetector::Initialize()
{
  info_list_ = std::make_shared<HandDetectorInfoList>(GetStringComponentVersion());
  PrepareAttributes(info_list_.get(), GetObjectName());

  std::string manifest_path = "../../config/app_manifest.json";
  ParseManifest(manifest_path, manifest_);

  if (GetChannel() == 0) {
    RegisterOpenAPIURI();
  }

  DebugLog("HandDetector Initialize (channel=%d)", GetChannel());
  return true;
}

bool HandDetector::ParseManifest(const std::string& manifest_path, ManifestInfo& info)
{
  std::ifstream manifest_stream(manifest_path.c_str());
  if (!manifest_stream.is_open()) {
    DebugLog("Fail to parse the app manifest file! (%s)", manifest_path.c_str());
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

void HandDetector::RegisterOpenAPIURI()
{
  Vector<String> methods;
  methods.push_back("POST");
  methods.push_back("GET");

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(
      String("/stream"), GetInstanceName(), methods);

  SendNoReplyEvent("AppDispatcher",
                   static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand),
                   0, uriRequest);
}

void HandDetector::Start()
{
  base::Start();
  DebugLog("HandDetector Start");
}

bool HandDetector::Finalize()
{
  DebugLog("HandDetector Finalize");
  return Component::Finalize();
}

bool HandDetector::ProcessAEvent(Event* event)
{
  bool result = true;

  switch (event->GetType())
  {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest:
    {
      auto param = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
      std::string path_info = param->GetFCGXParam("PATH_INFO").c_str();

      if (path_info == "/stream") {
        std::string method = param->GetMethod();
        if (method == "POST" || method == "GET") {
          auto body = param->GetRequestBody();
          result = HandleStreamRequest(param, body);
          break;
        }
        param->SetStatusCode(405);
        param->SetResponseBody("{\"error\":\"method not allowed\"}");
        break;
      }

      param->SetStatusCode(404);
      param->SetResponseBody("{\"error\":\"not found\"}");
      break;
    }
    case (int32_t)IAppDispatcher::EEventType::eHttpResponse:
      break;

    case (int32_t)ComponentInterface::EProtocolEventType::eSet:
      break;

    case (int32_t)ComponentInterface::EProtocolEventType::eRemove:
      break;

    case static_cast<int32_t>(IPStreamProviderManagerVideoRaw::EEventType::eVideoRawData):
    {
      // Phase 2에서 구현: Raw Video 수신 및 NPU 추론 파이프라인
      break;
    }
    default:
      base::ProcessAEvent(event);
      break;
  }

  return result;
}

bool HandDetector::HandleStreamRequest(OpenAppSerializable* param, const std::string& body)
{
  std::string mode;

  // query string에서 mode 추출
  std::string query = param->GetFCGXParam("QUERY_STRING").c_str();
  if (!query.empty()) {
    auto pos = query.find("mode=");
    if (pos != std::string::npos) {
      mode = query.substr(pos + 5);
      auto end = mode.find('&');
      if (end != std::string::npos) mode = mode.substr(0, end);
    }
  }

  // body JSON에서 mode 추출
  if (mode.empty() && !body.empty()) {
    JsonUtility::JsonDocument doc(JsonUtility::Type::kObjectType);
    doc.Parse(body);
    if (!doc.HasParseError() && doc.HasMember("mode")) {
      mode = doc["mode"].GetString();
    }
  }

  if (mode == "start") {
    run_flag_ = true;
    DebugLog("HandDetector: inference started");
    param->SetStatusCode(200);
    param->SetResponseBody("{\"result\":\"ok\",\"state\":\"running\"}");
    return true;
  }
  else if (mode == "stop") {
    run_flag_ = false;
    DebugLog("HandDetector: inference stopped");
    param->SetStatusCode(200);
    param->SetResponseBody("{\"result\":\"ok\",\"state\":\"stopped\"}");
    return true;
  }
  else if (mode == "info") {
    auto& attr = info_list_->app_attribute_info;
    std::string resp =
        std::string("{") +
        "\"state\":\"" + (run_flag_ ? "running" : "stopped") + "\"," +
        "\"model\":\"hand_yolov11n.nb\"," +
        "\"confidence_threshold\":" + std::to_string(attr.confidence_threshold) + "," +
        "\"nms_iou_threshold\":" + std::to_string(attr.nms_iou_threshold) + "," +
        "\"skip_frames\":" + std::to_string(attr.skip_frames) + "," +
        "\"alarm_on_threshold\":" + std::to_string(attr.alarm_on_threshold) + "," +
        "\"alarm_off_threshold\":" + std::to_string(attr.alarm_off_threshold) +
        "}";
    param->SetStatusCode(200);
    param->SetResponseBody(resp);
    return true;
  }
  else if (mode == "config") {
    if (body.empty()) {
      param->SetStatusCode(400);
      param->SetResponseBody("{\"error\":\"empty body\"}");
      return false;
    }
    JsonUtility::JsonDocument doc(JsonUtility::Type::kObjectType);
    doc.Parse(body);
    if (doc.HasParseError()) {
      param->SetStatusCode(400);
      param->SetResponseBody("{\"error\":\"invalid json\"}");
      return false;
    }

    auto& attr = info_list_->app_attribute_info;
    JsonUtility::get(doc, "confidence_threshold", attr.confidence_threshold);
    JsonUtility::get(doc, "nms_iou_threshold", attr.nms_iou_threshold);
    JsonUtility::get(doc, "skip_frames", attr.skip_frames);
    JsonUtility::get(doc, "mqtt_broker_host", attr.mqtt_broker_host);
    JsonUtility::get(doc, "mqtt_broker_port", attr.mqtt_broker_port);
    JsonUtility::get(doc, "alarm_on_threshold", attr.alarm_on_threshold);
    JsonUtility::get(doc, "alarm_off_threshold", attr.alarm_off_threshold);
    JsonUtility::get(doc, "alarm_cooldown_sec", attr.alarm_cooldown_sec);

    WriteAttributes(info_list_.get(), this->GetObjectName());
    DebugLog("HandDetector: config updated (conf=%.2f, nms=%.2f)",
             attr.confidence_threshold, attr.nms_iou_threshold);
    param->SetStatusCode(200);
    param->SetResponseBody("{\"result\":\"ok\"}");
    return true;
  }

  param->SetStatusCode(400);
  param->SetResponseBody("{\"error\":\"unknown mode\"}");
  return false;
}

NeuralNetwork* HandDetector::GetOrCreateNetwork(const std::string& name)
{
  auto* network = GetNetwork(name);
  if (!network) {
    network = NeuralNetwork::Create();
    nn_map_[name].reset(network);
  }
  return network;
}

NeuralNetwork* HandDetector::GetNetwork(const std::string& name)
{
  auto it = nn_map_.find(name);
  if (it != nn_map_.end()) return it->second.get();
  return nullptr;
}

HandDetector::NeuralNeworkMap& HandDetector::GetAllNetworks()
{
  return nn_map_;
}

void HandDetector::RemoveNetwork(const std::string& name)
{
  auto it = nn_map_.find(name);
  if (it != nn_map_.end()) {
    it->second.reset();
    nn_map_.erase(it);
  }
}

extern "C"
{
  HandDetector* create_component(void* mem_manager)
  {
    Component::allocator = decltype(Component::allocator)(mem_manager);
    Event::allocator = decltype(Event::allocator)(mem_manager);
    return new ("HandDetector") HandDetector();
  }

  void destroy_component(HandDetector* ptr)
  {
    delete ptr;
  }
}
