set(TARGET_LIB sample_component)

add_library(${TARGET_LIB} MODULE sample_component.cc deepstream_handle.cc)

target_include_directories(${TARGET_LIB} PRIVATE ${WISENET_PRODUCT_RELEASE_PATH}/lib/npu_api/includes)

target_include_directories(${TARGET_LIB} PRIVATE ${APP_SUPPORT_PACKAGE})
target_include_directories(${TARGET_LIB} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/includes)
target_include_directories(${TARGET_LIB} PRIVATE ${WISENET_PRODUCT_RELEASE_PATH}/includes/shared_buffer/video/includes)
target_include_directories(${TARGET_LIB} PRIVATE ${WISENET_PRODUCT_RELEASE_PATH}/includes/metadata)

target_link_libraries(${TARGET_LIB} PRIVATE metadata_manager technical_streamingService_sharedBuffer hash_utility_wrapper)

# deepstream sdk related
target_include_directories(${TARGET_LIB} PRIVATE ${DS_SRC_INC_PATH} ${DS_SRC_APP_COMMON_INC_PATH} ${CUDA_INCLUDE_PATH} ${NV_COMMON_INC_PATH} ${NV_COMMON_INC_PATH}/tegra)
target_include_directories(${TARGET_LIB} PRIVATE ${NV_COMMON_INC_PATH}/gstreamer-1.0 ${NV_COMMON_INC_PATH}/glib-2.0 ${NV_COMMON_LIB_PATH}/aarch64-linux-gnu/glib-2.0/include)

target_link_directories(${TARGET_LIB} PRIVATE ${DS_LIB_PATH} ${CUDA_LIB_PATH} ${NV_COMMON_LIB_PATH} ${NV_COMMON_LIB_PATH}/aarch64-linux-gnu ${NV_COMMON_LIB_PATH}/aarch64-linux-gnu/tegra)
target_link_libraries(${TARGET_LIB} PRIVATE gstreamer-1.0 gobject-2.0 glib-2.0 gmodule-2.0 gstapp-1.0 gstrtspserver-1.0)

target_link_libraries(${TARGET_LIB} PRIVATE nvbufsurface nvbufsurftransform nvdsgst_meta nvds_meta)


install (TARGETS ${TARGET_LIB} DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${TARGET_LIB})
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manifests/ DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${TARGET_LIB} FILES_MATCHING PATTERN "*manifest.json")
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manifests/ DESTINATION ${CMAKE_HOME_DIRECTORY}/res/models FILES_MATCHING PATTERN "*instance*.json" PATTERN "*attribute*.json")
