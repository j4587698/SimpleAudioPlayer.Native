cmake_minimum_required(VERSION 3.20)

if(ANDROID)
    # 强制使用LLD链接器
    set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "" FORCE)
    set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "" FORCE)
endif()

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
option(ENABLE_LTO "Enable Link Time Optimization" OFF)
if(ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW) # 确保LTO跨编译器兼容
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)


#----------------------------------------------------------
# FFmpeg跨平台编译
#----------------------------------------------------------
set(FFMPEG_VERSION "7.1")
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
	--disable-vulkan
    --disable-everything
    --enable-demuxer=aac,ac3,aiff,ape,asf,au,flac,mov,mp3,mpc,ogg,wav,wv
    --enable-decoder=ape,flac,alac,wavpack,mp3,aac,pcm_s16le,pcm_s24le,pcm_s32le
    --enable-protocol=file,pipe
	--enable-parser=aac,aac_latm,flac,ape,mp3,alac,wavpack
    #--enable-filter=aformat,anull,atrim
    --disable-shared
    --enable-static
	--enable-pic
	--extra-cflags=-fPIC
	--enable-cross-compile
)

# 平台特定参数
if(WINDOWS)
    list(APPEND FFMPEG_BASE_ARGS
        --target-os=mingw32
        --cross-prefix=${TOOLCHAIN_PREFIX}-
        --arch=${CROSS_ARCH}
    )
elseif(ANDROID)
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
	set(ANDROID_NDK $ENV{ANDROID_NDK_ROOT})
	set(ANDROID_TOOLCHAIN_ROOT ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64)
    set(ANDROID_SYSROOT ${ANDROID_TOOLCHAIN_ROOT}/sysroot)
    list(APPEND FFMPEG_BASE_ARGS
        --target-os=android
        --arch=${CROSS_ARCH}
        --sysroot=${ANDROID_SYSROOT}
		--cc=${ANDROID_TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}${ANDROID_NATIVE_API_LEVEL}-clang
		--cxx=${ANDROID_TOOLCHAIN_ROOT}/${TOOLCHAIN_PREFIX}${ANDROID_NATIVE_API_LEVEL}-clang++
    )
	if(CMAKE_ANDROID_ARCH_ABI MATCHES "armeabi-v7a")
		list(APPEND FFMPEG_BASE_ARGS
			--enable-vfp
			--enable-neon
		)
	endif()
elseif(APPLE)
	list(APPEND FFMPEG_BASE_ARGS
			--sysroot=${CMAKE_OSX_SYSROOT}
			--target-os=darwin
			--arch=${APPLE_ARCH}
			--cc=${CMAKE_C_COMPILER}
			--cxx=${CMAKE_CXX_COMPILER}
			--ld=${CMAKE_C_COMPILER}
			--ar=${CMAKE_AR}
			--extra-cflags=${CMAKE_C_FLAGS}
			--extra-ldflags=${CMAKE_EXE_LINKER_FLAGS}
			--disable-videotoolbox
	)
elseif(LINUX)
	list(APPEND FFMPEG_BASE_ARGS
		--arch=${CROSS_ARCH}
		--cross-prefix=${LLVM_TARGET_TRIPLE}-
		--target-os=linux
		--cc=${CMAKE_C_COMPILER}
        			--cxx=${CMAKE_CXX_COMPILER}
        			--ld=${CMAKE_C_COMPILER}
        			--ar=${CMAKE_AR}
        			--extra-cflags=${CMAKE_C_FLAGS}
        			--extra-ldflags=${CMAKE_EXE_LINKER_FLAGS}
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
	GIT_SHALLOW       TRUE
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
if(WINDOWS)
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
	if (APPLE_PLATFORM STREQUAL "MACOS")
		target_link_libraries(audio_player PRIVATE
				"-framework CoreFoundation"
				"-framework CoreAudio"
				"-framework AudioToolbox"
				"-lpthread"
				"-lm"
		)
		set_target_properties(audio_player PROPERTIES
				# Enable code signing for macOS (ad-hoc signing)
				CODE_SIGNING_REQUIRED TRUE
				CODE_SIGNING_ALLOWED TRUE
				CODE_SIGN_IDENTITY "-"
		)
	else ()
		add_definitions(-DMA_NO_RUNTIME_LINKING)
		project(audio_player LANGUAGES C OBJC)
		# Use Objective-C for audio_player.c file.
		set_source_files_properties(src/audio_player.c PROPERTIES LANGUAGE OBJC)

		# Link with frameworks
		target_link_libraries(audio_player PRIVATE
				"-framework CoreFoundation"
				"-framework CoreAudio"
				"-framework AudioToolbox"
				"-framework AVFoundation"
				"-framework Foundation"
				"-lpthread"
				"-lm"
		)

	endif ()

elseif(ANDROID)
	add_link_options(
        "-fuse-ld=lld"
        "-B${ANDROID_TOOLCHAIN_ROOT}/bin"
    )
	if(ENABLE_LTO)
        # 确保这些路径存在
        message(STATUS "LLVM工具链路径: ${ANDROID_TOOLCHAIN_ROOT}/bin/")
        set(CMAKE_AR "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ar" CACHE FILEPATH "AR" FORCE)
        set(CMAKE_RANLIB "${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ranlib" CACHE FILEPATH "RANLIB" FORCE)
    endif()
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
	if (APPLE)
		add_custom_command(TARGET audio_player POST_BUILD
			COMMAND ${CMAKE_STRIP} -S -x $<TARGET_FILE:audio_player>
	)
	else ()
    	add_custom_command(TARGET audio_player POST_BUILD
        	COMMAND ${CMAKE_STRIP} -S --strip-unneeded $<TARGET_FILE:audio_player>
    )
	endif ()
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
