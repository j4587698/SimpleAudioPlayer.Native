# Linux 交叉工具链
cmake_minimum_required(VERSION 3.20)

# 架构映射
set(LINUX_ARCH_MAP
    "arm=arm-linux-gnueabihf"
    "aarch64=aarch64-linux-gnu"
    "x86_64=x86_64-linux-gnu"
)

# 解析工具链前缀
foreach(PAIR IN LISTS LINUX_ARCH_MAP)
    string(REPLACE "=" ";" PAIR_LIST ${PAIR})
    list(GET PAIR_LIST 0 KEY)
    list(GET PAIR_LIST 1 VALUE)
    if("${CROSS_ARCH}" STREQUAL "${KEY}")
        set(LLVM_TARGET_TRIPLE "${VALUE}")
        break()
    endif()
endforeach()

set(CMAKE_SYSTEM_NAME Linux)

# 编译器路径
set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

# 通用编译选项
set(CMAKE_C_FLAGS "--target=${LLVM_TARGET_TRIPLE} -fPIC")
set(CMAKE_CXX_FLAGS "--target=${LLVM_TARGET_TRIPLE} -fPIC")
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld --target=${LLVM_TARGET_TRIPLE}")  # 使用 LLD 链接器

set(CMAKE_STRIP llvm-strip)

# ARM优化
if("${CROSS_ARCH}" STREQUAL "arm")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=hard -mfpu=neon-vfpv4")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)