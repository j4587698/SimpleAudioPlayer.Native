cmake_minimum_required(VERSION 3.20)

if(ANDROID)
    # 强制使用LLD链接器
    set(ANDROID_16KB_PAGE_LINKER_FLAG "-Wl,-z,max-page-size=16384")
    set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld ${ANDROID_16KB_PAGE_LINKER_FLAG}" CACHE STRING "" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld ${ANDROID_16KB_PAGE_LINKER_FLAG}" CACHE STRING "" FORCE)
    set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=lld ${ANDROID_16KB_PAGE_LINKER_FLAG}" CACHE STRING "" FORCE)
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
	--disable-autodetect
	--disable-vulkan
    --disable-everything
    --enable-demuxer=aac,ac3,aiff,ape,asf,au,flac,mov,mp3,mpc,ogg,wav,wv
	--enable-decoder=ape,flac,alac,wavpack,mp3,aac,vorbis,opus,pcm_s16le,pcm_s24le,pcm_s32le
    --enable-encoder=aac
    --enable-muxer=adts,ipod
    --enable-protocol=file,pipe
	--enable-parser=aac,aac_latm,flac,ape,mp3,alac,wavpack,vorbis,opus
    #--enable-filter=aformat,anull,atrim
    --disable-shared
    --enable-static
	--enable-pic
	--extra-cflags=-fPIC
	--extra-asflags=-fPIC
	--enable-cross-compile
)

# 平台特定参数
if(WINDOWS)
    list(APPEND FFMPEG_BASE_ARGS
        --target-os=mingw32
        --cross-prefix=${TOOLCHAIN_PREFIX}-
		--cc=${CMAKE_C_COMPILER}
		--cxx=${CMAKE_CXX_COMPILER}
		--ld=${CMAKE_C_COMPILER}
		--ar=${CMAKE_AR}
		--ranlib=${CMAKE_RANLIB}
		--nm=${CMAKE_NM}
		--strip=${CMAKE_STRIP}
		--windres=${CMAKE_RC_COMPILER}
        --arch=${CROSS_ARCH}
    )
