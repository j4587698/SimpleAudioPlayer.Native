cmake_minimum_required(VERSION 3.20)
project(audio_player)

#----------------------------------------------------------
# 全局配置
#----------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# LTO优化
option(ENABLE_LTO "Enable Link Time Optimization" ON)
if(ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    add_compile_options(-flto=auto)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto=auto")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#----------------------------------------------------------
# FFmpeg跨平台编译
#----------------------------------------------------------
set(FFMPEG_VERSION "6.1")
set(FFMPEG_INSTALL_DIR "${CMAKE_BINARY_DIR}/ffmpeg_install")

include(ExternalProject)

# 基础配置参数
set(FFMPEG_BASE_ARGS
    --prefix=${FFMPEG_INSTALL_DIR}
    --disable-doc
    --disable-programs
    --disable-avdevice
    --disable-swscale
    --disable-postproc
    --disable-avfilter
    --disable-network
    --disable-iconv
    --disable-sdl2
    --disable-zlib
    --disable-bzlib
    --disable-lzma
	--disable-asm
    --disable-everything
    --enable-demuxer=aac,ac3,aiff,ape,asf,au,flac,mov,mp3,mpc,ogg,wav,wv
    --enable-decoder=ape,flac,alac,wavpack,mp3,aac,pcm_s16le,pcm_s24le,pcm_s32le
    --enable-protocol=file,pipe
    #--enable-filter=aformat,anull,atrim
    --disable-shared
    --enable-static
	--enable-pic
	--extra-cflags=-fPIC
	--extra-ldflags=-fPIC
	--enable-cross-compile
)


# 平台特定参数
if(WIN32)
    list(APPEND FFMPEG_BASE_ARGS
        --target-os=mingw32
        --cross-prefix=${TOOLCHAIN_PREFIX}-
        --arch=${CROSS_ARCH}
    )
elseif(ANDROID)
    list(APPEND FFMPEG_BASE_ARGS
        --target-os=android
        --arch=${CROSS_ARCH}
        --sysroot=${ANDROID_NDK}/sysroot
    )
	if(CMAKE_ANDROID_ARCH_ABI MATCHES "armeabi-v7a")
		list(APPEND FFMPEG_BASE_ARGS
			--enable-vfp
			--enable-neon
		)
	endif()
elseif(APPLE)
    list(APPEND FFMPEG_BASE_ARGS
        --target-os=darwin
        --sysroot=${CMAKE_OSX_SYSROOT}
    )
    if(IOS)
        list(APPEND FFMPEG_BASE_ARGS --disable-asm)
    endif()
elseif(LINUX)
	list(APPEND FFMPEG_BASE_ARGS
		--arch=${CROSS_ARCH}
		--cross-prefix=${TOOLCHAIN_PREFIX}-
		--target-os=linux
	)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
		list(APPEND FFMPEG_BASE_ARGS
			--enable-vfp
			--enable-neon
			--extra-cflags=-mfloat-abi=hard
			
		)
	endif()
endif()

ExternalProject_Add(ffmpeg
    GIT_REPOSITORY "https://github.com/FFmpeg/FFmpeg.git"
    GIT_TAG "n${FFMPEG_VERSION}"
    PREFIX ${CMAKE_BINARY_DIR}/ffmpeg
    CONFIGURE_COMMAND <SOURCE_DIR>/configure ${FFMPEG_BASE_ARGS}
    BUILD_BYPRODUCTS
        ${FFMPEG_INSTALL_DIR}/lib/libavcodec.a
        ${FFMPEG_INSTALL_DIR}/lib/libavformat.a
        ${FFMPEG_INSTALL_DIR}/lib/libswresample.a
        ${FFMPEG_INSTALL_DIR}/lib/libavutil.a
)

#----------------------------------------------------------
# 主库配置
#----------------------------------------------------------
add_library(audio_player SHARED
    src/audio_player.c
    src/audio_player.h
    src/miniaudio_ffmpeg.h
    src/miniaudio.h
)

target_include_directories(audio_player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INSTALL_DIR}/include
)

target_link_libraries(audio_player PRIVATE
    ${FFMPEG_INSTALL_DIR}/lib/libavformat.a
    ${FFMPEG_INSTALL_DIR}/lib/libavcodec.a    
    ${FFMPEG_INSTALL_DIR}/lib/libswresample.a
    ${FFMPEG_INSTALL_DIR}/lib/libavutil.a
)

# 平台特定链接
if(WIN32)
    target_link_libraries(audio_player PRIVATE
        ole32 ws2_32 secur32 bcrypt
    )
    target_link_options(audio_player PRIVATE
        -Wl,--kill-at
        -Wl,--output-def=${CMAKE_BINARY_DIR}/audio_player.def
    )
elseif(LINUX)
	target_link_libraries(audio_player PRIVATE
        dl
        pthread
        m
    )
elseif(APPLE)
    find_library(AUDIO_TOOLBOX AudioToolbox)
    find_library(CORE_FOUNDATION CoreFoundation)
    target_link_libraries(audio_player PRIVATE
        ${AUDIO_TOOLBOX} ${CORE_FOUNDATION}
    )
elseif(ANDROID)
    target_link_libraries(audio_player PRIVATE log)
endif()

# 通用优化
target_compile_definitions(audio_player PRIVATE
    MA_NO_WAV
    MA_NO_MP3
    MA_NO_FLAC
)

# 符号精简
if(NOT MSVC)
    add_custom_command(TARGET audio_player POST_BUILD
        COMMAND ${CMAKE_STRIP} -S --strip-unneeded $<TARGET_FILE:audio_player>
    )
endif()

add_dependencies(audio_player ffmpeg)

#----------------------------------------------------------
# 安装规则
#----------------------------------------------------------
install(TARGETS audio_player
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
