#pragma once

#include "component.h"
#include "deepstream_handle.h"
#include "i_sample_component.h"
#include "i_p_app_profile_handler.h"
#include "dispatcher_serialize.h"

#include <map>
#include <mutex>

#include <nvbufsurface.h>

class SampleComponent : public Component, public ISampleComponent {
public:
  SampleComponent();
  SampleComponent(ClassID id, const char *name);
  virtual ~SampleComponent();
  bool ProcessAEvent(Event *event) override;

  AppPack &GetApp() { return app_; }
  IPAppProfileHandler::AppProfile &GetAppProfile() { return profile_; }

protected:
  bool Initialize() override;
  void Stop() override;

 private:
  bool HandleHttpRequest(Event *event);
  void RegisterURI();

  void ProcessRawVideo(Event* event);
  void ParseAppInfo(Event *event);

  void InitializeDynamicEvent();
  void RunDeepStream(int profile_index, const std::string& profile_name);
  void StopDeepStream();
  void GetFrameInfo(OpenAppSerializable *oas);
  void RemoveAiProfileInfo(JsonUtility::ValueType &parent);

  std::string DecodeUrl(const char *source, bool bSupportPlusEncoding);
  std::vector<std::string> SplitString(std::string_view target, char c);
  std::unordered_map<std::string, std::string>
  GetQueryMap(std::string_view query);

private:
 std::string app_id_;
 AppPack app_;
 IPAppProfileHandler::AppProfile profile_;
};
