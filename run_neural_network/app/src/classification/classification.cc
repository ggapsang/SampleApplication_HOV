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

#define MAX_SIZE 4096

using namespace std;
using namespace chrono;

Classification::Classification()
  : Classification(kComponentId, "Classification")
{
}

Classification::Classification(ClassID id, const char *name)
: base(id, name), relative_model_path("")
{
}

Classification::~Classification()
{
}

bool Classification::Initialize() {
  run_neural_network_info_list = std::make_shared<RunNeuralNetworkInfoList>(GetStringComponentVersion());
  PrepareAttributes(run_neural_network_info_list.get(), GetObjectName());
  std::string manifest_path = "../../config/app_manifest.json";
  ParseManifest(manifest_path, manifest_);

  if (GetChannel() == 0) {
    RegisterOpenAPIURI();
    SetMetaFrameCapabilitySchema();
    SetMetaFrameSchema();
  }

  return true;
}

bool Classification::ParseManifest(const std::string& manifest_path, ManifestInfo& info) {
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

void Classification::HandleRequest(Event* event) {
  if (event == nullptr || event->IsReply()) {
    return;
  }
}

bool Classification::Finalize()
{
  UnloadNetwork(relative_model_path);
  return Component::Finalize();
}

void Classification::SetMetaFrameSchema() {
  string app_id = manifest_.app_name;
  string schema = "<xs:complexType name=\"MetadataStream\"><xs:sequence><xs:element><xs:complexType name=\"VideoAnalytics\"><xs:sequence><xs:element><xs:complexType name=\"Frame\"><xs:sequence><xs:element><xs:complexType name=\"Object\"><xs:sequence><xs:element><xs:complexType name=\"Appearance\"><xs:sequence><xs:element><xs:complexType name=\"Class\"><xs:sequence><xs:element><xs:complexType name=\"Type\"><xs:simpleContent><xs:extension base=\"xs:string\"><xs:attribute name=\"Likelihood\" type=\"xs:float\"/></xs:extension></xs:simpleContent></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element></xs:sequence><xs:attribute name=\"ObjectId\" type=\"xs:integer\"/><xs:attribute name=\"Parent\" type=\"xs:integer\"/></xs:complexType></xs:element></xs:sequence><xs:attribute name=\"UtcTime\" type=\"xs:dateTime\" use=\"required\"/></xs:complexType></xs:element></xs:sequence></xs:complexType></xs:element></xs:sequence></xs:complexType>";
  string encoding = "base64"; // base64, UTF-8

  auto e = Base64::encode(schema.c_str(), schema.size());
  std::string e_s(e.first.get(), e.second);

  auto d = Base64::decode(e_s.c_str(), e_s.size());
  std::string d_s(d.first.get(), d.second);
  printf("[CHECK] Decode Base64\n%s\n", d_s.c_str());

  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto alloc = document.GetAllocator();

  document.AddMember("AppID", app_id, alloc);
  document.AddMember("Schema", e_s, alloc);
  document.AddMember("Encoding", encoding, alloc);

  std::string str_out;
  getJsonString(document, str_out);
  printf("Add MetaFrameSchema\n");

  SendNoReplyEvent("Stub::Dispatcher::OpenSDK", static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameSchema), 0, 
                    new ("Schema") Platform_Std_Refine::SerializableString(str_out.c_str()));
}

void Classification::SetMetaFrameCapabilitySchema() {
  string app_id = manifest_.app_name;
  string capabilities =
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Class/tt:Type\","
      "\"type\": \"xs:int\","
      "\"minimum\": 0,"
      "\"maximum\": 999"
      "},"
      "{"
      "\"xpath\": \"//tt:VideoAnalytics/tt:Frame/tt:Object/tt:Appearance/tt:Class/tt:Type/@Likelihood\","
      "\"type\": \"xs:float\","
      "\"minimum\": 0.0,"
      "\"maximum\": 1.0"
      "}";


  string str_out = "{\"AppID\": \"" + app_id + "\",\"Capabilities\": [" + capabilities + "]}";

  printf("Add MetaFrameCapabilitySchema\n");

  SendNoReplyEvent("Stub::Dispatcher::OpenSDK", static_cast<int32_t>(I_OpenSDKCGIDispatcher::EEventType::eMetaFrameCapability), 0, 
                    new ("Schema") Platform_Std_Refine::SerializableString(str_out.c_str()));
}

