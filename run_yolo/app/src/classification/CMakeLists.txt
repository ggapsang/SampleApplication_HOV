set(TARGET_LIB classification)

set(NPU_API_DIR ${WISENET_PRODUCT_RELEASE_PATH}/lib/npu_api/)

include_directories(${WISENET_PLATFORM_RELEASE_PATH}/includes)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/includes)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes/shared_buffer/video/includes)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes/metadata)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes/p_object_detector)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/lib/npu_api/includes)

link_directories(${WISENET_PLATFORM_RELEASE_PATH}/lib)
link_directories(${NPU_API_DIR}/libs)

link_libraries(metadata_manager)

#copy libraries which is not exist in library path of target board
file(COPY ${NPU_API_DIR}/libs/ DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/ PATTERN "*.so")
file(COPY ${WISENET_PLATFORM_RELEASE_PATH}/lib/libsituational_analytics_service.a DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/)

#link libraries which is not exist in library path of target board
link_directories(${CMAKE_HOME_DIRECTORY}/libs/app_dispatcher) #for libresponse_generator.so

if (SOC STREQUAL "cv5")
  include_directories(${WISENET_PRODUCT_RELEASE_PATH}/lib/cv5_sdk/video)
  link_libraries(sdk_cv5_video)
  link_libraries(cv5_technical_streamingService_sharedBuffer)
elseif(SOC STREQUAL "orinnx8g_jp512")
  include_directories(${WISENET_PRODUCT_RELEASE_PATH}/lib/nvidia/video/includes)
  link_libraries(technical_streamingService_nvidia)
  link_libraries(technical_streamingService_sharedBuffer)
  link_libraries(hash_utility_wrapper)
elseif(SOC STREQUAL "nt9869x")
  include_directories(${WISENET_PRODUCT_RELEASE_PATH}/lib/nt9869x_sdk/video)
  link_libraries(sdk_nt9869x_video)
  link_libraries(nt9869x_technical_streamingService_sharedBuffer)
elseif(SOC STREQUAL "wn9")
  include_directories(${WISENET_PRODUCT_RELEASE_PATH}/lib/wn9_sdk/video)
  link_libraries(sdk_wn9_video)
  link_libraries(wn9_technical_streamingService_sharedBuffer)
endif()
  link_libraries(npu_api)

add_library(${TARGET_LIB} MODULE classification.cc)

target_link_directories(${TARGET_LIB} PRIVATE ${NPU_API_DIR}/libs)
target_link_libraries(${TARGET_LIB} PRIVATE situational_analytics_service stdlib protocol_utility_wrapper response_generator)

install (TARGETS ${TARGET_LIB} DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${TARGET_LIB})
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manifests/ DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${TARGET_LIB} FILES_MATCHING PATTERN "*manifest.json")
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manifests/ DESTINATION ${CMAKE_HOME_DIRECTORY}/res/models FILES_MATCHING PATTERN "*instance*.json" PATTERN "*attribute*.json")
