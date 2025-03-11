#define MA_IMPLEMENTATION
#include "audio_player.h"
#include "miniaudio.h"
#include "miniaudio_ffmpeg.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


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
struct AudioContext{
    ma_decoder decoder;
    ma_device device;
	ma_mutex mutex;
    bool device_initialized;
} ;

// 数据回调函数
static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    AudioContext* ctx = (AudioContext*)pDevice->pUserData;
    if (ctx) {
		ma_mutex_lock(&ctx->mutex);
        ma_data_source_read_pcm_frames(&ctx->decoder, pOutput, frameCount, NULL);
		ma_mutex_unlock(&ctx->mutex);
    }
    (void)pInput;
}

ma_result ma_decoder_init_with_tell(ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek, ma_decoder_tell_proc onTell, void* pUserData, const ma_decoder_config* pConfig, ma_decoder* pDecoder)
{
	ma_decoder_config config;
	ma_result result;

	config = ma_decoder_config_init_copy(pConfig);

	result = ma_decoder__preinit(onRead, onSeek, onTell, pUserData, &config, pDecoder);
	if (result != MA_SUCCESS) {
		return result;
	}

	return ma_decoder_init__internal(onRead, onSeek, pUserData, &config, pDecoder);
}

MA_API AudioContext* audio_context_create(){
	AudioContext* ctx = (AudioContext*)ma_malloc(sizeof(AudioContext), NULL);
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(AudioContext));
    ma_mutex_init(&ctx->mutex);
    return ctx;
}

MA_API ma_result audio_init_device(AudioContext* ctx, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate)
{
    if (!ctx) return MA_INVALID_ARGS;
	if (ctx->device_initialized) return MA_SUCCESS;

    // 配置音频设备
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = format;
    deviceConfig.playback.channels = channels;
    deviceConfig.sampleRate        = sampleRate;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = ctx;

    ma_result result = ma_device_init(NULL, &deviceConfig, &ctx->device);
    if (result != MA_SUCCESS) {
        return MA_ERROR;
    }

    ctx->device_initialized = true;
    return MA_SUCCESS;
}



MA_API ma_result audio_init_decoder(AudioContext* ctx, ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek, ma_decoder_tell_proc onTell, void* userdata)
{
    if (!ctx) return MA_INVALID_ARGS;

	ma_mutex_lock(&ctx->mutex);
	if (ctx->decoder.pBackend != NULL){
		ma_decoder_uninit(&ctx->decoder);
	}

    // 配置自定义后端
    const ma_decoding_backend_vtable* backends[] = { &g_ma_decoding_backend_vtable_ffmpeg };
    
    ma_decoder_config decoderConfig = ma_decoder_config_init(ctx->device.playback.format, ctx->device.playback.channels, ctx->device.sampleRate);
    decoderConfig.ppCustomBackendVTables = backends;
    decoderConfig.customBackendCount = 1;
	
    // 初始化解码器
    ma_result result = ma_decoder_init_with_tell(onRead, onSeek, onTell, userdata, &decoderConfig, &ctx->decoder);
	
	ma_uint64 totalFrames;
	
	ma_mutex_unlock(&ctx->mutex);
	
    return result;
}

MA_API ma_result audio_play(AudioContext* ctx)
{
    if (!ctx) return MA_INVALID_ARGS;

    if (ma_device_start(&ctx->device) == MA_SUCCESS) {
        return MA_SUCCESS;
    }
	return MA_ERROR;
}

MA_API ma_result audio_stop(AudioContext* ctx)
{
    if (!ctx) return MA_INVALID_ARGS;
	
    ma_device_stop(&ctx->device);
    return MA_SUCCESS;
}

MA_API void audio_cleanup(AudioContext* ctx)
{
    if (!ctx) return;
	
	ma_mutex_lock(&ctx->mutex);
    ma_device_uninit(&ctx->device);
    ma_decoder_uninit(&ctx->decoder);
	
	ma_mutex_unlock(&ctx->mutex);
	ma_mutex_uninit(&ctx->mutex);
    ma_free(ctx, NULL);
}

MA_API ma_result seek_to_time(AudioContext* ctx, const double timeInSec) {
    if (timeInSec < 0) {
        return MA_INVALID_ARGS;
    }
	
    ma_uint64 target_frame = (ma_uint64)(timeInSec * ctx->decoder.outputSampleRate);
    return ma_decoder_seek_to_pcm_frame(&ctx->decoder, target_frame);
}

MA_API ma_result get_decoder(AudioContext* ctx, ma_decoder *decoder) {
	if (ctx == NULL || decoder == NULL){
		return MA_INVALID_ARGS;
	}
	
	ma_mutex_lock(&ctx->mutex);
    decoder = &ctx->decoder;
    ma_mutex_unlock(&ctx->mutex);
	
	return MA_SUCCESS;
}

MA_API ma_result get_length_in_pcm_frames(AudioContext* ctx, ma_int64* frames){
	if (ctx == NULL){
		return MA_INVALID_ARGS;
	}
	
	return ma_decoder_get_length_in_pcm_frames(&ctx->decoder, frames);
}

MA_API ma_result get_cursor_in_pcm_frames(AudioContext* ctx, ma_int64* frames){
	if (ctx == NULL){
		return MA_INVALID_ARGS;
	}
	
	return ma_decoder_get_cursor_in_pcm_frames(&ctx->decoder, frames);
}

MA_API ma_result get_time(AudioContext* ctx, double* time){
	if (ctx == NULL){
		return MA_INVALID_ARGS;
	}
    ma_uint64 cursor;
    ma_result result = ma_decoder_get_cursor_in_pcm_frames(&ctx->decoder, &cursor);
    if (result != MA_SUCCESS) {
		return result;
	}
	
    *time = (double)cursor / ctx->decoder.outputSampleRate;
    return MA_SUCCESS;
}

MA_API ma_result get_duration(AudioContext* ctx, double* duration){
	if (ctx == NULL){
		return MA_INVALID_ARGS;
	}
    ma_uint64 cursor;
    ma_result result = ma_decoder_get_length_in_pcm_frames(&ctx->decoder, &cursor);
    if (result != MA_SUCCESS) {
		return result;
	}
	
    *duration = (double)cursor / ctx->decoder.outputSampleRate;
    return MA_SUCCESS;
}

MA_API ma_result set_volume(AudioContext* ctx, float volume){
    if (ctx == NULL){
        return MA_INVALID_ARGS;
    }

    ma_result result = ma_device_set_master_volume(&ctx->device, volume);
    if (result != MA_SUCCESS){
        return result;
    }

    return MA_SUCCESS;
}

MA_API float get_volume(AudioContext* ctx){
    if (ctx == NULL){
        return -1;
    }

	float volume = 0;
    ma_result result = ma_device_get_master_volume(&ctx->device, &volume);
    if (result != MA_SUCCESS){
        return -1;
    }

    return volume;
}

MA_API ma_device_state get_play_state(AudioContext* ctx){
    if (ctx == NULL){
        return MA_INVALID_ARGS;
    }

    return ma_device_get_state(&ctx->device);

}