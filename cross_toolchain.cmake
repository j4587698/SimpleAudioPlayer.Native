# cross_toolchain.cmake

# 基础配置
cmake_minimum_required(VERSION 3.20)
set(CMAKE_SYSTEM_NAME ${CROSS_TARGET})
set(CMAKE_SYSTEM_PROCESSOR ${CROSS_ARCH})

# 架构映射表
set(ARCH_MAP
    "arm=arm-linux-gnueabihf"
    "aarch64=aarch64-linux-gnu"
    "x86_64=x86_64-linux-gnu"
    "x86_64-windows=x86_64-w64-mingw32"
)

# 自动检测工具链前缀
# 正确分割键值对
list(APPEND ARCH_LIST ${ARCH_MAP})  # 先按分号分割成键值对

foreach(PAIR IN LISTS ARCH_LIST)
  # 分割键值
  string(REPLACE "=" ";" PAIR_LIST ${PAIR})
  list(GET PAIR_LIST 0 KEY)
  list(GET PAIR_LIST 1 VALUE)

  # 对比键名
  if("${CROSS_ARCH}" STREQUAL "${KEY}")
    set(TOOLCHAIN_PREFIX "${VALUE}")
    break()
  endif()
endforeach()

#----------------------------------------------------------
# 平台特定配置
#----------------------------------------------------------
if("${CROSS_TARGET}" STREQUAL "Windows")
	set(WIN32 TRUE) 
    # Windows交叉编译配置
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
    set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)
    set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

    # 优化标志
    set(CMAKE_C_FLAGS_RELEASE "-Os -ffunction-sections -fdata-sections")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-Wl,--gc-sections,--exclude-libs=ALL")

elseif("${CROSS_TARGET}" STREQUAL "Linux")
	set(LINUX TRUE) 
    # Linux交叉编译配置
    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
    #set(CMAKE_SYSROOT /usr/${TOOLCHAIN_PREFIX})

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
	set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -fPIC")

    # ARM优化
    if(CROSS_ARCH MATCHES "arm")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard -mfpu=neon-vfpv4")
    endif()

elseif("${CROSS_TARGET}" STREQUAL "macOS")
    # macOS/iOS通用配置
    # 自动获取 SDK 路径
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # 设置系统级参数
    set(CMAKE_OSX_SYSROOT ${SDK_PATH})
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures")
    
    # 移除冲突的编译参数
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isysroot ${SDK_PATH}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${SDK_PATH}")


elseif("${CROSS_TARGET}" STREQUAL "Android")
    # Android NDK自动配置
    set(ANDROID_NDK $ENV{ANDROID_NDK_ROOT} CACHE PATH "Android NDK path")
    
    # 工具链配置
    set(ANDROID_TOOLCHAIN_PREFIX "llvm")
    set(ANDROID_NATIVE_API_LEVEL 21 CACHE STRING "Android API level")
    include(${ANDROID_NDK}/build/cmake/android.toolchain.cmake)

    # NEON优化
    if(CROSS_ARCH MATCHES "arm")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon -mfloat-abi=softfp")
    endif()
endif()

# 通用查找规则
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
