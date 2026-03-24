#include "mqtt_logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <iostream>

MqttLogger::~MqttLogger()
{
  Disconnect();
}

bool MqttLogger::SendBytes(const void* data, size_t len)
{
  if (sock_ < 0) return false;
  ssize_t sent = ::send(sock_, data, len, MSG_NOSIGNAL);
  return sent == (ssize_t)len;
}

std::vector<uint8_t> MqttLogger::BuildConnectPacket(const std::string& cid)
{
  // MQTT 3.1.1 CONNECT packet
  // Variable header: protocol name "MQTT", level 4, flags 0x02 (clean session), keepalive 60s
  std::vector<uint8_t> var = {
    0x00, 0x04, 'M', 'Q', 'T', 'T',  // protocol name
    0x04,                               // protocol level (3.1.1)
    0x02,                               // connect flags (clean session)
    0x00, 0x3C                          // keepalive 60s
  };
  // Payload: client id
  uint16_t cid_len = (uint16_t)cid.size();
  var.push_back((uint8_t)(cid_len >> 8));
  var.push_back((uint8_t)(cid_len & 0xFF));
  var.insert(var.end(), cid.begin(), cid.end());

  // Fixed header: type=CONNECT(0x10), remaining length
  std::vector<uint8_t> pkt;
  pkt.push_back(0x10);
  // Encode remaining length (variable-length encoding)
  size_t rem = var.size();
  do {
    uint8_t b = rem & 0x7F;
    rem >>= 7;
    if (rem > 0) b |= 0x80;
    pkt.push_back(b);
  } while (rem > 0);
  pkt.insert(pkt.end(), var.begin(), var.end());
  return pkt;
}

std::vector<uint8_t> MqttLogger::BuildPublishPacket(const std::string& topic, const std::string& payload)
{
  // Variable header: topic
  std::vector<uint8_t> var;
  uint16_t tlen = (uint16_t)topic.size();
  var.push_back((uint8_t)(tlen >> 8));
  var.push_back((uint8_t)(tlen & 0xFF));
  var.insert(var.end(), topic.begin(), topic.end());
  // QoS 0: no packet identifier
  // Payload
  var.insert(var.end(), payload.begin(), payload.end());

  // Fixed header: type=PUBLISH(0x30), QoS 0
  std::vector<uint8_t> pkt;
  pkt.push_back(0x30);
  size_t rem = var.size();
  do {
    uint8_t b = rem & 0x7F;
    rem >>= 7;
    if (rem > 0) b |= 0x80;
    pkt.push_back(b);
  } while (rem > 0);
  pkt.insert(pkt.end(), var.begin(), var.end());
  return pkt;
}

bool MqttLogger::Connect(const std::string& host, int port, const std::string& client_id)
{
  Disconnect();

  if (host.empty()) return false;

  broker_host_ = host;
  broker_port_ = port;
  client_id_ = client_id;

  // DNS resolve
  struct addrinfo hints{}, *res = nullptr;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  std::string port_str = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
    std::cout << "[MQTT] DNS resolve failed: " << host << std::endl;
    return false;
  }

  sock_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sock_ < 0) {
    freeaddrinfo(res);
    std::cout << "[MQTT] socket() failed" << std::endl;
    return false;
  }

  // 연결 타임아웃 2초 설정
  struct timeval tv{2, 0};
  setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  if (::connect(sock_, res->ai_addr, res->ai_addrlen) < 0) {
    freeaddrinfo(res);
    ::close(sock_);
    sock_ = -1;
    std::cout << "[MQTT] connect() failed: " << host << ":" << port << std::endl;
    return false;
  }
  freeaddrinfo(res);

  // MQTT CONNECT 패킷 전송
  auto pkt = BuildConnectPacket(client_id);
  if (!SendBytes(pkt.data(), pkt.size())) {
    ::close(sock_);
    sock_ = -1;
    std::cout << "[MQTT] CONNECT send failed" << std::endl;
    return false;
  }

  // CONNACK 수신 (4바이트)
  uint8_t connack[4] = {};
  ssize_t n = ::recv(sock_, connack, 4, 0);
  if (n != 4 || connack[0] != 0x20 || connack[3] != 0x00) {
    ::close(sock_);
    sock_ = -1;
    std::cout << "[MQTT] CONNACK failed (n=" << n << ")" << std::endl;
    return false;
  }

  connected_ = true;
  std::cout << "[MQTT] Connected to " << host << ":" << port << std::endl;
  return true;
}

void MqttLogger::Disconnect()
{
  if (sock_ >= 0) {
    if (connected_) {
      uint8_t disc[] = {0xE0, 0x00};
      SendBytes(disc, 2);
    }
    ::close(sock_);
    sock_ = -1;
  }
  connected_ = false;
}

void MqttLogger::Publish(const std::string& topic, const std::string& payload)
{
  if (!connected_) return;
  auto pkt = BuildPublishPacket(topic, payload);
  if (!SendBytes(pkt.data(), pkt.size())) {
    // 전송 실패 → 연결 끊김으로 간주
    connected_ = false;
    ::close(sock_);
    sock_ = -1;
  }
}

void MqttLogger::PublishDetection(int frame_id, int det_count, float max_conf)
{
  if (!connected_) return;
  std::string json = "{\"frame\":" + std::to_string(frame_id) +
                     ",\"det\":" + std::to_string(det_count) +
                     ",\"conf\":" + std::to_string((int)(max_conf * 100)) + "}";
  Publish("hand/detection", json);
}

void MqttLogger::PublishDebug(float pre_ms, float inf_ms, float post_ms, int det_count)
{
  if (!connected_) return;
  std::string json = "{\"pre\":" + std::to_string((int)pre_ms) +
                     ",\"inf\":" + std::to_string((int)inf_ms) +
                     ",\"post\":" + std::to_string((int)post_ms) +
                     ",\"det\":" + std::to_string(det_count) + "}";
  Publish("hand/debug", json);
}

void MqttLogger::PublishStatus(const std::string& state, const std::string& model)
{
  if (!connected_) return;
  Publish("hand/status", "{\"state\":\"" + state + "\",\"model\":\"" + model + "\"}");
}
