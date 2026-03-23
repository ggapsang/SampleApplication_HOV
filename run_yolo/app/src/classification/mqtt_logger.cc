#include "mqtt_logger.h"

// Phase 5에서 구현 예정
// 현재는 빈 스텁 — Phase 1 빌드 통과용

bool MqttLogger::Connect(const std::string& host, int port, const std::string& client_id)
{
  broker_host_ = host;
  broker_port_ = port;
  // TODO Phase 5: Paho MQTT C 연결 구현
  return false;
}

void MqttLogger::Disconnect()
{
  connected_ = false;
  // TODO Phase 5: Paho MQTT C 연결 해제 구현
}

void MqttLogger::Publish(const std::string& topic, const std::string& payload)
{
  // TODO Phase 5: QoS 0 publish 구현
  (void)topic;
  (void)payload;
}