bool Classification::ProcessAEvent(Event *event)
{
  bool result = true;
  switch (event->GetType())
  {
    case (int32_t)IAppDispatcher::EEventType::eHttpRequest:
    {
      DebugLog(">> Classification::%s START", __func__);
      auto param = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
      string path_info = param->GetFCGXParam("PATH_INFO").c_str();
  
      if (path_info == "/configuration") {
        if (param->GetMethod() == "POST") {
          auto body = param->GetRequestBody();

          result = ParseNpuEvent(body);
          if (!result) {
            param->SetStatusCode(400);
            param->SetResponseBody("request error");
          }
          printf("<< Classification::ParseNpuEvent END (%s)\n", result ? "Pass" : "Fail");
          break;
        }
        param->SetStatusCode(400);
        param->SetResponseBody("requested method is not supported");
        break;
      }
      DebugLog("<< Classification::ParseNpuEvent END (%s)", result ? "Pass" : "Fail");
      param->SetStatusCode(400);
      param->SetResponseBody("requested method is not supported");
      break;
    }
    case (int32_t)IAppDispatcher::EEventType::eHttpResponse:
    {
      break;
    }
    case (int32_t)ComponentInterface::EProtocolEventType::eSet:
    {
      HandleRequest(event);
      break;
    }
    case (int32_t)ComponentInterface::EProtocolEventType::eRemove:
    {
      HandleRequest(event);
      break;
    }
    case static_cast<int32_t>(IPStreamProviderManagerVideoRaw::EEventType::eVideoRawData):
      ProcessRawVideo(event);
      break;
    default:
      base::ProcessAEvent(event);
      break;
  }
  return result;
}

void Classification::Start()
{
  base::Start();
}


NeuralNetwork* Classification::GetOrCreateNetwork(const string& name) {
  auto* network = GetNetwork(name);

  if (!network) {
    network = NeuralNetwork::Create();
    nn_map_[name].reset(network);
  }

  return network;
}

NeuralNetwork* Classification::GetNetwork(const string& name) {
  auto it = nn_map_.find(name);
  if (it != nn_map_.end()) {
    return it->second.get();
  }

  return nullptr;
}

Classification::NeuralNeworkMap& Classification::GetAllNetworks() { return nn_map_; }

void Classification::RemoveNetwork(const string& name) {
  Log::Print(">> PObjectDetectorAI::%s() START \n", __func__);
  auto it = nn_map_.find(name);
  if (it != nn_map_.end()) {
    it->second.reset();
    nn_map_.erase(it);
  }
  Log::Print("<< PObjectDetectorAI::%s() END \n", __func__);
}

void Classification::ProcessRawVideo(Event* event) {
  if (event == nullptr || event->IsReply()) {
    return;
  }

  auto& network_map = GetAllNetworks();
  if (network_map.size() == 0) { return; }

  auto blob = event->GetBlobArgument();
  event->ClearBaseObjectArgument(); //detach event data

  std::pair<std::variant<BaseObject*, char*>, uint64_t> ret((char*)blob.GetRawData(), blob.GetSize());

  IPVideoFrameRaw* raw_frame = new ("GetImage") IPLVideoFrameRaw();
  raw_frame->DeserializeBaseObject(raw_frame, ret);

  std::shared_ptr<RawImage> img(raw_frame->GetRawImage());
  if(img == nullptr) {
    return;
  }

  Inference(img);
  blob.ClearResource(); //release raw frame
}

