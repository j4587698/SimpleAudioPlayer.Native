#!/bin/bash
set -e

# 参数检查
if [ $# -lt 2 ]; then
    echo "Usage: $0 <platform> <arch> [ffmpeg_version]"
    exit 1
fi

PLATFORM=$1
ARCH=$2
FFMPEG_VERSION=7.1
PREFIX=$(pwd)/output/$PLATFORM/$ARCH

# 清理并创建目录
rm -rf ffmpeg $PREFIX
git clone --depth 1 --branch $FFMPEG_VERSION https://github.com/FFmpeg/FFmpeg.git
mkdir -p $PREFIX

# 基础配置参数
CONFIG_FLAGS=(
    --disable-shared
    --enable-static
    --enable-pic
    --disable-doc
    --disable-debug
    --disable-avdevice
    --disable-swscale
    --disable-programs
    --disable-network
    --disable-muxers
    --disable-zlib
    --disable-lzma
    --disable-bzlib
    --disable-iconv
    --disable-libxcb
    --disable-bsfs
    --disable-filters
    --disable-indevs
    --disable-outdevs
    --disable-encoders
    --disable-decoders
    --disable-hwaccels
    --disable-nvenc
    --disable-videotoolbox
    --disable-audiotoolbox
    --enable-filter=aformat
    --enable-filter=anull
    --enable-filter=atrim
    --enable-filter=format
    --enable-filter=null
    --enable-filter=setpts
    --enable-filter=trim
    --enable-protocol=file
    --enable-protocol=pipe
	
	
    --enable-demuxer=aac
	--enable-demuxer=ac3
	--enable-demuxer=aiff
	--enable-demuxer=ape
	--enable-demuxer=asf
	--enable-demuxer=au
	--enable-demuxer=flac
	--enable-demuxer=mov
	--enable-demuxer=mp3
	--enable-demuxer=mpc
	--enable-demuxer=mpc8
	--enable-demuxer=ogg
	--enable-demuxer=pcm_alaw
	--enable-demuxer=pcm_mulaw
	--enable-demuxer=pcm_s32be
	--enable-demuxer=pcm_s32le
	--enable-demuxer=pcm_s24be
	--enable-demuxer=pcm_s24le
	--enable-demuxer=pcm_s16be
	--enable-demuxer=pcm_s16le
	--enable-demuxer=pcm_s8
	--enable-demuxer=rm
	--enable-demuxer=wav
	--enable-demuxer=wv
	--enable-demuxer=xwma
	--enable-demuxer=dsf
	
	# 新增关键demuxers（根据解码器需求）
	--enable-demuxer=apc       # APE专用封装
	--enable-demuxer=dff       # DSDIFF格式
	--enable-demuxer=latm      # AAC LATM封装
	--enable-demuxer=caf       # Apple Core Audio
	--enable-demuxer=mp4       # 标准MP4容器
	--enable-demuxer=nut       # 多轨工程文件
	--enable-demuxer=film_cpk  # 游戏封包音频
    
	
	    # 无损音频组
    --enable-decoder=ape       # Monkey's Audio (APE)
    --enable-decoder=flac      # FLAC无损
    --enable-decoder=alac      # Apple无损
    --enable-decoder=wavpack   # WavPack混合无损

    # DSD支持
    --enable-decoder=dsd_lsbf  # DSD Least Significant Bit First
    --enable-decoder=dsd_msbf  # DSD Most Significant Bit First

    # 有损压缩组
    --enable-decoder=mp3       # MP3标准支持
    --enable-decoder=aac       # AAC-LC/HE
    --enable-decoder=aac_latm  # AAC LATM格式
    --enable-decoder=opus      # Opus互联网音频
    --enable-decoder=vorbis    # Ogg Vorbis

    # PCM基础支持
    --enable-decoder=pcm_s16le # 16-bit小端 (最常用)
    --enable-decoder=pcm_s24le # 24-bit小端 (Hi-Res)
    --enable-decoder=pcm_s32le # 32-bit小端 (专业级)
    --enable-decoder=pcm_f32le # 32-bit浮点
    --enable-decoder=pcm_s16be # 大端字节序支持
    --enable-decoder=pcm_s24be
    --enable-decoder=pcm_s32be

    # 8-bit支持
    --enable-decoder=pcm_u8    # 无符号8-bit
    --enable-decoder=pcm_s8    # 有符号8-bit

    # 容器依赖
    --enable-decoder=pcm_alaw  # G.711 A-law (WAV常用)
    --enable-decoder=pcm_mulaw # G.711 μ-law
    --enable-decoder=pcm_bluray # 蓝光音频提取
    --enable-decoder=pcm_dvd    # DVD音频提取
    --prefix=$PREFIX
)

# 平台特定配置
case $PLATFORM in
    android)
        export NDK=/usr/local/android-ndk
        TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/linux-x86_64
        CONFIG_FLAGS+=(
            --target-os=android
            --enable-cross-compile
            --sysroot=$TOOLCHAIN/sysroot
            --cc=$TOOLCHAIN/bin/$ARCH-clang
            --ar=$TOOLCHAIN/bin/llvm-ar
        )
        ;;
    ios)
        export MIN_VERSION=12.0
        CONFIG_FLAGS+=(
            --target-os=darwin
            --enable-cross-compile
            --arch=$ARCH
            --extra-cflags="-arch $ARCH -miphoneos-version-min=$MIN_VERSION"
            --extra-ldflags="-arch $ARCH -miphoneos-version-min=$MIN_VERSION"
        )
        ;;
    linux|macos)
        CONFIG_FLAGS+=(--target-os=${PLATFORM%64})
        ;;
    windows)
        CONFIG_FLAGS+=(
            --target-os=mingw32
            --cross-prefix=x86_64-w64-mingw32-
            --arch=$ARCH
        )
        ;;
esac

# 进入源码目录执行配置和编译
cd FFmpeg
./configure "${CONFIG_FLAGS[@]}"
make -j$(nproc)
make install
