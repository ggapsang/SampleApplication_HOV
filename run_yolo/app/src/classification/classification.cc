#include "classification.h"
#include "yolo_postprocess.h"

#include <chrono>
#include <memory>
#include <cmath>
#include <ctime>
#include <sstream>
#include <iomanip>

#include <cstring>

#include "data_context.h"
#include "i_data_manager.h"
#include "i_exception_manager.h"

#include "pl_video_frame_raw.h"
#include "i_p_metadata_manager.h"
#include "i_metadata_manager.h"
#include "metadata_writer.h"
#include "i_opensdk_cgi_dispatcher.h"
#include "i_eventstatus_cgi_dispatcher.h"
#include "i_p_stream_provider_manager_video_raw.h"
#include "i_p_video_frame_raw.h"
#include "i_p_stream_requester_manager_video.h"
#include "life_cycle_manager_openapp.h"

using namespace std;
using namespace chrono;

static std::string Base64Encode(const uint8_t* data, size_t len) {
  static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((len + 2) / 3 * 4);
  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = (uint32_t)data[i] << 16;
    if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
    if (i + 2 < len) n |= data[i + 2];
    out += tbl[(n >> 18) & 63];
    out += tbl[(n >> 12) & 63];
    out += (i + 1 < len) ? tbl[(n >> 6) & 63] : '=';
    out += (i + 2 < len) ? tbl[n & 63] : '=';
  }
  return out;
}

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

void HandDetector::AppendLog(const std::string& msg)
{
  char ts[32];
  time_t t = time(NULL);
  struct tm* tm_info = localtime(&t);
  strftime(ts, sizeof(ts), "%H:%M:%S", tm_info);
  std::string entry = std::string("[") + ts + "] " + msg;
  debug_log_.push_back(entry);
  if (debug_log_.size() > 200) debug_log_.erase(debug_log_.begin());
  DebugLog("%s", entry.c_str());
}

void HandDetector::WriteEventLog(const std::string& message)
{
  AppendLog("WriteEventLog: " + message);
  auto* log = new Log(
      Log::LogType::EVENT_LOG,
      Log::LogDetailType::EVENT_OPENAPP,
      GetChannel(),
      static_cast<uint32_t>(time(NULL)),
      Platform_Std_Refine::String(message.c_str()));

  SendNoReplyEvent("LogManager",
      static_cast<int32_t>(ILogManager::EEvent::eWrite),
      0,
      log);
}

bool HandDetector::Initialize()
{
  RegisterOpenAPIURI();

  // 이전 크래시 로그 복구 (LoadNeuralNetwork segfault 진단용)
  {
    FILE* prev = fopen("/tmp/npu_load.log", "r");
    if (prev) {
      AppendLog("=== PREVIOUS CRASH LOG ===");
      char line[256];
      while (fgets(line, sizeof(line), prev)) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        AppendLog(std::string("  ") + line);
      }
      AppendLog("=== END CRASH LOG ===");
      fclose(prev);
      remove("/tmp/npu_load.log");
    }
  }


  info_list_ = std::make_shared<HandDetectorInfoList>(GetStringComponentVersion());
  PrepareAttributes(info_list_.get(), GetObjectName());

  std::string manifest_path = "../../config/app_manifest.json";
  ParseManifest(manifest_path, manifest_);

  // app_id_ 조기 초기화 (eInformAppInfo 수신 전 fallback)
  app_id_ = manifest_.app_name;

  AppendLog("Initialize: channel=" + std::to_string(GetChannel()) +
            " app_id=" + app_id_ +
            " manifest.app_name=" + manifest_.app_name);

  // MetaFrame 스키마 등록 (채널 0에서만 1회)
  if (GetChannel() == 0) {
    SetMetaFrameCapabilitySchema();
    SetMetaFrameSchema();
    // SendMetadataSchema();       // TODO: OpenEventDispatcher 매핑 해결 후 활성화
    // SendEventStatusSchema();    // TODO: OpenEventDispatcher 매핑 해결 후 활성화
  } else {
    AppendLog("Initialize: SKIP schema registration (channel=" + std::to_string(GetChannel()) + " != 0)");
  }

  // MQTT 연결
  auto& attr = info_list_->app_attribute_info;
  if (!attr.mqtt_broker_host.empty()) {
    bool ok = mqtt_.Connect(attr.mqtt_broker_host, attr.mqtt_broker_port);
    AppendLog("MQTT: " + std::string(ok ? "connected to " : "FAILED to connect ") +
              attr.mqtt_broker_host + ":" + std::to_string(attr.mqtt_broker_port));
    if (ok) mqtt_.PublishStatus("initialized", "network_binary.nb");
  } else {
    AppendLog("MQTT: broker host not configured (skipped)");
  }

  AppendLog("Initialize called");
  DebugLog("HandDetector Initialize");
  WriteEventLog("HandDetector Initialize OK");
  return Component::Initialize();
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

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(
      String("/configuration"), GetInstanceName(), methods);

  SendNoReplyEvent("AppDispatcher",
                   static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand),
                   0, uriRequest);
}