elseif(ANDROID)
	# 架构映射表（NDK ABI -> 工具链前缀）
	set(ANDROID_ARCH_MAP
		"armeabi-v7a=armv7a-linux-androideabi"
		"arm64-v8a=aarch64-linux-android"
		"x86_64=x86_64-linux-android"
		"x86=i686-linux-android"
	)
	
	# FFmpeg --arch 映射（NDK ABI 名与 FFmpeg 架构名不同）
	set(FFMPEG_ARCH_MAP
		"armeabi-v7a=arm"
		"arm64-v8a=aarch64"
		"x86_64=x86_64"
		"x86=i686"
	)
	
	# 解析工具链前缀
	foreach(PAIR IN LISTS ANDROID_ARCH_MAP)
		string(REPLACE "=" ";" PAIR_LIST ${PAIR})
		list(GET PAIR_LIST 0 KEY)
		list(GET PAIR_LIST 1 VALUE)
		if("${ANDROID_ABI}" STREQUAL "${KEY}")
			set(TOOLCHAIN_PREFIX "${VALUE}")
			break()
		endif()
	endforeach()

	# 解析 FFmpeg 架构名
	set(FFMPEG_ARCH "")
	foreach(PAIR IN LISTS FFMPEG_ARCH_MAP)
		string(REPLACE "=" ";" PAIR_LIST ${PAIR})
		list(GET PAIR_LIST 0 KEY)
		list(GET PAIR_LIST 1 VALUE)
		if("${ANDROID_ABI}" STREQUAL "${KEY}")
			set(FFMPEG_ARCH "${VALUE}")
			break()
		endif()
	endforeach()

	set(ANDROID_NDK $ENV{ANDROID_NDK_ROOT})
	set(ANDROID_TOOLCHAIN_ROOT ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64)
    set(ANDROID_SYSROOT ${ANDROID_TOOLCHAIN_ROOT}/sysroot)
    list(APPEND FFMPEG_BASE_ARGS
        --target-os=android
        --arch=${FFMPEG_ARCH}
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
        --ar=${CMAKE_AR}
        --ld=${CMAKE_C_COMPILER}
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

#----------------------------------------------------------
# 汇编优化探测（asm / x86asm）
#
# ffmpeg 有两个独立开关：
#   --disable-asm    : 关闭全部平台汇编（含 ARM NEON），代价大
#   --disable-x86asm : 仅关闭依赖外部汇编器（nasm）的 x86 SIMD
#
# 策略：
#   ARM / ARM64 : 汇编走编译器内置汇编器，无外部依赖，始终开启
#   x86_64      : 需要 nasm >= 2.13，探测到才开启，否则自动降级
#   x86 (32 位) : --enable-pic 与 i386 汇编的 ebx 分配冲突，且在 Android
#                 上会产生 TEXTREL 导致 .so 被拒绝，一律关闭 x86asm
#----------------------------------------------------------
option(ENABLE_FFMPEG_ASM "Enable ffmpeg assembly optimizations when the toolchain allows it" ON)

# 归一化目标架构（各平台传入的变量名不同）
if(ANDROID)
	set(SAP_TARGET_ARCH "${ANDROID_ABI}")
elseif(APPLE)
	set(SAP_TARGET_ARCH "${APPLE_ARCH}")
else()
	set(SAP_TARGET_ARCH "${CROSS_ARCH}")
endif()
string(TOLOWER "${SAP_TARGET_ARCH}" SAP_TARGET_ARCH_LOWER)

set(SAP_ARCH_IS_X86_64 FALSE)
set(SAP_ARCH_IS_X86_32 FALSE)
if(SAP_TARGET_ARCH_LOWER MATCHES "^(x86_64|x64|amd64)$")
	set(SAP_ARCH_IS_X86_64 TRUE)
elseif(SAP_TARGET_ARCH_LOWER MATCHES "^(x86|i386|i686)$")
	set(SAP_ARCH_IS_X86_32 TRUE)
endif()

# 仅 x86_64 需要外部汇编器；探测失败时不报错，走降级分支
set(SAP_HAVE_USABLE_NASM FALSE)
if(ENABLE_FFMPEG_ASM AND SAP_ARCH_IS_X86_64)
	find_program(SAP_NASM_EXECUTABLE NAMES nasm)
	if(SAP_NASM_EXECUTABLE)
		execute_process(
			COMMAND "${SAP_NASM_EXECUTABLE}" -v
			RESULT_VARIABLE SAP_NASM_PROBE_RESULT
			OUTPUT_VARIABLE SAP_NASM_PROBE_OUTPUT
			ERROR_VARIABLE SAP_NASM_PROBE_OUTPUT
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		if(SAP_NASM_PROBE_RESULT EQUAL 0 AND SAP_NASM_PROBE_OUTPUT MATCHES "([0-9]+)\\.([0-9]+)")
			set(SAP_NASM_MAJOR "${CMAKE_MATCH_1}")
			set(SAP_NASM_MINOR "${CMAKE_MATCH_2}")
			if(SAP_NASM_MAJOR GREATER 2
				OR (SAP_NASM_MAJOR EQUAL 2 AND SAP_NASM_MINOR GREATER_EQUAL 13))
				set(SAP_HAVE_USABLE_NASM TRUE)
			else()
				message(WARNING "[asm] nasm ${SAP_NASM_MAJOR}.${SAP_NASM_MINOR} 过旧（需要 >= 2.13），将关闭 x86asm")
			endif()
		endif()
	endif()
endif()

if(NOT ENABLE_FFMPEG_ASM)
	list(APPEND FFMPEG_BASE_ARGS --disable-asm)
	set(SAP_ASM_SUMMARY "disabled (ENABLE_FFMPEG_ASM=OFF)")
elseif(SAP_ARCH_IS_X86_32)
	list(APPEND FFMPEG_BASE_ARGS --disable-x86asm)
	set(SAP_ASM_SUMMARY "x86asm disabled (32-bit x86 与 PIC 不兼容)")
elseif(SAP_ARCH_IS_X86_64)
	if(SAP_HAVE_USABLE_NASM)
		list(APPEND FFMPEG_BASE_ARGS --enable-x86asm --x86asmexe=${SAP_NASM_EXECUTABLE})
		set(SAP_ASM_SUMMARY "x86asm enabled via ${SAP_NASM_EXECUTABLE}")
	else()
		list(APPEND FFMPEG_BASE_ARGS --disable-x86asm)
		set(SAP_ASM_SUMMARY "x86asm disabled (未找到可用的 nasm，已自动降级)")
	endif()
else()
	# ARM / ARM64 等：使用 ffmpeg 默认的 --enable-asm，无需外部工具
	set(SAP_ASM_SUMMARY "asm enabled (compiler built-in assembler)")
endif()

message(STATUS "[asm] target arch = '${SAP_TARGET_ARCH}', ${SAP_ASM_SUMMARY}")

ExternalProject_Add(ffmpeg
    GIT_REPOSITORY "https://github.com/FFmpeg/FFmpeg.git"
    GIT_TAG "n${FFMPEG_VERSION}"
	GIT_SHALLOW       TRUE
    PREFIX ${CMAKE_BINARY_DIR}/ffmpeg
    CONFIGURE_COMMAND <SOURCE_DIR>/configure ${FFMPEG_BASE_ARGS}
	INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install-libs install-headers
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
    src/audio_recorder.h
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
		-Wl,-Bstatic
		winpthread
		-Wl,-Bdynamic
		ole32 ws2_32 secur32 bcrypt
    )
    target_link_options(audio_player PRIVATE
		-static-libgcc
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
	if(NOT DEFINED ANDROID_16KB_PAGE_LINKER_FLAG)
		set(ANDROID_16KB_PAGE_LINKER_FLAG "-Wl,-z,max-page-size=16384")
	endif()
	target_link_options(audio_player PRIVATE
        "-fuse-ld=lld"
        "-B${ANDROID_TOOLCHAIN_ROOT}/bin"
        "${ANDROID_16KB_PAGE_LINKER_FLAG}"
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
	AUDIO_PLAYER_BUILD
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

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
	install(DIRECTORY include/ DESTINATION include)
endif()
