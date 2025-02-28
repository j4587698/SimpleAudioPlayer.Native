# Android NDK 工具链
cmake_minimum_required(VERSION 3.20)


# 架构映射表
set(ANDROID_ARCH_MAP
    "armeabi-v7a=armv7a-linux-androideabi"
    "arm64-v8a=aarch64-linux-android"
    "x86_64=x86_64-linux-android"
	"x86=i686-linux-android"
)

# 解析架构参数
foreach(PAIR IN LISTS ANDROID_ARCH_MAP)
    string(REPLACE "=" ";" PAIR_LIST ${PAIR})
    list(GET PAIR_LIST 0 KEY)
    list(GET PAIR_LIST 1 VALUE)
    if("${ANDROID_ABI}" STREQUAL "${KEY}")
        set(TOOLCHAIN_PREFIX "${VALUE}")
        break()
    endif()
endforeach()

set(ANDROID_TOOLCHAIN clang)

# NDK工具链路径
set(ANDROID_NDK $ENV{ANDROID_NDK_ROOT})
set(ANDROID_TOOLCHAIN_ROOT ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64)

set(ANDROID_TARGET_TRIPLE "${TOOLCHAIN_PREFIX}${ANDROID_NATIVE_API_LEVEL}")

# 编译器配置
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION ${ANDROID_NATIVE_API_LEVEL})  # 最低API级别
set(CMAKE_ANDROID_ARCH_ABI ${ANDROID_ABI})

# 动态生成编译器路径
set(CLANG_ROOT ${ANDROID_TOOLCHAIN_ROOT}/bin)
set(CMAKE_C_COMPILER ${CLANG_ROOT}/${TOOLCHAIN_PREFIX}${ANDROID_NATIVE_API_LEVEL}-clang)
set(CMAKE_CXX_COMPILER ${CLANG_ROOT}/${TOOLCHAIN_PREFIX}${ANDROID_NATIVE_API_LEVEL}-clang++)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})

set(CMAKE_C_FLAGS "--target=${ANDROID_TARGET_TRIPLE} -fpic")
set(CMAKE_CXX_FLAGS "--target=${ANDROID_TARGET_TRIPLE} -fpic")

# 架构特定优化
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon -mfloat-abi=softfp")
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+crc")
endif()

# 系统根目录
set(CMAKE_SYSROOT ${ANDROID_TOOLCHAIN_ROOT}/sysroot)

set(CMAKE_ANDROID_LINKER lld)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
