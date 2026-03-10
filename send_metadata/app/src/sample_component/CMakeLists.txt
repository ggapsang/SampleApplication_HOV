set(TARGET_LIB sample_component)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/includes)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes/metadata)
include_directories(${APP_SUPPORT_PACKAGE}/sunapi_requester/includes)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/3rd_party/include/libxml2)
link_libraries(metadata_manager)
link_libraries(-lxml2)

add_library(${TARGET_LIB} MODULE sample_component.cc)

target_link_libraries(${TARGET_LIB} PRIVATE stdlib)

install (TARGETS ${TARGET_LIB} DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${TARGET_LIB})
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manifests/ DESTINATION ${CMAKE_HOME_DIRECTORY}/libs/${TARGET_LIB} FILES_MATCHING PATTERN "*manifest.json")
install (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/manifests/ DESTINATION ${CMAKE_HOME_DIRECTORY}/res/models FILES_MATCHING PATTERN "*instance*.json" PATTERN "*attribute*.json")