bool HandDetector::LoadNeuralNetwork()
{
  const std::string model_name = "network_binary.nb";
  const std::string model_path = std::string("../res/ai_bin/") + model_name;

  // 크래시 진단 파일 로그
  FILE* lf = fopen("/tmp/npu_load.log", "w");
  auto logstep = [&](const char* step) {
    if (lf) { fprintf(lf, "%s\n", step); fflush(lf); }
    AppendLog(std::string("LoadNPU: ") + step);
  };

  logstep("GetOrCreateNetwork");
  NeuralNetwork* network = GetOrCreateNetwork(model_name);
  if (!network) { logstep("FAIL: null"); if (lf) fclose(lf); return false; }

  // hand_detect end-to-end: nbg_meta.json name 필드 기준
  logstep("CreateInputTensor(images_0)");
  const auto& input_tensor = network->CreateInputTensor("images_0");
  if (!input_tensor) { logstep("FAIL: null"); if (lf) fclose(lf); return false; }

  logstep("CreateOutputTensor(attach_output0/out0)");
  const auto& output_tensor = network->CreateOutputTensor("attach_output0/out0");
  if (!output_tensor) { logstep("FAIL: null"); if (lf) fclose(lf); return false; }

  logstep("LoadNetwork(mean=0, scale=1) — may crash here");
  auto mean  = std::vector<float>{0.0f, 0.0f, 0.0f};
  auto scale = std::vector<float>{1.0f, 1.0f, 1.0f};

  if (!network->LoadNetwork(model_path, mean, scale)) {
    logstep("FAIL: LoadNetwork returned false");
    if (lf) fclose(lf);
    return false;
  }
  logstep("LoadNetwork OK!");

  int out_cnt = static_cast<int>(network->GetOutputTensorCount());
  char buf[128];
  snprintf(buf, sizeof(buf), "SUCCESS! OutputTensorCount=%d", out_cnt);
  logstep(buf);

  for (int i = 0; i < out_cnt; i++) {
    const auto& ot = network->GetOutputTensor(i);
    if (ot) {
      snprintf(buf, sizeof(buf), "output[%d] shape=[%u,%u,%u]",
               i, ot->Length(0), ot->Length(1), ot->Length(2));
      logstep(buf);
    }
  }

  npu_load_info_.model_name_ = model_name;
  npu_load_info_.input_tensor_names_ = "images_0";
  npu_load_info_.output_tensor_names_ = "attach_output0/out0";

  if (lf) fclose(lf);
  remove("/tmp/npu_load.log");

  AppendLog("LoadNeuralNetwork: SUCCESS");
  WriteEventLog("HandDetector: NPU model loaded OK");
  return true;
}

void HandDetector::Start()
{
  base::Start();
  AppendLog("Start called");
  DebugLog("HandDetector Start");
  WriteEventLog("HandDetector Start OK");
  if (!LoadNeuralNetwork()) {
    AppendLog("Start: LoadNeuralNetwork FAILED");
  }
}