std::string Classification::TimePointToString(uint64_t timestamp) const {
  time_t sec = (time_t)(timestamp / 1000);
  uint32_t msec = (uint32_t)(timestamp % 1000);
  auto conv_time = ::gmtime((const time_t*)&sec);
  std::stringstream ss("");
  ss << std::put_time(conv_time, "%FT%T.") << std::setfill('0') << std::setw(3) << msec << "Z";
  return ss.str();
}

bool Classification::UnloadNetwork(const string& model_path)
{
  DebugLog(">> Classification::%s:%d Start", __func__, __LINE__);
  auto network = GetNetwork(model_path);
  if (!network) { return false; }
  network->UnloadNetwork();
  RemoveNetwork(model_path);
  DebugLog("<< Classification::%s:%d End", __func__, __LINE__);
  return true;
}

void Classification::Inference(shared_ptr<RawImage> img)
{
  if (!run_flag) { return; }

  if (!img) { DebugLog("return @ %s:%d", __func__, __LINE__); return; }

  const shared_ptr<Tensor> rgb(Tensor::Create());

  int images_cnt = 0;
  for (auto *image = img.get(); image; image = image->next)
  {
    if (image->width != 0 && image->height != 0)
    {
      images_cnt++;
    }
  }

  if(images_cnt > 1)
  {
    for (auto *image = img.get(); image; image = image->next)
    {
      if (image->width < 3840 && image->height < 2160)
      {
        if (image->width <= MAX_SIZE && image->height <= MAX_SIZE){
          if (rgb->Allocate(*image) == false) { return; }
          break;
        }
      }
    }
  }
  else
  {
    if (rgb->Allocate(*img) == false) { return; }
  }

  if (!rgb) { return; }

  raw_pts = img->pts;
  for (auto& item : GetAllNetworks())
  {
    auto name = item.first;

    bool result = PreProcess(name, const_pointer_cast<Tensor>(rgb)) &&
                  Execute(name) &&
                  PostProcess(name, rgb);
    if (!result) {
      DebugLog("Failed @ %s (network name: %s)", __func__, name.c_str());
      break;
    }
  }
}

bool Classification::PreProcess(const string& model_path, shared_ptr<Tensor> rgb)
{
  auto* network = GetNetwork(model_path);
  if (!network) { return false; }

  const shared_ptr<Tensor> &input_tensor(network->GetInputTensor(0));
  img_size_t size = {
    .width = input_tensor->Length(0),
    .height = input_tensor->Length(1)
  };

  bool result = rgb->Resize(*input_tensor, size);

  return result;
}

bool Classification::Execute(const string& model_path)
{
  auto* network = GetNetwork(model_path);
  if (!network) { return false; }

  stat_t stat = { 0, };
  return network->RunNetwork(stat);
  //return true;
}

bool Classification::PostProcess(const string& model_path, const shared_ptr<Tensor>& img)
{
  auto* network = GetNetwork(model_path);
  if (!network) { return false; }

  for (uint32_t k = 0; k < network->GetOutputTensorCount(); k++)
  {
    const shared_ptr<Tensor>& output_tensor(network->GetOutputTensor(k));
    if (!output_tensor) { continue; }

    auto* result_bin = output_tensor->VirtAddr();
    if (!result_bin) { continue; }

    //parse result_bin for dedicated model
    int width = output_tensor->Length(0);
    parse_result = ParseResult(static_cast<float*>(result_bin), width);
    SendMetadata();
  }

  return true;
}

