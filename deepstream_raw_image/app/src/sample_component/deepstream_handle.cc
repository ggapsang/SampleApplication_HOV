#include "deepstream_handle.h"

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <thread>

#include "gstnvdsmeta.h"
#include "i_log_manager.h"
#include "i_metadata_manager.h"
#include "i_p_metadata_manager.h"
#include "i_pl_video_frame_raw.h"
#include "metadata_writer.h"
#include "nvbufsurface.h"

constexpr std::string_view PipelineName = "DeepStreamPipeline";

constexpr uint32_t PGIE_CLASS_ID_VEHICLE = 0;
constexpr uint32_t PGIE_CLASS_ID_PERSON = 2;

static uint64_t sequence = 0;

static std::string decimal_format(float f, int digits) {
  std::string str = std::to_string(f);
  auto pos = str.find('.');
  if (pos != std::string::npos) {
    str = str.substr(0, pos + digits + 1);
  }
  return str;
}

static std::string get_xml(const FrameMetadataItem &frame_meta) {
  time_t sec = (time_t)(frame_meta.timestamp() / 1000);
  uint32_t msec = (uint32_t)(frame_meta.timestamp() % 1000);
  auto conv_time = ::gmtime((const time_t *)&sec);
  std::stringstream ss("");
  ss << std::put_time(conv_time, "%FT%T.") << std::setfill('0') << std::setw(3)
     << msec << "Z";

  std::string str_meta = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
  str_meta +=
      "<tt:MetadataStream xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
      "xmlns:ttr=\"https://www.onvif.org/ver20/analytics/radiometry\" "
      "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
      "xmlns:tns1=\"http://www.onvif.org/ver10/topics\" "
      "xmlns:tnssamsung=\"http://www.samsungcctv.com/2011/event/topics\" "
      "xmlns:fc=\"http://www.onvif.org/ver20/analytics/humanface\" "
      "xmlns:bd=\"http://www.onvif.org/ver20/analytics/"
      "humanbody\"><tt:VideoAnalytics>";

  str_meta += "<tt:Frame UtcTime=\"" + ss.str() + "\"";
  str_meta += (frame_meta.source().empty()
                   ? ""
                   : std::string(" Source=\"") + frame_meta.source() + "\"");
  str_meta += ">";

  for (auto &o : frame_meta.object()) {
    str_meta += "<tt:Object ObjectId=\"" + std::to_string(o.object_id) +
                "\"><tt:Appearance><tt:Class>";
    str_meta += "<tt:Type Likelihood=\"" + decimal_format(o.likelihood, 1) +
                "\">" + metadata::common::GetOnvifObjectType(o.category) +
                "</tt:Type></tt:Class></tt:Appearance></tt:Object>";
  }

  str_meta += "</tt:Frame></tt:VideoAnalytics></tt:MetadataStream>";

  return str_meta;
}

static std::string timepoint_to_string(uint64_t timestamp) {
  time_t sec = (time_t)(timestamp / 1000);
  uint32_t msec = (uint32_t)(timestamp % 1000);
  auto conv_time = ::gmtime((const time_t *)&sec);
  std::stringstream ss("");
  ss << std::put_time(conv_time, "%FT%T.") << std::setfill('0') << std::setw(3)
     << msec << "Z";
  return ss.str();
}

void MetadataSender::SendMetadata(StreamMetadata &metadata) {
  if (metadata.frame_metadata().object().empty()) {
    return;
  }

  auto &frame = metadata.frame_metadata();

  for (auto &o : frame.object()) {
    if (GetFrameMetaInfoRef().size() > 10) {
      GetFrameMetaInfoRef().pop_front();
    }

    FrameMetaInfo info;
    info.sequence = sequence++;
    info.timestamp = timepoint_to_string(frame.timestamp());
    info.parent_id = o.parent_id;
    info.object_id = o.object_id;
    info.category = metadata::common::GetOnvifObjectType(o.category);
    info.sx = o.rect.sx;
    info.sy = o.rect.sy;
    info.ex = o.rect.ex;
    info.ey = o.rect.ey;

    GetFrameMetaInfoRef().push_back(std::move(info));
  }

  auto writer = MetadataWriter::Create("stream_metadata_ai_xml");
  if (writer) {
    metadata.Accept(*writer);

    auto m =
        StringMetadata(0 /* Channel */, metadata.frame_metadata().timestamp());
    m.Set(std::get<1>(writer->Get()));

    auto request = new ("Request") IPMetadataManager::StringMetadataRequest();
    request->SetStringMetadata(std::move(m));

    SendNoReplyEvent(
        "MetadataManager",
        static_cast<int32_t>(IMetadataManager::EEventType::eRequestRawMetadata),
        0, request);
  }
}