bool HandDetector::Finalize()
{
  AppendLog("Finalize called");
  DebugLog("HandDetector Finalize");
  WriteEventLog("HandDetector Finalize");
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

      if (path_info == "/configuration") {
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
      static int vcnt = 0;
      if (++vcnt % 30 == 0)
        AppendLog("VideoRaw frame#" + std::to_string(vcnt));
      ProcessRawVideo(event);
      break;
    }
    case static_cast<int32_t>(LifeCycleManagerOpenApp::EEventType::eInformAppInfo):
    {
      auto blob = event->GetBlobArgument();
      auto* base_object = blob.GetBaseObject();
      if (base_object) {
        auto str = *(static_cast<String*>(base_object));
        JsonUtility::JsonDocument doc;
        doc.Parse(str.c_str());
        if (doc.HasMember("AppId") && doc["AppId"].IsString()) {
          app_id_ = doc["AppId"].GetString();
          AppendLog("AppId received: " + app_id_);
        }
      }
      break;
    }
    case static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameSchema):
    {
      AppendLog("eMetaFrameSchema request received");
      SetMetaFrameSchema();
      break;
    }
    case static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameCapability):
    {
      AppendLog("eMetaFrameCapability request received");
      SetMetaFrameCapabilitySchema();
      break;
    }
    // TODO: OpenEventDispatcher 매핑 해결 후 활성화
    // case static_cast<int32_t>(I_EventstatusCGIDispatcher::EEventType::eMetadataSchema):
    // case static_cast<int32_t>(I_EventstatusCGIDispatcher::EEventType::eEventstatusSchema):
    default:
    {
      static int unk_cnt = 0;
      if (++unk_cnt <= 10)
        AppendLog("ProcessAEvent: unknown event type=" + std::to_string(event->GetType()));

      // event 2006001이 eReadyForUse일 수 있음 → Capability 재전송
      static bool cap_resent = false;
      if (!cap_resent && event->GetType() == 2006001) {
        cap_resent = true;
        SetMetaFrameCapabilitySchema();
        SetMetaFrameSchema();
        AppendLog("MetaFrame schemas re-sent on event 2006001");
      }

      base::ProcessAEvent(event);
      break;
    }
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

  if (mode == "test") {
    WriteEventLog("Test Log!");
    AppendLog("mode=test: WriteEventLog called");
    param->SetStatusCode(200);
    param->SetResponseBody("{\"result\":\"ok\"}");
    return true;
  }
  else if (mode == "log") {
    std::string resp = "[";
    for (size_t i = 0; i < debug_log_.size(); ++i) {
      // JSON string escape
      std::string s = debug_log_[i];
      std::string escaped;
      for (char c : s) {
        if (c == '"') escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else escaped += c;
      }
      resp += "\"" + escaped + "\"";
      if (i + 1 < debug_log_.size()) resp += ",";
    }
    resp += "]";
    param->SetStatusCode(200);
    param->SetResponseBody(resp);
    return true;
  }
  else if (mode == "clear") {
    debug_log_.clear();
    AppendLog("Log cleared");
    param->SetStatusCode(200);
    param->SetResponseBody("{\"result\":\"ok\"}");
    return true;
  }
  else if (mode == "start") {
    run_flag_ = true;
    AppendLog("mode=start: inference started");
    DebugLog("HandDetector: inference started");
    WriteEventLog("HandDetector: inference started");
    mqtt_.PublishStatus("running", "network_binary.nb");
    param->SetStatusCode(200);
    param->SetResponseBody("{\"result\":\"ok\",\"state\":\"running\"}");
    return true;
  }
  else if (mode == "stop") {
    run_flag_ = false;
    AppendLog("mode=stop: inference stopped");
    DebugLog("HandDetector: inference stopped");
    WriteEventLog("HandDetector: inference stopped");
    mqtt_.PublishStatus("stopped", "network_binary.nb");
    param->SetStatusCode(200);
    param->SetResponseBody("{\"result\":\"ok\",\"state\":\"stopped\"}");
    return true;
  }
  else if (mode == "capture") {
    std::lock_guard<std::mutex> lock(detections_mutex_);
    if (last_jpeg_.empty()) {
      std::string diag = run_flag_
          ? "frame empty - check SnapDiag log"
          : "inference not running (press Start first)";
      param->SetStatusCode(200);
      param->SetResponseBody("{\"error\":\"" + diag + "\"}");
      return true;
    }
    std::string b64 = Base64Encode(last_jpeg_.data(), last_jpeg_.size());
    std::string dets = "[";
    for (size_t i = 0; i < last_detections_.size(); i++) {
      const auto& d = last_detections_[i];
      if (i > 0) dets += ",";
      dets += "{\"x1\":" + std::to_string((int)d.x1) +
              ",\"y1\":" + std::to_string((int)d.y1) +
              ",\"x2\":" + std::to_string((int)d.x2) +
              ",\"y2\":" + std::to_string((int)d.y2) +
              ",\"conf\":" + std::to_string(d.confidence) + "}";
    }
    dets += "]";
    param->SetStatusCode(200);
    param->SetResponseBody("{\"gray\":\"" + b64 + "\""
        ",\"w\":320,\"h\":180"
        ",\"frame_w\":" + std::to_string(frame_width_) +
        ",\"frame_h\":" + std::to_string(frame_height_) +
        ",\"detections\":" + dets + "}");
    return true;
  }
  else if (mode == "snapshot") {
    std::lock_guard<std::mutex> lock(detections_mutex_);
    if (last_jpeg_.empty()) {
      param->SetStatusCode(503);
      param->SetResponseBody("{\"error\":\"no frame\"}");
      return false;
    }
    // raw grayscale 480x270 → base64
    std::string b64 = Base64Encode(last_jpeg_.data(), last_jpeg_.size());
    param->SetStatusCode(200);
    param->SetResponseBody("{\"gray\":\"" + b64 + "\",\"w\":480,\"h\":270}");
    return true;
  }
  else if (mode == "detections" || mode == "view") {
    std::lock_guard<std::mutex> lock(detections_mutex_);
    std::string resp = "{\"detections\":[";
    for (size_t i = 0; i < last_detections_.size(); i++) {
      const auto& d = last_detections_[i];
      if (i > 0) resp += ",";
      resp += "{\"x1\":" + std::to_string((int)d.x1) +
              ",\"y1\":" + std::to_string((int)d.y1) +
              ",\"x2\":" + std::to_string((int)d.x2) +
              ",\"y2\":" + std::to_string((int)d.y2) +
              ",\"conf\":" + std::to_string(d.confidence) + "}";
    }
    resp += "],\"frame_w\":" + std::to_string(frame_width_) +
            ",\"frame_h\":" + std::to_string(frame_height_) + "}";
    param->SetStatusCode(200);
    param->SetResponseBody(resp);
    return true;
  }
  else if (mode == "info") {
    AppendLog("mode=info requested");
    auto& attr = info_list_->app_attribute_info;
    std::string resp =
        std::string("{") +
        "\"state\":\"" + (run_flag_ ? "running" : "stopped") + "\"," +
        "\"model\":\"network_binary.nb\"," +
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
    AppendLog("mode=config updated: conf=" + std::to_string(attr.confidence_threshold) +
              " nms=" + std::to_string(attr.nms_iou_threshold));
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

void HandDetector::ProcessRawVideo(Event* event)
{
  if (event == nullptr || event->IsReply()) return;

  auto blob = event->GetBlobArgument();
  event->ClearBaseObjectArgument();

  static int blob_log_cnt = 0;
  if (++blob_log_cnt == 1)
    AppendLog("ProcessRawVideo: first blob size=" + std::to_string(blob.GetSize()));

  std::pair<std::variant<BaseObject*, char*>, uint64_t> ret(
      (char*)blob.GetRawData(), blob.GetSize());

  IPVideoFrameRaw* raw_frame = new ("GetImage") IPLVideoFrameRaw();
  raw_frame->DeserializeBaseObject(raw_frame, ret);

  std::shared_ptr<RawImage> img(raw_frame->GetRawImage());
  if (img == nullptr) {
    static int null_img_cnt = 0;
    if (++null_img_cnt == 1)
      AppendLog("ProcessRawVideo: WARN GetRawImage returned null");
    blob.ClearResource();
    return;
  }

  Inference(img);
  blob.ClearResource();
}

void HandDetector::Inference(std::shared_ptr<RawImage> img)
{
  if (!run_flag_) return;
  if (!img) return;

  // 모델은 Start()에서 이미 로딩됨
  if (npu_load_info_.model_name_.empty()) return;  // 로딩 실패 시 skip

  int skip = info_list_->app_attribute_info.skip_frames;
  if (skip > 0 && (frame_count_ % (skip + 1)) != 0) {
    frame_count_++;
    return;
  }
  frame_count_++;

  const std::shared_ptr<Tensor> rgb(Tensor::Create());

  constexpr uint32_t kMaxSize = 4096;
  int images_cnt = 0;
  for (auto* image = img.get(); image; image = image->next) {
    if (image->width != 0 && image->height != 0) images_cnt++;
  }

  static int inf_cnt = 0;
  ++inf_cnt;

  if (inf_cnt == 1)
    AppendLog("Inference: planes=" + std::to_string(images_cnt) +
              " pts=" + std::to_string(img->pts));

  if (images_cnt > 1) {
    bool selected = false;
    for (auto* image = img.get(); image; image = image->next) {
      if (image->width < 3840 && image->height < 2160) {
        if (image->width <= kMaxSize && image->height <= kMaxSize) {
          if (inf_cnt == 1)
            AppendLog("Inference: multi-plane selected w=" + std::to_string(image->width) +
                      " h=" + std::to_string(image->height));
          if (!rgb->Allocate(*image)) {
            AppendLog("Inference #" + std::to_string(inf_cnt) +
                      ": FAIL Allocate (multi-plane) w=" + std::to_string(image->width) +
                      " h=" + std::to_string(image->height));
            return;
          }
          frame_width_  = image->width;
          frame_height_ = image->height;
          selected = true;
          break;
        }
      }
    }
    if (!selected) {
      AppendLog("Inference #" + std::to_string(inf_cnt) +
                ": FAIL no suitable plane found (planes=" + std::to_string(images_cnt) + ")");
      return;
    }
  } else {
    if (inf_cnt == 1)
      AppendLog("Inference: single-plane w=" + std::to_string(img->width) +
                " h=" + std::to_string(img->height));
    if (!rgb->Allocate(*img)) {
      AppendLog("Inference #" + std::to_string(inf_cnt) +
                ": FAIL Allocate (single) w=" + std::to_string(img->width) +
                " h=" + std::to_string(img->height));
      return;
    }
    frame_width_  = img->width;
    frame_height_ = img->height;
  }

  raw_pts_ = img->pts;

  if (inf_cnt == 1)
    AppendLog("Inference: first frame allocated w=" + std::to_string(frame_width_) +
              " h=" + std::to_string(frame_height_));

  auto t0 = chrono::steady_clock::now();
  if (!PreProcess(rgb)) {
    static int pp_fail = 0;
    if (++pp_fail <= 3)
      AppendLog("Inference #" + std::to_string(inf_cnt) + ": FAIL PreProcess");
    return;
  }

  auto t1 = chrono::steady_clock::now();
  if (!Execute()) {
    static int ex_fail = 0;
    if (++ex_fail <= 3)
      AppendLog("Inference #" + std::to_string(inf_cnt) + ": FAIL Execute");
    return;
  }

  auto t2 = chrono::steady_clock::now();
  if (!PostProcess()) {
    static int pp2_fail = 0;
    if (++pp2_fail <= 3)
      AppendLog("Inference #" + std::to_string(inf_cnt) + ": FAIL PostProcess");
    return;
  }

  auto t3 = chrono::steady_clock::now();

  float pre_ms  = chrono::duration<float, milli>(t1 - t0).count();
  float inf_ms  = chrono::duration<float, milli>(t2 - t1).count();
  float post_ms = chrono::duration<float, milli>(t3 - t2).count();

  // MQTT: heartbeat 5초 1회 + detection 즉시
  static auto last_hb = chrono::steady_clock::now();
  auto now_hb = chrono::steady_clock::now();
  bool is_heartbeat = chrono::duration<float>(now_hb - last_hb).count() >= 5.0f;

  if (is_heartbeat) {
    last_hb = now_hb;
    AppendLog("Timing #" + std::to_string(inf_cnt) +
              ": pre=" + std::to_string((int)pre_ms) +
              "ms inf=" + std::to_string((int)inf_ms) +
              "ms post=" + std::to_string((int)post_ms) +
              "ms det=" + std::to_string(last_detection_count_));
    mqtt_.PublishDebug(pre_ms, inf_ms, post_ms, last_detection_count_);
  }

  if (last_detection_count_ > 0) {
    mqtt_.PublishDetection(inf_cnt, last_detection_count_,
        last_detection_count_ > 0 ? 1.0f : 0.0f);
  }
}

bool HandDetector::PreProcess(std::shared_ptr<Tensor> rgb)
{
  auto* network = GetNetwork(npu_load_info_.model_name_);
  if (!network) {
    static int net_fail = 0;
    if (++net_fail <= 3)
      AppendLog("PreProcess: FAIL GetNetwork(\"" + npu_load_info_.model_name_ + "\") returned null");
    return false;
  }

  const std::shared_ptr<Tensor>& input_tensor(network->GetInputTensor(0));
  if (!input_tensor) {
    AppendLog("PreProcess: FAIL GetInputTensor(0) returned null");
    return false;
  }

  img_size_t size = {
    .width  = 640,
    .height = 640
  };

  static bool resize_logged = false;
  if (!resize_logged) {
    AppendLog("PreProcess: input_tensor dim=[" +
              std::to_string(size.width) + "x" + std::to_string(size.height) + "]");
    resize_logged = true;
  }

  if (!rgb->Resize(*input_tensor, size)) {
    static int resize_fail = 0;
    if (++resize_fail <= 3)
      AppendLog("PreProcess: FAIL Resize to [" +
                std::to_string(size.width) + "x" + std::to_string(size.height) + "]" +
                " src=[" + std::to_string(frame_width_) + "x" + std::to_string(frame_height_) + "]");
    return false;
  }

  // Resize 후 input_tensor에서 프레임 캡처 (rgb->VirtAddr()=0이므로 input_tensor 사용)
  {
    auto* ptr = (const uint8_t*)input_tensor->VirtAddr();
    int tw = (int)size.width, th = (int)size.height;  // 640x640

    static bool snap_diag2 = false;
    if (!snap_diag2) {
      snap_diag2 = true;
      AppendLog("SnapDiag2: input_tensor VirtAddr=" + std::to_string((uintptr_t)ptr) +
                " tw=" + std::to_string(tw) + " th=" + std::to_string(th));
      if (ptr) {
        std::string sample;
        for (int k = 0; k < 10; k++)
          sample += std::to_string((int)ptr[k]) + " ";
        AppendLog("SnapDiag2: data[0..9]=" + sample);
      }
    }

    if (ptr && tw > 0 && th > 0) {
      // input_tensor: 640x640x3 PLANAR (각 채널 640*640 바이트 연속)
      const int plane = tw * th;  // 409600
      const int cw = 320, ch = 180;

      // Direct resize: 전체 640x640이 유효 영역 (패딩 없음)
      std::vector<uint8_t> buf(cw * ch * 3);
      for (int j = 0; j < ch; j++) {
        int sy = j * th / ch;
        if (sy >= th) sy = th - 1;
        for (int i = 0; i < cw; i++) {
          int sx = i * tw / cw;
          if (sx >= tw) sx = tw - 1;
          int pidx = sy * tw + sx;  // 플레인 내 인덱스
          int dst = (j * cw + i) * 3;
          buf[dst]     = ptr[0 * plane + pidx];  // R
          buf[dst + 1] = ptr[1 * plane + pidx];  // G
          buf[dst + 2] = ptr[2 * plane + pidx];  // B
        }
      }
      std::lock_guard<std::mutex> lock(detections_mutex_);
      last_jpeg_ = std::move(buf);
    }
  }

  if (frame_width_ > 0 && frame_height_ > 0) {
    // Direct resize: 640x640으로 직접 스트레칭 (패딩 없음)
    scale_x_ = static_cast<float>(frame_width_)  / static_cast<float>(size.width);
    scale_y_ = static_cast<float>(frame_height_) / static_cast<float>(size.height);

    static bool scale_logged = false;
    if (!scale_logged) {
      AppendLog("PreProcess: direct resize scale_x=" + std::to_string(scale_x_) +
                " scale_y=" + std::to_string(scale_y_) +
                " (frame " + std::to_string(frame_width_) + "x" + std::to_string(frame_height_) +
                " -> model " + std::to_string(size.width) + "x" + std::to_string(size.height) + ")");
      scale_logged = true;
    }
  }

  return true;
}

bool HandDetector::Execute()
{
  auto* network = GetNetwork(npu_load_info_.model_name_);
  if (!network) {
    AppendLog("Execute: FAIL GetNetwork(\"" + npu_load_info_.model_name_ + "\") returned null");
    return false;
  }

  stat_t stat = {0,};
  bool ok = network->RunNetwork(stat);
  if (!ok)
    AppendLog("Execute: FAIL RunNetwork returned false");
  return ok;
}

bool HandDetector::PostProcess()
{
  auto* network = GetNetwork(npu_load_info_.model_name_);
  if (!network) {
    AppendLog("PostProcess: FAIL GetNetwork(\"" + npu_load_info_.model_name_ + "\") returned null");
    return false;
  }

  // YOLOv7 raw logit: 단일 출력 텐서 (1, 25200, 6)
  // 각 anchor: [tx, ty, tw, th, obj_logit, cls_logit] (stride 6)
  // sigmoid + grid decode 필요
  const std::shared_ptr<Tensor>& output_tensor(network->GetOutputTensor(0));
  if (!output_tensor) {
    AppendLog("PostProcess: FAIL GetOutputTensor(0) null");
    return false;
  }
  const float* raw = static_cast<const float*>(output_tensor->VirtAddr());
  if (!raw) {
    AppendLog("PostProcess: FAIL VirtAddr() null");
    return false;
  }

  const int N = 25200;       // YOLOv7 640x640: 80*80*3 + 40*40*3 + 20*20*3
  const int S = 6;           // tx, ty, tw, th, obj_logit, cls_logit
  auto& attr = info_list_->app_attribute_info;

  // --- 앵커 그리드 사전 생성 (한 번만) ---
  struct AnchorInfo { float grid_x, grid_y, stride, anchor_w, anchor_h; };
  static std::vector<AnchorInfo> anchor_grid;
  if (anchor_grid.empty()) {
    // YOLOv7 COCO default anchors
    const float anchors[][6] = {
      {12,16,  19,36,  40,28},   // P3 stride=8
      {36,75,  76,55,  72,146},  // P4 stride=16
      {142,110, 192,243, 459,401} // P5 stride=32
    };
    const int strides[] = {8, 16, 32};
    const int grids[]   = {80, 40, 20};

    anchor_grid.reserve(N);
    for (int lv = 0; lv < 3; lv++) {
      int gs = grids[lv];
      for (int gy = 0; gy < gs; gy++) {
        for (int gx = 0; gx < gs; gx++) {
          for (int a = 0; a < 3; a++) {
            anchor_grid.push_back({
              static_cast<float>(gx),
              static_cast<float>(gy),
              static_cast<float>(strides[lv]),
              anchors[lv][a * 2],
              anchors[lv][a * 2 + 1]
            });
          }
        }
      }
    }
    AppendLog("AnchorGrid: built " + std::to_string(anchor_grid.size()) + " anchors");
  }

  auto sigmoid = [](float x) -> float { return 1.0f / (1.0f + expf(-x)); };

  // 진단 로그
  static int pp_logged = 0;
  if (++pp_logged <= 3) {
    AppendLog("PostProcess: output shape=[" +
              std::to_string(output_tensor->Length(0)) + "," +
              std::to_string(output_tensor->Length(1)) + "," +
              std::to_string(output_tensor->Length(2)) + "]");
    std::string sample;
    for (int i = 0; i < S; i++)
      sample += std::to_string(raw[i]) + " ";
    AppendLog("PostProcess: raw logit (anchor0): " + sample);
    // sigmoid 적용 후 값
    float obj0 = sigmoid(raw[4]);
    float cls0 = sigmoid(raw[5]);
    AppendLog("PostProcess: anchor0 sigmoid obj=" + std::to_string(obj0) +
              " cls=" + std::to_string(cls0) + " conf=" + std::to_string(obj0 * cls0));
  }

  // 진단: 전체 anchor 중 max confidence (sigmoid 적용 후)
  static int diag_cnt = 0;
  if (++diag_cnt <= 10 || diag_cnt % 30 == 1) {
    float max_obj = 0, max_cls = 0, max_conf = 0;
    int max_idx = -1;
    for (int i = 0; i < N; i++) {
      float o = sigmoid(raw[i * S + 4]);
      float c = sigmoid(raw[i * S + 5]);
      float f = o * c;
      if (f > max_conf) { max_conf = f; max_obj = o; max_cls = c; max_idx = i; }
    }
    AppendLog("Diag: max_conf=" + std::to_string(max_conf) +
              " (obj=" + std::to_string(max_obj) + " cls=" + std::to_string(max_cls) +
              ") anchor=" + std::to_string(max_idx));
    if (max_idx >= 0) {
      const auto& ag = anchor_grid[max_idx];
      float cx = (sigmoid(raw[max_idx * S + 0]) * 2.0f - 0.5f + ag.grid_x) * ag.stride;
      float cy = (sigmoid(raw[max_idx * S + 1]) * 2.0f - 0.5f + ag.grid_y) * ag.stride;
      float w  = powf(sigmoid(raw[max_idx * S + 2]) * 2.0f, 2) * ag.anchor_w;
      float h  = powf(sigmoid(raw[max_idx * S + 3]) * 2.0f, 2) * ag.anchor_h;
      AppendLog("Diag: best bbox cx=" + std::to_string(cx) + " cy=" + std::to_string(cy) +
                " w=" + std::to_string(w) + " h=" + std::to_string(h) +
                " grid=(" + std::to_string((int)ag.grid_x) + "," + std::to_string((int)ag.grid_y) +
                ") stride=" + std::to_string((int)ag.stride));
    }
  }

  // Detection loop: sigmoid + grid decode
  std::vector<Detection> detections;
  for (int i = 0; i < N; i++) {
    float obj = sigmoid(raw[i * S + 4]);
    float cls = sigmoid(raw[i * S + 5]);
    float conf = obj * cls;
    if (conf < attr.confidence_threshold) continue;

    const auto& ag = anchor_grid[i];
    float cx = (sigmoid(raw[i * S + 0]) * 2.0f - 0.5f + ag.grid_x) * ag.stride;
    float cy = (sigmoid(raw[i * S + 1]) * 2.0f - 0.5f + ag.grid_y) * ag.stride;
    float w  = powf(sigmoid(raw[i * S + 2]) * 2.0f, 2) * ag.anchor_w;
    float h  = powf(sigmoid(raw[i * S + 3]) * 2.0f, 2) * ag.anchor_h;

    Detection d;
    d.x1 = (cx - w / 2.0f) * scale_x_;
    d.y1 = (cy - h / 2.0f) * scale_y_;
    d.x2 = (cx + w / 2.0f) * scale_x_;
    d.y2 = (cy + h / 2.0f) * scale_y_;
    d.confidence = conf;
    d.class_id = 0;

    d.x1 = std::max(0.0f, std::min(d.x1, static_cast<float>(frame_width_)));
    d.y1 = std::max(0.0f, std::min(d.y1, static_cast<float>(frame_height_)));
    d.x2 = std::max(0.0f, std::min(d.x2, static_cast<float>(frame_width_)));
    d.y2 = std::max(0.0f, std::min(d.y2, static_cast<float>(frame_height_)));

    if (d.x2 > d.x1 && d.y2 > d.y1)
      detections.push_back(d);
  }

  // NMS
  std::sort(detections.begin(), detections.end(),
            [](const Detection& a, const Detection& b) { return a.confidence > b.confidence; });
  std::vector<Detection> nms_result;
  std::vector<bool> suppressed(detections.size(), false);
  for (size_t i = 0; i < detections.size(); i++) {
    if (suppressed[i]) continue;
    nms_result.push_back(detections[i]);
    if (nms_result.size() >= 100) break;
    for (size_t j = i + 1; j < detections.size(); j++) {
      if (suppressed[j]) continue;
      float ix1 = std::max(detections[i].x1, detections[j].x1);
      float iy1 = std::max(detections[i].y1, detections[j].y1);
      float ix2 = std::min(detections[i].x2, detections[j].x2);
      float iy2 = std::min(detections[i].y2, detections[j].y2);
      float inter = std::max(0.0f, ix2-ix1) * std::max(0.0f, iy2-iy1);
      float a1 = (detections[i].x2-detections[i].x1) * (detections[i].y2-detections[i].y1);
      float a2 = (detections[j].x2-detections[j].x1) * (detections[j].y2-detections[j].y1);
      if (inter / (a1 + a2 - inter) > attr.nms_iou_threshold)
        suppressed[j] = true;
    }
  }
  detections = std::move(nms_result);

  // 주기적 디버그 로그
  static int pp_cnt = 0;
  if (++pp_cnt % 30 == 1) {
    AppendLog("PostProcess #" + std::to_string(pp_cnt) +
              ": det=" + std::to_string(detections.size()) +
              " threshold=" + std::to_string(attr.confidence_threshold));
  }

  last_detection_count_ = static_cast<int>(detections.size());
  {
    std::lock_guard<std::mutex> lock(detections_mutex_);
    last_detections_ = detections;
  }

  SendFrameMetadata(detections);
  float max_conf = detections.empty() ? 0.0f : detections[0].confidence;
  // NotifyEventMetadata(!detections.empty(), static_cast<int>(detections.size()), max_conf);  // TODO: OpenEventDispatcher 해결 후
  SendOsd(static_cast<int>(detections.size()), max_conf);

  if (!detections.empty()) {
    std::string det_log = "Det(" + std::to_string(detections.size()) + "):";
    for (size_t i = 0; i < std::min(detections.size(), size_t(5)); i++) {
      const auto& d = detections[i];
      det_log += " [" + std::to_string((int)d.x1) + "," + std::to_string((int)d.y1) +
                 "-" + std::to_string((int)d.x2) + "," + std::to_string((int)d.y2) +
                 " c=" + std::to_string((int)(d.confidence * 100)) + "%]";
    }
    AppendLog(det_log);
  } else {
    static int zero_det_cnt = 0;
    if (++zero_det_cnt % 30 == 1)
      AppendLog("PostProcess #" + std::to_string(pp_cnt) + ": det=0 (no hand detected)");
  }

  return true;
}

void HandDetector::SetMetaFrameSchema()
{
  std::string meta_schema =
      "{\"AppID\": \"" + manifest_.app_name + "\","
      "\"Schema\": \"\","
      "\"Encoding\": \"base64\"}";

  SendNoReplyEvent("Stub::Dispatcher::OpenSDK",
      static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameSchema), 0,
      new ("Schema") Platform_Std_Refine::SerializableString(meta_schema.c_str()));
  AppendLog("SetMetaFrameSchema: registered (AppID=" + manifest_.app_name + ")");
}