bool Classification::ParseResult(float* data, int out_width)
{
  memset(max_id, 0, sizeof(max_id));
  memset(max_val, 0, sizeof(max_val));

  for (int i = 0; i < out_width; i++) {
    if (max_val[0] < *(data + i)) {
      max_val[4] = max_val[3], max_id[4] = max_id[3];
      max_val[3] = max_val[2], max_id[3] = max_id[2];
      max_val[2] = max_val[1], max_id[2] = max_id[1];
      max_val[1] = max_val[0], max_id[1] = max_id[0];
      max_val[0] = *(data + i), max_id[0] = i;
    }
    else if (max_val[1] < *(data + i)) {
      max_val[4] = max_val[3], max_id[4] = max_id[3];
      max_val[3] = max_val[2], max_id[3] = max_id[2];
      max_val[2] = max_val[1], max_id[2] = max_id[1];
      max_val[1] = *(data + i), max_id[1] = i;
    }
    else if (max_val[2] < *(data + i)) {
      max_val[4] = max_val[3], max_id[4] = max_id[3];
      max_val[3] = max_val[2], max_id[3] = max_id[2];
      max_val[2] = *(data + i), max_id[2] = i;
    }
    else if (max_val[3] < *(data + i)) {
      max_val[4] = max_val[3], max_id[4] = max_id[3];
      max_val[3] = *(data + i), max_id[3] = i;
    }
    else if (max_val[4] < *(data + i)) {
      max_val[4] = *(data + i), max_id[4] = i;
    }
  }

  DebugLog("[class, prob] Top 5: [%d, %f] [%d, %f] [%d, %f] [%d, %f] [%d, %f]",
      max_id[0], max_val[0],
      max_id[1], max_val[1],
      max_id[2], max_val[2],
      max_id[3], max_val[3],
      max_id[4], max_val[4]);

  for (int i = 0; i < 5 ; i++) {
    if (max_val[i] < 0.0 || max_val[i] > 1.0) {
      DebugLog("Failed: False probability! [class, prob]: [%d, %f]", max_id[i], max_val[i]);
      return false;
    }
  }

  return true;
}

string Classification::GetXml(const int* max_id, const float* max_val, const int64_t timestamp) {
  string str_time = TimePointToString(timestamp);
  string str_object_id = "2";

  string star_xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  string MetadataStream = "<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\" xmlns:ttr=\"https://www.onvif.org/ver20/analytics/radiometry\" xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" xmlns:tns1=\"http://www.onvif.org/ver10/topics\" xmlns:tnssamsung=\"http://www.samsungcctv.com/2011/event/topics\" xmlns:fc=\"http://www.onvif.org/ver20/analytics/humanface\" xmlns:bd=\"http://www.onvif.org/ver20/analytics/humanbody\"><tt:VideoAnalytics>";
  string utc_time = "<tt:Frame UtcTime=\"" + str_time + "\">";
  string object = "<tt:Object ObjectId=\"" + str_object_id + "\"><tt:Appearance><tt:Class>";
  string typedata_0 = "<tt:Type Likelihood=\"" + to_string(max_val[0]) + "\">" + to_string(max_id[0]) + "</tt:Type>";
  string typedata_1 = "<tt:Type Likelihood=\"" + to_string(max_val[1]) + "\">" + to_string(max_id[1]) + "</tt:Type>";
  string typedata_2 = "<tt:Type Likelihood=\"" + to_string(max_val[2]) + "\">" + to_string(max_id[2]) + "</tt:Type>";
  string typedata_3 = "<tt:Type Likelihood=\"" + to_string(max_val[3]) + "\">" + to_string(max_id[3]) + "</tt:Type>";
  string typedata_4 = "<tt:Type Likelihood=\"" + to_string(max_val[4]) + "\">" + to_string(max_id[4]) + "</tt:Type>";
  string end_xml = "</tt:Class></tt:Appearance></tt:Object></tt:Frame></tt:VideoAnalytics></tt:MetadataStream>";

  string xml_data = star_xml + MetadataStream + utc_time + object + typedata_0 + typedata_1 + typedata_2 + typedata_3 + typedata_4 + end_xml;

  return xml_data;
}


void Classification::SendMetadata() {
  if (GetChannel() == 0) {
    auto timestamp = raw_pts;
    auto metadata = StringMetadata(GetChannel(), timestamp);
    metadata.Set(GetXml(max_id, max_val, timestamp));

    auto req = new ("MetadataRequest") IPMetadataManager::StringMetadataRequest();
    req->SetStringMetadata(std::move(metadata));

    SendNoReplyEvent("MetadataManager", static_cast<int32_t>(IMetadataManager::EEventType::eRequestRawMetadata), 0, req);
  }
}

