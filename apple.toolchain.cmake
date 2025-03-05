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
    set(CMAKE_SYSTEM_PROCESSOR "arm64")
    set(CMAKE_OSX_ARCHITECTURES "${APPLE_ARCH}")
    set(VERSION_FLAG_PREFIX "iphoneos")
endif()

# 工具链配置
execute_process(
        COMMAND xcrun --sdk ${APPLE_SDK_NAME} --find clang
        OUTPUT_VARIABLE CLANG_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_C_COMPILER "${CLANG_PATH}" CACHE STRING "C编译器" FORCE)

execute_process(
        COMMAND xcrun --sdk ${APPLE_SDK_NAME} --find clang++
        OUTPUT_VARIABLE CLANGXX_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_CXX_COMPILER "${CLANGXX_PATH}" CACHE STRING "C++编译器" FORCE)
# ar配置
execute_process(
        COMMAND xcrun --sdk ${APPLE_SDK_NAME} --find ar
        OUTPUT_VARIABLE AR_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_AR "${AR_PATH}" CACHE FILEPATH "归档工具" FORCE)

# ranlib配置
execute_process(
        COMMAND xcrun --sdk ${APPLE_SDK_NAME} --find ranlib
        OUTPUT_VARIABLE RANLIB_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_RANLIB "${RANLIB_PATH}" CACHE FILEPATH "库索引工具" FORCE)

# 编译标志配置
string(APPEND CMAKE_C_FLAGS_INIT
        " -m${VERSION_FLAG_PREFIX}-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
)

string(APPEND CMAKE_CXX_FLAGS_INIT
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

execute_process(
        COMMAND xcrun --sdk ${SDK_NAME} --find ld
        OUTPUT_VARIABLE LD_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CMAKE_LINKER "${LD_PATH}" CACHE FILEPATH "链接器" FORCE)

if(APPLE_PLATFORM STREQUAL "MACOS")
    set(TARGET_TRIPLE "${APPLE_ARCH}-apple-macos${CMAKE_OSX_DEPLOYMENT_TARGET}")
elseif(APPLE_PLATFORM STREQUAL "IOS")
    set(TARGET_TRIPLE "${APPLE_ARCH}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT
        " -target ${TARGET_TRIPLE}"
)

# iOS特殊链接参数
if(APPLE_PLATFORM STREQUAL "IOS")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT
            " -miphoneos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
            " -fembed-bitcode"
            " -Xlinker -bitcode_verify -bitcode_bundle"  # 验证Bitcode完整性
    )
endif()

# 路径查找策略
set(CMAKE_FIND_ROOT_PATH "${CMAKE_OSX_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_LINK_SEARCH_START_STATIC ON)
set(CMAKE_LINK_SEARCH_END_STATIC ON)
if(APPLE_PLATFORM STREQUAL "IOS")
    set(CMAKE_LINK_DEPENDS_NO_SHARED ON)  # iOS应禁止链接共享库依赖
endif()