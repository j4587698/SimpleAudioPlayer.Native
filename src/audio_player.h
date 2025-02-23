#define AUDIO_EXPORTS
#define MA_IMPLEMENTATION
#include "audio_player.h"
#include "miniaudio.h"
#include "miniaudio_ffmpeg.h"
#include <stdlib.h>
#include <string.h>

// 自定义后端实现
static ma_result ma_decoding_backend_init__ffmpeg(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_ffmpeg* pFFmpeg;

    (void)pUserData;

    pFFmpeg = (ma_ffmpeg*)ma_malloc(sizeof(*pFFmpeg), pAllocationCallbacks);
    if (pFFmpeg == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_ffmpeg_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pFFmpeg);
    if (result != MA_SUCCESS) {
        ma_free(pFFmpeg, pAllocationCallbacks);
        return result;
    }

    *ppBackend = pFFmpeg;
    return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__ffmpeg(void* pUserData, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks)
{
    ma_ffmpeg* pFFmpeg = (ma_ffmpeg*)pBackend;
    ma_ffmpeg_uninit(pFFmpeg, pAllocationCallbacks);
    ma_free(pFFmpeg, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_ffmpeg = {
    ma_decoding_backend_init__ffmpeg,
    NULL, NULL, NULL,
    ma_decoding_backend_uninit__ffmpeg
};

// 播放器状态结构
typedef struct {
    ma_decoder decoder;
    ma_device device;
    ma_bool8 is_playing;
} AudioContext;

// 数据回调函数
static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    AudioContext* ctx = (AudioContext*)pDevice->pUserData;
    if (ctx && ctx->is_playing) {
        ma_data_source_read_pcm_frames(&ctx->decoder, pOutput, frameCount, NULL);
    }
    (void)pInput;
}

AUDIO_API AudioError audio_init(const char* filePath, AudioHandle* handle)
{
    if (!filePath || !handle) return AUDIO_ERROR_INVALID_FILE;

    AudioContext* ctx = (AudioContext*)calloc(1, sizeof(AudioContext));
    if (!ctx) return AUDIO_ERROR_INIT_DECODER;

    // 配置自定义后端
    ma_decoding_backend_vtable* backends[] = { &g_ma_decoding_backend_vtable_ffmpeg };
    
    ma_decoder_config decoderConfig = ma_decoder_config_init_default();
    decoderConfig.ppCustomBackendVTables = backends;
    decoderConfig.customBackendCount = 1;

    // 初始化解码器
    ma_result result = ma_decoder_init_file(filePath, &decoderConfig, &ctx->decoder);
    if (result != MA_SUCCESS) {
        free(ctx);
        return AUDIO_ERROR_INIT_DECODER;
    }

    // 获取音频格式信息
    ma_format format;
    ma_uint32 channels, sampleRate;
    ma_data_source_get_data_format(&ctx->decoder, &format, &channels, &sampleRate, NULL, 0);

    // 配置音频设备
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = format;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate        = sampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = ctx;

    result = ma_device_init(NULL, &deviceConfig, &ctx->device);
    if (result != MA_SUCCESS) {
        ma_decoder_uninit(&ctx->decoder);
        free(ctx);
        return AUDIO_ERROR_INIT_DEVICE;
    }

    ctx->is_playing = MA_FALSE;
    *handle = ctx;
    return AUDIO_SUCCESS;
}

AUDIO_API void audio_play(AudioHandle handle)
{
    if (!handle) return;

    AudioContext* ctx = (AudioContext*)handle;
    if (ma_device_start(&ctx->device) == MA_SUCCESS) {
        ctx->is_playing = MA_TRUE;
    }
}

AUDIO_API void audio_stop(AudioHandle handle)
{
    if (!handle) return;

    AudioContext* ctx = (AudioContext*)handle;
    ma_device_stop(&ctx->device);
    ctx->is_playing = MA_FALSE;
}

AUDIO_API void audio_cleanup(AudioHandle handle)
{
    if (!handle) return;

    AudioContext* ctx = (AudioContext*)handle;
    ma_device_uninit(&ctx->device);
    ma_decoder_uninit(&ctx->decoder);
    free(ctx);
}