bool Classification::ParseNpuEvent(const string& body)
{
  bool result = true;

  DebugLog(">> Classification::%s START", __func__);

  NeuralNetwork* network = nullptr;
  if (npu_load_info.model_name_.size() > 0)
  {
    network = GetNetwork(npu_load_info.model_name_);
  }
 
  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);

  document.Parse(body);
  if (document.HasParseError()) {
    return false;
  }

  switch (HashStr(document["mode"].GetString()))
  {
    case HashStr("create_network"):{
      result = CreateNetwork(network, document);
      break;
    }
    case HashStr("create_input_tensor"):{
      result = CreateTensor(network, document, "input_tensor");
      break;
    }
    case HashStr("create_output_tensor"):{
      result = CreateTensor(network, document, "output_tensor");
      break;
    }
    case HashStr("load_network"):{
      result = LoadNetwork(network);
      break;
    }
    case HashStr("run_network"):{
      result = RunNetwork(network);
      break;
    }
    case HashStr("check_parse_result"):{
      result = CheckParseResult(network);
      break;
    }
    case HashStr("unload_network"):{
      result = UnloadNetwork(network);
      break;
    }
    case HashStr("get_input_tensor_name"):{
      result = GetTensorName(network, document, "input_tensor");
      break;
    }
    case HashStr("get_output_tensor_name"):{
      result = GetTensorName(network, document, "output_tensor");
      break;
    }
    case HashStr("get_input_tensor_index"):{
      result = GetTensorIndex(network, document, "input_tensor");
      break;
    }
    case HashStr("get_output_tensor_index"):{
      result = GetTensorIndex(network, document, "output_tensor");
      break;
    }
    case HashStr("get_all_input_tensors"):{
      result = GetAllTensor(network, "input_tensor");
      break;
    }
    case HashStr("get_all_output_tensors"):{
      result = GetAllTensor(network, "output_tensor");
      break;
    }
    case HashStr("get_input_tensor_count"):{
      result = GetTensorCount(network, "input_tensor");
      break;
    }
    case HashStr("get_output_tensor_count"):{
      result = GetTensorCount(network, "output_tensor");
      break;
    }
    default:{
      result = false;
      break;
    }
  }

  return result;
}

bool Classification::CreateNetwork(NeuralNetwork* network, JsonUtility::JsonDocument& document)
{
  DebugLog("Create Network");
  if (network) {
    DebugLog("Failed: Network already exists (Model name: %s)", npu_load_info.model_name_.c_str());
    return false;
  }

  if (document["model_name"].GetString() != string("")) { run_neural_network_info_list->app_attribute_info.model_name = document["model_name"].GetString(); }
  WriteAttributes(run_neural_network_info_list.get(), this->GetObjectName());
  npu_load_info.model_name_ = run_neural_network_info_list->app_attribute_info.model_name;

  network = GetOrCreateNetwork(npu_load_info.model_name_);
  if (!network) {
    DebugLog("Failed: Network creation failed(model_name: %s)", npu_load_info.model_name_.c_str());
    return false;
  }
  return true;
}

bool Classification::CreateTensor(NeuralNetwork* network, JsonUtility::JsonDocument& document, const string& request)
{
  DebugLog("Create %s", request.c_str());
  if (!network) { return false; }

  string* p_tensor_name;
  string* p_npu_tensor_names;
  if (request == "input_tensor") {
    p_tensor_name = &run_neural_network_info_list->app_attribute_info.input_tensor_names;
    p_npu_tensor_names = &npu_load_info.input_tensor_names_;
  } else if (request == "output_tensor") {
    p_tensor_name = &run_neural_network_info_list->app_attribute_info.output_tensor_names;
    p_npu_tensor_names = &npu_load_info.output_tensor_names_;
  } else {
    DebugLog("Failed: requested mod is not supported(request: %s)", request.c_str());
    return false;
  }

  if (document[request].GetString() != string("")) { *p_tensor_name = document[request].GetString(); }
  WriteAttributes(run_neural_network_info_list.get(), this->GetObjectName());

  auto name_list = Split(*p_tensor_name, ',');
  for (auto name : name_list) {
    if (!InsertNpuLoadInfo(*p_npu_tensor_names, name)) { continue; }

    if (request == "input_tensor") {
      const shared_ptr<Tensor>& tensor(network->CreateInputTensor(name));
      if (!tensor) {
        DebugLog("Failed: Input tensor creation failed(input_tensor name: %s)", name.c_str());
        return false;
      }
    } else if (request == "output_tensor") {
      const shared_ptr<Tensor>& tensor(network->CreateOutputTensor(name));
      if (!tensor) {
        DebugLog("Failed: Output tensor creation failed(output_tensor name: %s)", name.c_str());
        return false;
      }
    }
  }
  return true;
}

