#pragma once

#include <component.h>

class SampleComponent : public Component {
 public:
  SampleComponent();
  SampleComponent(ClassID id, const char* name);
  virtual ~SampleComponent();

  bool ProcessAEvent(Event* event) override;

 protected:
  bool Initialize() override;
  void RegisterOpenAPIURI();

 private:
  bool ParseHttpEvent(Event* event);
  void StartServer(const std::string& request);
  void SendImage();
  std::string GetHttpResponse(const std::string& file_path);

 private:
  int client_id_;
};