void HandDetector::SetMetaFrameCapabilitySchema()
{
  // 테스트: ONVIF 표준 타입만 사용하여 카메라가 등록하는지 확인
  std::string str_out =
      "{\"AppID\": \"" + manifest_.app_name + "\","
      "\"Capabilities\": ["
      "{\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Class/tt:Type\","
      "\"type\": \"xs:string\","
      "\"enum\": [\"Human\",\"Unknown\"]},"
      "{\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Class/tt:Type/@Likelihood\","
      "\"type\": \"xs:float\","
      "\"minimum\": 0.0,"
      "\"maximum\": 1.0},"
      "{\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/@ObjectId\","
      "\"type\": \"xs:integer\","
      "\"minimum\": 0,"
      "\"maximum\": 999}"
      "]}";

  SendNoReplyEvent("Stub::Dispatcher::OpenSDK",
      static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameCapability), 0,
      new ("Schema") Platform_Std_Refine::SerializableString(str_out.c_str()));
  AppendLog("SetMetaFrameCapabilitySchema: registered");
  AppendLog("CapabilityJSON: " + str_out.substr(0, 300));
}

void HandDetector::SendFrameMetadata(const std::vector<Detection>& detections)
{
  static int meta_cnt = 0;
  meta_cnt++;

  if (app_id_.empty()) {
    if (meta_cnt <= 3)
      AppendLog("SendFrameMetadata: SKIP app_id_ empty (cnt=" + std::to_string(meta_cnt) + ")");
    return;
  }

  // system clock 사용 (send_metadata 샘플과 동일 — raw_pts_는 카메라 내부 PTS로 Unix ms가 아님)
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  FrameMetadataItem frame_metadata;
  frame_metadata.set_resolution(frame_width_, frame_height_);
  frame_metadata.set_timestamp(timestamp);

  int object_id = 1;
  for (const auto& det : detections) {
    metadata::common::Object obj;
    obj.object_id  = object_id++;
    obj.parent_id  = 0;
    obj.timestamp  = timestamp;
    obj.likelihood = det.confidence;
    obj.category   = static_cast<metadata::common::ObjectCategory>(0);  // kUnknown
    obj.attr_num   = static_cast<int>(detections.size());
    obj.rect.sx    = static_cast<int>(det.x1);
    obj.rect.ex    = static_cast<int>(det.x2);
    obj.rect.sy    = static_cast<int>(det.y1);
    obj.rect.ey    = static_cast<int>(det.y2);
    frame_metadata.add_object(std::move(obj));
  }

  auto metadata = StreamMetadata(GetChannel(), timestamp, Metadata::MetadataType::kAI);
  metadata.set_frame_metadata(std::move(frame_metadata));

  // 첫 5회: 전송 내용 상세 로그
  if (meta_cnt <= 5) {
    std::string log = "SendFrameMetadata #" + std::to_string(meta_cnt) +
        ": app_id=" + app_id_ +
        " ch=" + std::to_string(GetChannel()) +
        " ts=" + std::to_string(timestamp) +
        " res=" + std::to_string(frame_width_) + "x" + std::to_string(frame_height_) +
        " objs=" + std::to_string(detections.size());
    if (!detections.empty()) {
      const auto& d = detections[0];
      log += " obj0=[" + std::to_string((int)d.x1) + "," + std::to_string((int)d.y1) +
             "-" + std::to_string((int)d.x2) + "," + std::to_string((int)d.y2) +
             " c=" + std::to_string((int)(d.confidence * 100)) + "%]";
    }
    AppendLog(log);
  }

  // 1) 웹뷰어용: SDK MetadataWriter → 정확한 XML → eRequestRawMetadata
  auto writer = MetadataWriter::Create("stream_metadata_ai_xml");
  if (writer) {
    metadata.Accept(*writer);
    auto xml_str = std::get<1>(writer->Get());
    auto str_meta = StringMetadata(GetChannel(), timestamp);
    str_meta.Set(xml_str);
    auto* req_raw = new ("MetadataRequest") IPMetadataManager::StringMetadataRequest();
    req_raw->SetStringMetadata(std::move(str_meta));
    SendNoReplyEvent("MetadataManager",
        static_cast<int32_t>(IMetadataManager::EEventType::eRequestRawMetadata), 0, req_raw);
    if (meta_cnt <= 3)
      AppendLog("MetadataWriter XML sent (len=" + std::to_string(xml_str.size()) + "): " + xml_str.substr(0, 300));
  } else {
    if (meta_cnt <= 3)
      AppendLog("MetadataWriter::Create FAILED");
  }

  // 2) VMS용: 구조화 API → eRequestMetadata
  auto builder = IPMetadataManager::StreamMetadataRequest::builder();
  builder.set_app_id(app_id_);
  builder.set_stream_metadata(std::forward<StreamMetadata>(metadata));
  auto* req = reinterpret_cast<IPMetadataManager::StreamMetadataRequest*>(builder.build());
  SendNoReplyEvent("MetadataManager",
      static_cast<int32_t>(IMetadataManager::EEventType::eRequestMetadata), 0, req);

  // 매 100회마다 카운터 로그
  if (meta_cnt % 100 == 0)
    AppendLog("SendFrameMetadata: sent " + std::to_string(meta_cnt) + " frames total");
}

