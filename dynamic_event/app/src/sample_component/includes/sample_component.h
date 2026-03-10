#pragma once

#include "component.h"
#include "typedef_base.h"
#include "i_log_manager.h"
#include "i_p_metadata_manager.h"
#include "i_metadata_manager.h"

enum class _EPTestComponent {
  _eBegin = GET_LAYER_UID(_ELayer::_ePApplication),
  _eSampleComponent,
  _eEnd,
};

constexpr ClassID _SampleComponent_Id = GET_CLASS_UID(_EPTestComponent::_eSampleComponent);

class SampleComponent : public Component {
 public:
  enum class EEventType { eBegin = _SampleComponent_Id, eRequestSunapi, eRequestSunapiDone, eEnd };

 public:
  SampleComponent();
  SampleComponent(ClassID id, const char* name);
  virtual ~SampleComponent();

  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;

  virtual void RegisterOpenAPIURI();
  
  void SendMetadataSchema(Event* event);
  void SendEventStatusSchema(Event* event);
  void SendMetadata(StreamMetadata&& metadata);

  void NotifyChannelEvent();
  void NotifyChannelEvent(Event* event);
  void NotifyEventMetadata(Event* event);
  void SendMetaFrameSchema(Event* event);
  void SendMetaFrameCapability(Event* event);

 private:
  struct SchemaInfo {
    std::string metadata_schema;
    std::string eventstatus_schema;
    std::string meta_frame_schema;
    std::string meta_frame_capability;
  };

 private:
  bool ParseHttpEvent(Event* event);
  void DebugLog(const char* format, ...)
  {
    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    auto* arg = new ("") Platform_Std_Refine::SerializableString(buffer);
    SendTargetEvents(
                    ILogManager::remote_debug_message_group, 
                    static_cast<int32_t>(ILogManager::EEvent::eRemoteDebugMessage), 
                    0,
                    arg
                    );
    std::cout << buffer << std::endl;
  }
 private:
  int metadata_send_count = 0;
  std::map<std::string, std::string> schema_infos_;
  std::string app_id_;
};