bool Classification::LoadNetwork(NeuralNetwork* network)
{
  DebugLog("Load Network");
  if (!network || npu_load_info.input_tensor_names_.empty() || npu_load_info.output_tensor_names_.empty()) { return false; }
    
  const String open_sdk_path = "../res/ai_bin/";
  relative_model_path = open_sdk_path + npu_load_info.model_name_;

  auto mean = std::vector<float>{123.68, 116.779, 103.939};
  auto scale = std::vector<float>{1.0, 1.0, 1.0};

  if (!network->LoadNetwork(relative_model_path, mean, scale)) {
    DebugLog("Failed: Load Network failed(model_name: %s)", npu_load_info.model_name_.c_str());
    return false;
  }

  return true;
}

bool Classification::RunNetwork(NeuralNetwork* network)
{
  DebugLog("Run Network");
  if (!network || npu_load_info.input_tensor_names_.empty() || npu_load_info.output_tensor_names_.empty()) { return false; }

  run_flag = 1;
  return true;
}

bool Classification::CheckParseResult(NeuralNetwork* network)
{
  DebugLog("Check Parse Result");
  if (!network) { return false; }

  if (!parse_result) {
    DebugLog("Check ParseResult Failed!");
    return false;
  }
  return true;
}

bool Classification::UnloadNetwork(NeuralNetwork* network)
{
  DebugLog("Unload Network");
  if (!network) { return false; }

  network->UnloadNetwork();
  RemoveNetwork(npu_load_info.model_name_);
  npu_load_info.model_name_.clear();
  npu_load_info.input_tensor_names_.clear();
  npu_load_info.output_tensor_names_.clear();
  run_flag = 0;

  return true;
}

bool Classification::GetTensorName(NeuralNetwork* network, JsonUtility::JsonDocument& document, const string& request)
{
  DebugLog("Get %s_Name", request.c_str());
  if (!network) { return false; }

  string* p_tensor_name;
  if (request == "input_tensor") {
    p_tensor_name = &run_neural_network_info_list->app_attribute_info.get_index_input_name;
  } else if (request == "output_tensor") {
    p_tensor_name = &run_neural_network_info_list->app_attribute_info.get_index_output_name;
  } else {
    DebugLog("Failed: requested mod is not supported(request: %s)", request.c_str());
    return false;
  }

  if (document[request].GetString() != string("")) { *p_tensor_name = document[request].GetString(); }
  WriteAttributes(run_neural_network_info_list.get(), this->GetObjectName());

  if (request == "input_tensor") {
    const shared_ptr<Tensor>& tensor(network->GetInputTensor(*p_tensor_name));
    if (!tensor) {
      DebugLog("Failed: Get inputtensor failed(input_name: %s)", p_tensor_name->c_str());
      return false;
    }
  } else if (request == "output_tensor") {
    const shared_ptr<Tensor>& tensor(network->GetOutputTensor(*p_tensor_name));
    if (!tensor) {
      DebugLog("Failed: Get outputtensor failed(output_name: %s)", p_tensor_name->c_str());
      return false;
    }
  }
  return true;
}

