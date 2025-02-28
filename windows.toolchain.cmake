# MinGW 工具链
cmake_minimum_required(VERSION 3.20)


# 架构映射
set(WIN_ARCH_MAP
    "x86_64=x86_64-w64-mingw32"
)

# 查找工具链前缀
foreach(PAIR IN LISTS WIN_ARCH_MAP)
    string(REPLACE "=" ";" PAIR_LIST ${PAIR})
    list(GET PAIR_LIST 0 KEY)
    list(GET PAIR_LIST 1 VALUE)
    if("${CROSS_ARCH}" STREQUAL "${KEY}")
        set(TOOLCHAIN_PREFIX "${VALUE}")
        break()
    endif()
endforeach()

set(WINDOWS TRUE) 
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ${CROSS_ARCH})

# 编译器路径
find_program(MINGW_GCC ${TOOLCHAIN_PREFIX}-gcc)
find_program(MINGW_GXX ${TOOLCHAIN_PREFIX}-g++)

# Windows交叉编译配置
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# 优化标志
set(CMAKE_C_FLAGS_RELEASE "-Os -ffunction-sections -fdata-sections")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-Wl,--gc-sections,--exclude-libs=ALL")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)