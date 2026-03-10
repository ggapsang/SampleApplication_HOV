
#include "sample_component.h"

#include <dispatcher_serialize.h>
#include <i_app_dispatcher.h>
#include <i_p_audio_frame_raw.h>
#include <i_p_stream_provider_manager_audio_raw.h>

SampleComponent::SampleComponent() : SampleComponent(_SampleComponent_Id, "SampleComponent") {}

SampleComponent::SampleComponent(ClassID id, const char* name) : Component(id, name) {}

SampleComponent::~SampleComponent() {}

bool SampleComponent::Initialize() {
  RegisterOpenAPIURI();
  return Component::Initialize();
}

bool SampleComponent::ProcessAEvent(Event* event) {
  switch (event->GetType()) {
    case static_cast<int>(IPStreamProviderManagerAudioRaw::EEventType::eAudioRawData): {
      HandleAudioRaw(event);
      break;
    }
    case static_cast<int32_t>(IAppDispatcher::EEventType::eHttpRequest): {
      ParseHttpEvent(event);
      break;
    }
    default:
      Component::ProcessAEvent(event);
      break;
  }
  return true;
}

void SampleComponent::HandleAudioRaw(Event* event) {
  DebugLog("@@@@@@@@@@@@@@@@@@@SampleComponent Raw Audio@@@@@@@@@@@@@@@@@@");
  auto blob = event->GetBlobArgument();
  std::pair<std::variant<BaseObject*, char*>, uint64_t> ret((char*)blob.GetRawData(), blob.GetSize());

  IPAudioFrameRaw audio_frame_raw;
  audio_frame_raw.DeserializeBaseObject(&audio_frame_raw, ret);

  auto raw_audio = audio_frame_raw.GetRawAudio();
  if (raw_audio == nullptr) {
    DebugLog("ERROR! raw audio is nullptr");
    return;
  }

  uint64_t timestamp = raw_audio->GetHeader()->packet_time_sec * (uint64_t)1000 + raw_audio->GetHeader()->packet_time_usec / 1000;
  DebugLog("timestamp : %lu", timestamp);
  DebugLog("samplerate : %u", raw_audio->GetHeader()->samplerate);
  DebugLog("amplitude : %d", raw_audio->GetHeader()->amplitude);
  std::string codec_type = "None";
  if (raw_audio->IsPCM()) {
    codec_type = "PCM";
  }
  DebugLog("codec type : %s", codec_type.c_str());

  // auto data = raw_audio->Data();
  auto data_size = raw_audio->DataSize();
  DebugLog("data size : %d", data_size);

  DebugLog("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");

  if (audio_raw_infos_.size() > 10) {
    audio_raw_infos_.pop_front();
  }

  audio_raw_infos_.push_back(AudioRawInfo{
      .timestamp = timestamp, .samplerate = raw_audio->GetHeader()->samplerate, .codec_type = codec_type, .amplitude = raw_audio->GetHeader()->amplitude});
}

void SampleComponent::RegisterOpenAPIURI() {
  auto get_puts = Vector<String>{};
  get_puts.push_back("GET");

  auto uriRequest = new ("OpenAPI") IAppDispatcher::OpenAPIRegistrar(String("/sample"), GetInstanceName(), get_puts);

  SendNoReplyEvent("AppDispatcher", static_cast<int32_t>(IAppDispatcher::EEventType::eRegisterCommand), 0, uriRequest);
}

bool SampleComponent::ParseHttpEvent(Event* event) {
  if (event->IsReply()) {
    return true;
  }

  auto oas = static_cast<OpenAppSerializable*>(event->GetBaseObjectArgument());
  auto path_info = oas->GetFCGXParam("PATH_INFO");

  DebugLog("%s, %s:%d, %s", GetObjectName(), __func__, __LINE__, path_info.c_str());

  JsonUtility::JsonDocument document(JsonUtility::Type::kObjectType);
  auto& alloc = document.GetAllocator();

  auto array = JsonUtility::ValueType(JsonUtility::Type::kArrayType);

  for (auto& info : audio_raw_infos_) {
    auto object = JsonUtility::ValueType(JsonUtility::Type::kObjectType);
    object.AddMember("timestamp", info.timestamp, alloc);
    object.AddMember("samplerate", info.samplerate, alloc);
    object.AddMember("codec_type", info.codec_type, alloc);
    object.AddMember("amplitude", info.amplitude, alloc);

    array.PushBack(object, alloc);
  }

  document.AddMember("infos", array, alloc);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
  document.Accept(writer);

  oas->SetResponseBody(strbuf.GetString(), strbuf.GetLength());

  return true;
}

extern "C" {
SampleComponent* create_component(void* mem_manager) {
  Component::allocator = decltype(Component::allocator)(mem_manager);
  Event::allocator = decltype(Event::allocator)(mem_manager);
  return new ("SampleComponent") SampleComponent();
}

void destroy_component(SampleComponent* ptr) { delete ptr; }
}