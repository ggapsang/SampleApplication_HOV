#pragma once

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <cstdint>

#include "i_p_app_profile_handler.h"

class StreamMetadata;

struct FrameMetaInfo {
  uint32_t sequence;
  std::string timestamp;
  uint32_t parent_id;
  uint32_t object_id;
  std::string category;
  int sx;
  int sy;
  int ex;
  int ey;
};

class MetadataSender : public ComponentPart {
public:
  void SendMetadata(StreamMetadata& metadata);

  inline std::deque<FrameMetaInfo>& GetFrameMetaInfoRef() { return frame_meta_info_; }
  inline void set_source(std::string_view source) { source_ = source; }
  inline const std::string& source() const { return source_; }

 private:
  std::deque<FrameMetaInfo> frame_meta_info_;
  std::string source_ = "";
};

struct AppPack {
  GMainLoop *loop = NULL;
  GstElement *pipeline = NULL;
  GstElement *appsrc = NULL;
  GstRTSPServer* server = NULL;
  bool is_playing = false;
  GstClockTime timestamp = 0;
  int rtsp_port = 8554;
  std::unique_ptr<MetadataSender> metadata_sender;
  int server_id = -1;
};

void FeedFrame(AppPack& app, uint64_t pts, int32_t dmabuf_fd);
void StartPipeline(AppPack &app, IPAppProfileHandler::AppProfile& profile);
void StopPipeline(AppPack &app);
