include(toolchain.cmake)

set(NVIDIA_SDK_PATH /opt/opensdk/common/product/${SOC}/sdk)

set(DS_VERSION 6.3)
set(DS_SDK_PATH ${NVIDIA_SDK_PATH}/opt/nvidia/deepstream/deepstream-${DS_VERSION})
set(DS_LIB_PATH ${DS_SDK_PATH}/lib)
set(DS_SRC_INC_PATH ${DS_SDK_PATH}/sources/includes)
set(DS_SRC_APP_COMMON_INC_PATH ${DS_SDK_PATH}/sources/apps/apps-common/includes)

set(CUDA_VERSION 11.4)
set(CUDA_PATH ${NVIDIA_SDK_PATH}/usr/local/cuda-${CUDA_VERSION})
set(CUDA_INCLUDE_PATH ${CUDA_PATH}/include)
set(CUDA_LIB_PATH ${CUDA_PATH}/lib64)

set(NV_COMMON_INC_PATH ${NVIDIA_SDK_PATH}/usr/include)
set(NV_COMMON_LIB_PATH ${NVIDIA_SDK_PATH}/usr/lib)

message(STATUS "DeepStream SDK Version ${DS_VERSION}")
message(STATUS "DeepStream SDK Path ${DS_SDK_PATH}")
message(STATUS "DeepStream SDK Library Path ${DS_LIB_PATH}")
message(STATUS "DeepStream SDK Source Include Path ${DS_SRC_INC_PATH}")
message(STATUS "DeepStream SDK App Common Include Path ${DS_SRC_APP_COMMON_INC_PATH}")

message(STATUS "CUDA Version ${CUDA_VERSION}")
message(STATUS "CUDA Path ${CUDA_PATH}")
message(STATUS "CUDA Include Path ${CUDA_INCLUDE_PATH}")
message(STATUS "CUDA Library Path ${CUDA_LIB_PATH}")

message(STATUS "Nvidia Common Include Path ${NV_COMMON_INC_PATH}")
message(STATUS "Nvidia Common Library Path ${NV_COMMON_LIB_PATH}")