void HandDetector::SendOsd(int det_count, float max_conf)
{
  IPStreamRequesterManagerVideo::MultilineOsdItem item;
  item.Enable       = true;
  item.Index        = 101;
  item.OSDType      = "Title";
  item.OSDColor     = "White";
  item.Transparency = "Off";
  item.FontSize     = "Small";
  item.PositionX    = 0;
  item.PositionY    = 0;

  if (det_count > 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Hand: %d | Conf: %d%%", det_count, (int)(max_conf * 100));
    item.OSD = buf;
  } else {
    item.OSD = "Hand: 0";
  }

  auto* osd = new ("MultilineOSD")
      IPStreamRequesterManagerVideo::MultilineExternalOsdDeliveryObject();
  osd->vMultilineOsd.push_back(std::move(item));
  SendNoReplyEvent("SRMgrVideo",
      static_cast<int32_t>(IPStreamRequesterManagerVideo::EEventType::eExternalMultiLineOsd),
      0, osd);
}

void HandDetector::SendMetadataSchema()
{
  std::string schema =
      "{\"EventName\": \"OpenSDK.hand_detector.HandDetection\","
      "\"EventTopic\": \"tns1:OpenApp/hand_detector/HandDetection\","
      "\"EventSchema\": \"\","
      "\"SchemaType\": \"PROPRIETARY\"}";

  SendNoReplyEvent("OpenEventDispatcher",
      static_cast<int32_t>(I_EventstatusCGIDispatcher::EEventType::eMetadataSchema), 0,
      new ("Schema") Platform_Std_Refine::SerializableString(schema.c_str()));
  AppendLog("SendMetadataSchema: registered (topic=tns1:OpenApp/hand_detector/HandDetection)");
}

