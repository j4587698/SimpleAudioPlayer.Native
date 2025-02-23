cmake_minimum_required(VERSION 3.20)
project(audio_player)

#-----------------------------------------------------------------------
# 交叉编译工具链配置
#-----------------------------------------------------------------------
#set(CMAKE_SYSTEM_NAME Windows)
#set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# 查找MinGW工具链
find_program(MINGW_CC ${TOOLCHAIN_PREFIX}-gcc REQUIRED)
find_program(MINGW_CXX ${TOOLCHAIN_PREFIX}-g++ REQUIRED)
find_program(MINGW_AR ${TOOLCHAIN_PREFIX}-ar REQUIRED)
find_program(MINGW_RANLIB ${TOOLCHAIN_PREFIX}-ranlib REQUIRED)

set(CMAKE_C_COMPILER ${MINGW_CC})
set(CMAKE_CXX_COMPILER ${MINGW_CXX})
set(CMAKE_AR ${MINGW_AR})
set(CMAKE_RANLIB ${MINGW_RANLIB})

# 系统根目录配置
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

#-----------------------------------------------------------------------
# 项目结构配置
#-----------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 输出目录设置
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#-----------------------------------------------------------------------
# FFmpeg 交叉编译配置
#-----------------------------------------------------------------------
set(FFMPEG_VERSION "7.1")
set(FFMPEG_INSTALL_DIR "${CMAKE_BINARY_DIR}/ffmpeg_install")

include(ExternalProject)

# 核心解码器配置
set(FFMPEG_BASE_ARGS
    --prefix=${FFMPEG_INSTALL_DIR}
    --enable-pic
    --disable-doc
    --disable-programs
    --disable-avdevice
    --disable-swscale
    --disable-postproc
    --disable-avfilter
    --disable-network
    --disable-everything
    --enable-demuxer=aac,ac3,aiff,ape,asf,au,flac,mov,mp3,mpc,ogg,wav,wv
    --enable-decoder=ape,flac,alac,wavpack,mp3,aac,pcm_s16le,pcm_s24le,pcm_s32le
    --enable-protocol=file,pipe
    --enable-filter=aformat,anull,atrim
    --disable-shared
    --enable-static
    --target-os=mingw32
    --cross-prefix=${TOOLCHAIN_PREFIX}-
    --arch=x86_64
    --extra-cflags="-static"
    --extra-ldflags="-static"
)

ExternalProject_Add(ffmpeg
    GIT_REPOSITORY "https://github.com/FFmpeg/FFmpeg.git"
    GIT_TAG "n${FFMPEG_VERSION}"
    PREFIX ${CMAKE_BINARY_DIR}/ffmpeg
    CONFIGURE_COMMAND <SOURCE_DIR>/configure ${FFMPEG_BASE_ARGS}
    BUILD_BYPRODUCTS
        ${FFMPEG_INSTALL_DIR}/lib/libavcodec.a
        ${FFMPEG_INSTALL_DIR}/lib/libavformat.a
        ${FFMPEG_INSTALL_DIR}/lib/libavutil.a
)

#-----------------------------------------------------------------------
# 主音频库配置
#-----------------------------------------------------------------------
add_library(audio_player SHARED
    src/audio_player.c
    src/audio_player.h
    src/miniaudio_ffmpeg.h
)

target_include_directories(audio_player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INSTALL_DIR}/include
)

# 静态链接配置
target_link_libraries(audio_player PRIVATE
    ${FFMPEG_INSTALL_DIR}/lib/libavcodec.a
    ${FFMPEG_INSTALL_DIR}/lib/libavformat.a
    ${FFMPEG_INSTALL_DIR}/lib/libavutil.a
)

# Windows系统库链接
target_link_libraries(audio_player PRIVATE
    ole32
    ws2_32
    secur32
    bcrypt
)

# 静态链接选项
target_link_options(audio_player PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
    -Wl,--enable-auto-import
)

# 符号导出控制
set_target_properties(audio_player PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# 依赖管理
add_dependencies(audio_player ffmpeg)

#-----------------------------------------------------------------------
# 安装规则
#-----------------------------------------------------------------------
install(TARGETS audio_player
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
