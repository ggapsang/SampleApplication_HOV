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
 private:
  enum class EEventType { eBegin = _SampleComponent_Id, eRequestSunapi, eRequestSunapiDone, eEnd };
  enum class SDCardEventType { kInserted, kRemoved };

  struct SDCardState {
    bool inserted;
  };

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
  void HandleSDCardStatus(SDCardEventType event, std::string_view param);
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
  std::map<int, SDCardState> sdcard_status_;
};