void HandDetector::SendEventStatusSchema()
{
  std::string schema =
      "{\"AppID\": \"" + app_id_ + "\","
      "\"Schema\": ["
      "{\"Type\": \"SCHEME\", \"Format\": \"TEXT\","
      "\"Content\": \"EventName=OpenSDK.hand_detector.HandDetection;"
      "Channel=0;State=bool;DetectionCount=int;MaxConfidence=float\"}"
      "]}";

  SendNoReplyEvent("OpenEventDispatcher",
      static_cast<int32_t>(I_EventstatusCGIDispatcher::EEventType::eEventstatusSchema), 0,
      new ("Schema") Platform_Std_Refine::SerializableString(schema.c_str()));
  AppendLog("SendEventStatusSchema: registered");
}

void HandDetector::NotifyEventMetadata(bool detected, int det_count, float max_conf)
{
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

  auto event_metadata = EventMetadataItem();
  event_metadata.set_topic("tns1:OpenApp/hand_detector/HandDetection");
  event_metadata.set_timestamp(timestamp);
  event_metadata.add_source({"VideoSourceToken", "VideoSourceToken-0"});
  event_metadata.add_data({"State", detected ? "true" : "false"});

  auto metadata = StreamMetadata(GetChannel(), timestamp);
  metadata.add_event_metadata(std::move(event_metadata));
  metadata.NeedBroadcasting(true);

  auto builder = IPMetadataManager::StreamMetadataRequest::builder();
  builder.set_app_id(app_id_);
  builder.set_stream_metadata(std::forward<StreamMetadata>(metadata));
  auto* req = reinterpret_cast<IPMetadataManager::StreamMetadataRequest*>(builder.build());
  SendNoReplyEvent("MetadataManager",
      static_cast<int32_t>(IMetadataManager::EEventType::eRequestMetadata), 0, req);

  static int evt_cnt = 0;
  if (++evt_cnt <= 3)
    AppendLog("NotifyEventMetadata #" + std::to_string(evt_cnt) +
              ": detected=" + std::string(detected ? "true" : "false") +
              " count=" + std::to_string(det_count) +
              " broadcast=true");
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
