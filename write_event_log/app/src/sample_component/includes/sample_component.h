#pragma once

#include "component.h"
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
  SampleAppInfoList info_list;
};