static GstFlowReturn new_sample(GstElement *sink, gpointer user_data) {
  auto metadata_sender = static_cast<MetadataSender *>(user_data);
  auto sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));

  if (gst_app_sink_is_eos(GST_APP_SINK(sink))) {
    LOG(PipelineName.data(), __func__, __LINE__, "EOS received in Appsink");
  }

  if (sample) {
    auto buf = gst_sample_get_buffer(sample);
    auto batch_meta = gst_buffer_get_nvds_batch_meta(buf);

    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();

    for (auto l_frame = batch_meta->frame_meta_list; l_frame != NULL;
         l_frame = l_frame->next) {
      NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);

      auto frame = FrameMetadataItem();
      frame.set_resolution(1920, 1080);
      frame.set_timestamp(timestamp);
      frame.set_source(metadata_sender->source());

      // int offset = 0;
      for (auto l_obj = frame_meta->obj_meta_list; l_obj != NULL;
           l_obj = l_obj->next) {
        auto obj_meta = (NvDsObjectMeta *)(l_obj->data);

        auto obj = metadata::common::Object();

        obj.object_id = obj_meta->object_id;
        if (obj_meta->parent) {
          obj.parent_id = obj_meta->parent->object_id;
        }
        obj.timestamp = timestamp;
        obj.likelihood = obj_meta->confidence;
        if (obj_meta->class_id == PGIE_CLASS_ID_VEHICLE) {
          obj.category = metadata::common::ObjectCategory::kVehicle;
        } else if (obj_meta->class_id == PGIE_CLASS_ID_PERSON) {
          obj.category = metadata::common::ObjectCategory::kHuman;
        } else {
          obj.category = metadata::common::ObjectCategory::kOther;
        }
        obj.rect.sx =
            (int32_t)(obj_meta->detector_bbox_info.org_bbox_coords.left);
        obj.rect.sy =
            (int32_t)(obj_meta->detector_bbox_info.org_bbox_coords.top);
        obj.rect.ex =
            (int32_t)(obj_meta->detector_bbox_info.org_bbox_coords.left +
                      obj_meta->detector_bbox_info.org_bbox_coords.width);
        obj.rect.ey =
            (int32_t)(obj_meta->detector_bbox_info.org_bbox_coords.top +
                      obj_meta->detector_bbox_info.org_bbox_coords.height);

#if 0
        g_print(
            "[CHECK] frame metadata PID[%d] OID[%d], likehood[%f] "
            "category[%s] rect[start(%d,%d),end(%d,%d)]\n",
            obj.parent_id, obj.object_id, obj.likelihood, metadata::common::GetOnvifObjectType(obj.category).c_str(), obj.rect.sx, obj.rect.sy, obj.rect.ex,
            obj.rect.ey);
#endif
        frame.add_object(std::move(obj));
      }

      auto metadata = StreamMetadata(0 /* Channel */, timestamp,
                                     Metadata::MetadataType::kAI);
      metadata.set_frame_metadata(std::move(frame));

      metadata_sender->SendMetadata(metadata);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

static void notify_to_destroy(gpointer user_data) { g_free(user_data); }

static void start_rtsp_streaming(AppPack &app, guint64 udp_buffer_size) {
  LOG(PipelineName.data(), __func__, __LINE__, "\t>> @begin");
  if (udp_buffer_size == 0) {
    udp_buffer_size = 512 * 1024;
  }
  auto &server = app.server;
  server = gst_rtsp_server_new();
  if (server == NULL) {
    LOG(PipelineName.data(), __func__, __LINE__, "rtsp server is nullptr");
    LOG(PipelineName.data(), __func__, __LINE__, "\t>> @end");
    return;
  }

  std::ostringstream oss;

  oss << "( udpsrc uri=udp://127.0.0.1 port=5000 caps=\"application/x-rtp, "
         "media=(string)video, payload=(int)96, clock-rate=(int)90000, "
         "encoding-name=(string)H264 \"  ! rtph264depay ! rtph264pay name=pay0 "
         ")";

  LOG(PipelineName.data(), __func__, __LINE__, oss.str());

  gst_rtsp_server_set_service(server, std::to_string(app.rtsp_port).c_str());

  auto mounts = gst_rtsp_server_get_mount_points(server);
  auto factory = gst_rtsp_media_factory_new();

  std::string rtsp_server_launch = oss.str();
  gst_rtsp_media_factory_set_launch(factory, rtsp_server_launch.c_str());
  gst_rtsp_media_factory_set_shared(factory, TRUE);
  gst_rtsp_media_factory_set_eos_shutdown(factory, TRUE);

  gst_rtsp_mount_points_add_factory(mounts, "/deepstream_raw_image", factory);

  g_object_unref(mounts);

  app.server_id = gst_rtsp_server_attach(server, NULL);
  if (app.server_id < 0) {
    LOG(PipelineName.data(), __func__, __LINE__, "RTSP server attach failed");
  } else {
    LOG(PipelineName.data(), __func__, __LINE__,
        "RTSP streaming at rtsp://127.0.0.1:" + std::to_string(app.rtsp_port) +
            "/deepstream_raw_image");
  }

  LOG(PipelineName.data(), __func__, __LINE__, "\t>> @end");
}

static GstRTSPFilterResult client_filter(GstRTSPServer *server,
                                         GstRTSPClient *client,
                                         gpointer user_data) {
  return GST_RTSP_FILTER_REMOVE;
}

static void stop_rtsp_streaming(AppPack &app) {
  auto &server = app.server;
  auto mounts = gst_rtsp_server_get_mount_points(server);
  gst_rtsp_mount_points_remove_factory(mounts, "/deepstream_raw_image");
  g_object_unref(mounts);
  gst_rtsp_server_client_filter(server, client_filter, NULL);
  auto pool = gst_rtsp_server_get_session_pool(server);
  gst_rtsp_session_pool_cleanup(pool);
  g_object_unref(pool);
}

void FeedFrame(AppPack &app, uint64_t pts, int32_t dmabuf_fd) {
  auto &appsrc = app.appsrc;

  if (appsrc == NULL) {
    return;
  }

  GstBuffer *buffer = NULL;
  GstFlowReturn ret;
  GstMapInfo map = {0};
  gpointer data = NULL, user_data = NULL;
  GstMemoryFlags flags = (GstMemoryFlags)0;

  NvBufSurface *nvbuf_surf = nullptr;
  if (NvBufSurfaceFromFd(dmabuf_fd, (void **)(&nvbuf_surf)) != 0) {
    LOG(PipelineName.data(), __func__, __LINE__, "NvBufSurface get failed...");
    return;
  }

  user_data = g_malloc(sizeof(uint64_t));
  *(uint64_t *)user_data = nvbuf_surf->surfaceList[0].bufferDesc;

  data = g_malloc(sizeof(NvBufSurface));
  buffer = gst_buffer_new_wrapped_full(flags, data, sizeof(NvBufSurface), 0,
                                       sizeof(NvBufSurface), user_data,
                                       notify_to_destroy);

  // LOG(PipelineName.data(), __func__, __LINE__, "pts : " +
  // std::to_string(pts));

  GST_BUFFER_PTS(buffer) = pts;

  gst_buffer_map(buffer, &map, GST_MAP_WRITE);
  memcpy(map.data, nvbuf_surf, sizeof(NvBufSurface));
  gst_buffer_unmap(buffer, &map);

  g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
  if (ret != 0) {
    LOG(PipelineName.data(), __func__, __LINE__,
        "appsrc ret " + std::to_string(ret));
  }

  gst_buffer_unref(buffer);
}

void StartPipeline(AppPack &app, IPAppProfileHandler::AppProfile &profile) {
  LOG(PipelineName.data(), __func__, __LINE__, "\t>> @begin");

  auto t = std::thread([&app, &profile]() {
    LOG(PipelineName.data(), __func__, __LINE__, "*** pipeline begin ***");

    if (gst_is_initialized() == FALSE) {
      gst_init(NULL, NULL);
    }

    auto &loop = app.loop;
    auto &pipeline = app.pipeline;
    auto &appsrc = app.appsrc;

    loop = g_main_loop_new(NULL, FALSE);

    std::ostringstream oss;

    // Please refer this link to use accelerated gstreamer
    // https://docs.nvidia.com/jetson/archives/r35.4.1/DeveloperGuide/text/SD/Multimedia/AcceleratedGstreamer.html#encode-examples

    // Configuration below is just for sample application.
    oss << "appsrc name=appsrc is-live=true ! "
        << "video/x-raw(memory:NVMM),width=" << profile.width_
        << ",height=" << profile.height_
        << ",format=NV12,framerate=" << profile.framerate_ << "/1 ! "
        << "nvvidconv ! m.sink_0 nvstreammux name=m batch-size=1 "
           "batched-push-timeout=40000  width=1920 height=1080 live-source=1 ! "
        << "nvinfer config-file-path=pgie_config.yml ! tee name=t ! queue ! "
        << "nvvidconv ! nvdsosd ! nvvidconv ! "
        << "nvv4l2h264enc maxperf-enable=1 profile=2 insert-sps-pps=1 "
           "idrinterval=10 poc-type=2 iframeinterval=30  insert-aud=1 "
           "insert-vui=1 bitrate="
        << profile.bitrate_ << " ! "
        << "h264parse ! rtph264pay name=pay0 pt=96 ! "
        << "udpsink host=127.0.0.1 port=5000 async=false sync=true "
        << "t. ! queue ! appsink name=appsink ";

    LOG(PipelineName.data(), __func__, __LINE__, oss.str());

    GError *error = NULL;

    std::string launch_string = oss.str();
    pipeline = gst_parse_launch(launch_string.c_str(), &error);
    if (!pipeline) {
      LOG(PipelineName.data(), __func__, __LINE__, error->message);
      return;
    }
    if (error) {
      g_error_free(error);
    }

    appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc");
    if (!appsrc) {
      LOG(PipelineName.data(), __func__, __LINE__, "appsrc is nullptr");
      return;
    }
    gst_app_src_set_stream_type(GST_APP_SRC(appsrc),
                                GST_APP_STREAM_TYPE_STREAM);

    auto appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink");
    if (!appsink) {
      LOG(PipelineName.data(), __func__, __LINE__, "appsink is nullptr");
      return;
    }
    g_object_set(appsink, "emit-signals", TRUE, "async", FALSE, NULL);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(new_sample),
                     app.metadata_sender.get());

    gst_element_set_state((GstElement *)pipeline, GST_STATE_PLAYING);

    app.is_playing = true;

    start_rtsp_streaming(app, 0);

    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_poll(bus, GST_MESSAGE_EOS, GST_CLOCK_TIME_NONE);

    gst_element_set_state((GstElement *)pipeline, GST_STATE_NULL);

    stop_rtsp_streaming(app);

    g_source_remove(app.server_id);
    app.server_id = -1;

    while (0 < GST_OBJECT_REFCOUNT_VALUE(app.server)) {
      g_print("Ref Count: %u\n", GST_OBJECT_REFCOUNT_VALUE(app.server));
      g_object_unref(app.server);
    }

    gst_object_unref(GST_OBJECT(pipeline));
    g_main_loop_unref(loop);

    LOG(PipelineName.data(), __func__, __LINE__, "*** pipeline end ***");
  });

  t.detach();

  LOG(PipelineName.data(), __func__, __LINE__, "\t>> @end");
}

void StopPipeline(AppPack &app) {
  LOG(PipelineName.data(), __func__, __LINE__, "\t>> @begin");

  gst_element_send_event(app.pipeline, gst_event_new_eos());

  app.is_playing = false;
  app.timestamp = 0;

  LOG(PipelineName.data(), __func__, __LINE__, "\t>> @end");
}
