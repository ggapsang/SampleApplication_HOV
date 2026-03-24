#pragma once

#include <string>
#include <vector>

// 최소 MQTT 3.1.1 클라이언트 (QoS 0 PUBLISH 전용)
// 외부 라이브러리 없이 POSIX TCP 소켓으로 직접 구현

class MqttLogger {
 public:
  MqttLogger() = default;
  ~MqttLogger();

  bool Connect(const std::string& host, int port, const std::string& client_id = "hand_detector");
  void Disconnect();
  void Publish(const std::string& topic, const std::string& payload);

  bool IsConnected() const { return connected_; }

  // 편의 메서드: JSON 형식 발행
  void PublishDetection(int frame_id, int det_count, float max_conf);
  void PublishDebug(float pre_ms, float inf_ms, float post_ms, int det_count);
  void PublishStatus(const std::string& state, const std::string& model);

 private:
  int sock_ = -1;
  bool connected_ = false;
  std::string broker_host_;
  int broker_port_ = 1883;
  std::string client_id_;

  bool SendBytes(const void* data, size_t len);
  std::vector<uint8_t> BuildConnectPacket(const std::string& client_id);
  std::vector<uint8_t> BuildPublishPacket(const std::string& topic, const std::string& payload);
};
