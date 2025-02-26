# cross_toolchain.cmake

# 基础配置
cmake_minimum_required(VERSION 3.20)
set(CMAKE_SYSTEM_NAME ${CROSS_TARGET})
set(CMAKE_SYSTEM_PROCESSOR ${CROSS_ARCH})

# 架构映射表
set(ARCH_MAP
    "armv7=arm-linux-gnueabihf"
    "arm64=aarch64-linux-gnu"
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

elseif("${CROSS_TARGET}" STREQUAL "Darwin")
    # macOS/iOS通用配置
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS version")
    set(CMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH NO)

    # iOS特殊配置
    if(IOS)
        set(CMAKE_SYSTEM_NAME iOS)
        set(CMAKE_OSX_SYSROOT iphoneos)
        set(CMAKE_OSX_ARCHITECTURES "arm64")
    else()
        set(CMAKE_OSX_SYSROOT auto)
    endif()

elseif("${CROSS_TARGET}" STREQUAL "Android")
    # Android NDK自动配置
    set(ANDROID_NDK $ENV{ANDROID_NDK_ROOT} CACHE PATH "Android NDK path")
    set(ANDROID_ABI_MAP
        "armv7=armeabi-v7a"
        "arm64=arm64-v8a"
        "x86_64=x86_64"
    )
    
    # 自动匹配ABI
    list(FIND ANDROID_ABI_MAP "${CROSS_ARCH}" ABI_INDEX)
    math(EXPR PAIR_INDEX "${ABI_INDEX} + 1")
    list(GET ANDROID_ABI_MAP ${PAIR_INDEX} ANDROID_ABI)
    
    # 工具链配置
    set(ANDROID_TOOLCHAIN_PREFIX "llvm")
    set(ANDROID_NATIVE_API_LEVEL 24 CACHE STRING "Android API level")
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
