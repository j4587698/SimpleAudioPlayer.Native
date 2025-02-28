# Apple平台工具链
cmake_minimum_required(VERSION 3.20)


# 自动获取Xcode SDK路径
execute_process(
    COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE SDK_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 平台参数配置
if("${APPLE_PLATFORM}" STREQUAL "MACOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
elseif("${APPLE_PLATFORM}" STREQUAL "IOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum iOS version")
    set(SDK_PATH ${SDK_PATH}/iPhoneOS.sdk)  # 修正为iOS SDK路径
endif()

set(CMAKE_SYSTEM_NAME Daw)

# 系统级配置
set(CMAKE_OSX_SYSROOT ${SDK_PATH})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isysroot ${SDK_PATH}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${SDK_PATH}")

# 禁用自动框架搜索
set(CMAKE_FIND_FRAMEWORK NEVER)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)