#pragma once

#include <component.h>
#include <typedef_base.h>
#include "i_log_manager.h"

enum class _EPTestComponent {
  _eBegin = GET_LAYER_UID(_ELayer::_ePApplication),
  _eSampleComponent,
  _eEnd,
};

constexpr ClassID _SampleComponent_Id = GET_CLASS_UID(_EPTestComponent::_eSampleComponent);

class SampleComponent : public Component {
 public:
  SampleComponent();
  SampleComponent(ClassID id, const char* name);
  virtual ~SampleComponent();

  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;
  virtual void RegisterOpenAPIURI();

 private:
  void SendSunapiRequest(void);
  void HandleResponse(Event* event);
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
  struct AppInfo {
    std::string app_id;
    std::string app_name;
    std::string status;
    std::string installed_date;
    std::string version;
  };
  std::deque<AppInfo> app_infos_;
};