bool Classification::GetTensorIndex(NeuralNetwork* network, JsonUtility::JsonDocument& document, const string& request)
{
  DebugLog("Get %s_Index", request.c_str());
  if (!network) { return false; }

  string* p_tensor_index;
  if (request == "input_tensor") {
    p_tensor_index = &run_neural_network_info_list->app_attribute_info.get_name_input_index;
  } else if (request == "output_tensor") {
    p_tensor_index = &run_neural_network_info_list->app_attribute_info.get_name_output_index;
  } else {
    DebugLog("Failed: requested mod is not supported(request: %s)", request.c_str());
    return false;
  }

  if (document[request].GetString() != string("")) { *p_tensor_index = document[request].GetString(); }
  WriteAttributes(run_neural_network_info_list.get(), this->GetObjectName());

  if (request == "input_tensor") {
    const shared_ptr<Tensor> &tensor(network->GetInputTensor(stoi(*p_tensor_index)));
    if (!tensor) {
      DebugLog("Failed: Get inputtensor failed(input_index: %d)", stoi(*p_tensor_index));
      return false;
    }
  } else if (request == "output_tensor") {
    const shared_ptr<Tensor>& tensor(network->GetOutputTensor(stoi(*p_tensor_index)));
    if (!tensor) {
      DebugLog("Failed: Get outputtensor failed(output_index: %d)", stoi(*p_tensor_index));
      return false;
    }
  }
  return true;
}

bool Classification::GetAllTensor(NeuralNetwork* network, const string& request)
{
  DebugLog("Get All %s", request.c_str());
  if (!network) { return false; }

  if (request == "input_tensor") {
    const vector<shared_ptr<Tensor>>& all_tensor(network->GetAllInputTensors());
    for (const auto& tensor : all_tensor) {
      if (!tensor) {
        DebugLog("Failed: Get all inputtensor failed");
        return false;
      }
    }
  } else if (request == "output_tensor") {
    const vector<shared_ptr<Tensor>>& all_tensor(network->GetAllOutputTensors());
    for (const auto& tensor : all_tensor) {
      if (!tensor) {
        DebugLog("Failed: Get all outputtensor failed");
        return false;
      }
    }
  } else {
    DebugLog("Failed: requested mod is not supported(request: %s)", request.c_str());
    return false;
  }
  return true;
}

bool Classification::GetTensorCount(NeuralNetwork* network, const string& request)
{
  DebugLog("Get %s Count", request.c_str());
  if (!network) { return false; }

  if (request == "input_tensor") {
    DebugLog("GetInputTensorCount(): %ld", network->GetInputTensorCount());
  } else if (request == "output_tensor") {
    DebugLog("GetOutputTensorCount(): %ld", network->GetOutputTensorCount());
  } else {
    DebugLog("Failed: requested mod is not supported(mod: %s)", request.c_str());
    return false;
  }
  return true;
}

bool Classification::InsertNpuLoadInfo(string& target, string recv_value)
{
  if (target.empty()) {
    target += recv_value;
  }
  else {
    if (target.find(recv_value) == string::npos) {
      target += ",";
      target += recv_value;
    }
    else {
      DebugLog("The tensor already exists.!![%s]", recv_value.c_str());
      return false;
    }
  }
  return true;
}

void Classification::RegisterOpenAPIURI() {
  Vector<String> methods;
  methods.push_back("POST");

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/configuration"), GetInstanceName(), methods);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, uriRequest);
}

Vector<String> Classification::Split(String line, char seperator)
{
  //TODO: Trim whitespace near seperator

  Vector<String> split_result;
  stringstream stream(line);

  string token;
  while (getline(stream, token, seperator))
  {
    split_result.push_back(token);
  }
  return split_result;
}

extern "C"
{
  Classification* create_component(void *mem_manager)
  {
    Component::allocator = decltype(Component::allocator)(mem_manager);
    Event::allocator = decltype(Event::allocator)(mem_manager);
    return new("Classification") Classification();
  }

  void destroy_component(Classification *ptr)
  {
    delete ptr;
  }
}
