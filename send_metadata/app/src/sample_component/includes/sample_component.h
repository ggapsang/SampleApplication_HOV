#pragma once

#include "component.h"
#include "i_p_metadata_manager.h"
#include "i_metadata_manager.h"
#include "i_sample_component.h"
#include "i_log_manager.h"

class SampleComponent : public Component, public ISampleComponent {
 public:
  SampleComponent();
  SampleComponent(ClassID id, const char* name);
  virtual ~SampleComponent();

  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;

  virtual void RegisterOpenAPIURI();

 private:
  bool ParseHttpEvent(Event* event);

  bool ParseManifest(const std::string& manifest_path, ManifestInfo& info);
  void SendMetadata(class StreamMetadata&& metadata);
  void RequestMaxResolution();
  void ParseMaxResolution(const std::string& response);
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
  SampleAppInfoList attributes_info;
  ManifestInfo manifest_;
  uint32_t max_width;
  uint32_t max_height;

  std::string app_id_;
};
