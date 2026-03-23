#pragma once

#include <string>

// Phase 5에서 구현 예정
// Eclipse Paho MQTT C 라이브러리 기반 MQTT 로거
// 현재는 빈 스텁 — Phase 1 빌드 통과용

class MqttLogger {
 public:
  MqttLogger() = default;
  ~MqttLogger() = default;

  bool Connect(const std::string& host, int port, const std::string& client_id);
  void Disconnect();
  void Publish(const std::string& topic, const std::string& payload);

  bool IsConnected() const { return connected_; }

 private:
  bool connected_ = false;
  std::string broker_host_;
  int broker_port_ = 1883;
};
