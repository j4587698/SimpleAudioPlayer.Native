# Apple分平台工具链
cmake_minimum_required(VERSION 3.20)

# 平台类型选择 (必须显式指定)
set(APPLE_PLATFORM "MACOS" CACHE STRING "目标平台 (MACOS/IOS)")
set_property(CACHE APPLE_PLATFORM PROPERTY STRINGS MACOS IOS)

# 平台参数初始化
if(APPLE_PLATFORM STREQUAL "MACOS")
    # macOS参数配置
    set(APPLE_SDK_NAME "macosx")
    set(DEFAULT_ARCH "x86_64")
    set(SUPPORTED_ARCHS "x86_64;arm64")
    set(DEFAULT_DEPLOYMENT_TARGET "11.0")
elseif(APPLE_PLATFORM STREQUAL "IOS")
    # iOS参数配置
    set(APPLE_SDK_NAME "iphoneos")
    set(DEFAULT_ARCH "arm64")
    set(SUPPORTED_ARCHS "arm64;armv7")
    set(DEFAULT_DEPLOYMENT_TARGET "13.0")
else()
    message(FATAL_ERROR "不支持的平台类型: ${APPLE_PLATFORM}")
endif()

# 架构选择
set(APPLE_ARCH "${DEFAULT_ARCH}" CACHE STRING "目标架构")
set_property(CACHE APPLE_ARCH PROPERTY STRINGS ${SUPPORTED_ARCHS})
if(NOT APPLE_ARCH IN_LIST SUPPORTED_ARCHS)
    message(FATAL_ERROR "架构 ${APPLE_ARCH} 不支持 ${APPLE_PLATFORM} 平台")
endif()

# SDK路径获取
execute_process(
        COMMAND xcrun --sdk ${APPLE_SDK_NAME} --show-sdk-path
        OUTPUT_VARIABLE SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 系统配置
set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_OSX_SYSROOT "${SDK_PATH}")
set(CMAKE_OSX_DEPLOYMENT_TARGET "${DEFAULT_DEPLOYMENT_TARGET}" CACHE STRING "最低部署版本")

# 架构映射表
if(APPLE_PLATFORM STREQUAL "MACOS")
    set(CMAKE_SYSTEM_PROCESSOR "${APPLE_ARCH}")
    set(CMAKE_OSX_ARCHITECTURES "${APPLE_ARCH}")
    set(VERSION_FLAG_PREFIX "macosx")
elseif(APPLE_PLATFORM STREQUAL "IOS")
    set(CMAKE_SYSTEM_PROCESSOR "aarch64")
    set(CMAKE_OSX_ARCHITECTURES "${APPLE_ARCH}")
    set(VERSION_FLAG_PREFIX "iphoneos")
endif()

# 工具链配置
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_AR "/usr/bin/ar")
set(CMAKE_RANLIB "/usr/bin/ranlib")

# 编译标志配置
string(APPEND CMAKE_C_FLAGS_INIT
        " -isysroot \"${CMAKE_OSX_SYSROOT}\""
        " -m${VERSION_FLAG_PREFIX}-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
)

string(APPEND CMAKE_CXX_FLAGS_INIT
        " -isysroot \"${CMAKE_OSX_SYSROOT}\""
        " -m${VERSION_FLAG_PREFIX}-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
)

# iOS特殊处理
if(APPLE_PLATFORM STREQUAL "IOS")
    string(APPEND CMAKE_C_FLAGS_INIT
            " -target ${APPLE_ARCH}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}"
            " -fembed-bitcode"
    )
    string(APPEND CMAKE_CXX_FLAGS_INIT
            " -target ${APPLE_ARCH}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}"
            " -fembed-bitcode"
    )
endif()

# 路径查找策略
set(CMAKE_FIND_ROOT_PATH "${CMAKE_OSX_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
