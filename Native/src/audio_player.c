#define MA_IMPLEMENTATION
#include "audio_player.h"
#include "miniaudio.h"
#include "miniaudio_ffmpeg.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#define AUDIO_DECODE_BUFFER_SECONDS 4
#define AUDIO_DECODE_CHUNK_FRAMES 4096
#define AUDIO_DECODE_MIN_BUFFER_FRAMES 8192

typedef struct AudioDecoderBackendConfig {
    AudioLengthCallback onGetLength;
    ma_bool32 canSeek;
} AudioDecoderBackendConfig;

typedef struct AudioDecoderCallbackState {
    ma_decoder_read_proc onRead;
    ma_decoder_seek_proc onSeek;
    ma_decoder_tell_proc onTell;
    ma_bool32 canSeek;
} AudioDecoderCallbackState;

static ma_result audio_decoder_read_proxy(ma_decoder* pDecoder, void* pBufferOut, size_t bytesToRead, size_t* pBytesRead)
{
    AudioDecoderCallbackState* state = (AudioDecoderCallbackState*)pDecoder->pUserData;
    if (state == NULL || state->onRead == NULL) {
        return MA_INVALID_ARGS;
    }

    return state->onRead(pDecoder, pBufferOut, bytesToRead, pBytesRead);
}

static ma_result audio_decoder_seek_proxy(ma_decoder* pDecoder, ma_int64 byteOffset, ma_seek_origin origin)
{
    AudioDecoderCallbackState* state = (AudioDecoderCallbackState*)pDecoder->pUserData;
    if (state == NULL || state->onSeek == NULL) {
        return MA_INVALID_ARGS;
    }

    if (!state->canSeek) {
        return MA_NOT_IMPLEMENTED;
    }

    return state->onSeek(pDecoder, byteOffset, origin);
}

static ma_result audio_decoder_tell_proxy(ma_decoder* pDecoder, ma_int64* pCursor)
{
    AudioDecoderCallbackState* state = (AudioDecoderCallbackState*)pDecoder->pUserData;
    if (state == NULL || state->onTell == NULL) {
        return MA_INVALID_ARGS;
    }

    return state->onTell(pDecoder, pCursor);
}


// 自定义后端实现
static ma_result ma_decoding_backend_init__ffmpeg(void* pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend)
{
    ma_result result;
    ma_ffmpeg* pFFmpeg;
    AudioDecoderBackendConfig* backendConfig = (AudioDecoderBackendConfig*)pUserData;

    pFFmpeg = (ma_ffmpeg*)ma_malloc(sizeof(*pFFmpeg), pAllocationCallbacks);
    if (pFFmpeg == NULL) {
        return MA_OUT_OF_MEMORY;
    }

    result = ma_ffmpeg_init(
        onRead,
        onSeek,
        onTell,
        backendConfig != NULL ? (ma_ffmpeg_length_proc)backendConfig->onGetLength : NULL,
        backendConfig != NULL ? backendConfig->canSeek : MA_TRUE,
        pReadSeekTellUserData,
        pConfig,
        pAllocationCallbacks,
        pFFmpeg);
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
	ma_mutex buffer_mutex;
	ma_event decode_event;
	ma_thread decode_thread;
	AudioStopCallback managedCallback;
    bool device_initialized;
    bool decoder_initialized;
    bool decode_thread_started;
    bool decode_thread_stop;
    bool decode_active;
    bool decode_eof;
    bool decode_failed;
    bool playback_completed;
    ma_result decode_result;
    ma_uint8* pcm_buffer;
    ma_uint64 pcm_buffer_capacity_frames;
    ma_uint64 pcm_buffer_read_frame;
    ma_uint64 pcm_buffer_write_frame;
    ma_uint64 pcm_buffer_available_frames;
    ma_uint64 playback_cursor_frames;
    ma_uint32 bytes_per_frame;
    ma_uint8* decode_scratch;
    ma_uint64 decode_scratch_capacity_frames;
    AudioDecoderBackendConfig decoder_backend_config;
    AudioDecoderCallbackState decoder_callback_state;
} ;

static ma_uint64 audio_buffer_space_locked(AudioContext* ctx)
{
    return ctx->pcm_buffer_capacity_frames - ctx->pcm_buffer_available_frames;
}

static void audio_buffer_reset_locked(AudioContext* ctx)
{
    ctx->pcm_buffer_read_frame = 0;
    ctx->pcm_buffer_write_frame = 0;
    ctx->pcm_buffer_available_frames = 0;
}

static ma_uint64 audio_buffer_read_locked(AudioContext* ctx, void* pOutput, ma_uint64 frameCount)
{
    if (!ctx->pcm_buffer || ctx->bytes_per_frame == 0 || frameCount == 0) {
        return 0;
    }

    ma_uint64 framesToRead = frameCount < ctx->pcm_buffer_available_frames ? frameCount : ctx->pcm_buffer_available_frames;
    ma_uint64 first = framesToRead;
    if (ctx->pcm_buffer_read_frame + first > ctx->pcm_buffer_capacity_frames) {
        first = ctx->pcm_buffer_capacity_frames - ctx->pcm_buffer_read_frame;
    }

    if (first > 0) {
        memcpy(pOutput,
               ctx->pcm_buffer + (ctx->pcm_buffer_read_frame * ctx->bytes_per_frame),
               (size_t)(first * ctx->bytes_per_frame));
    }
    if (first < framesToRead) {
        memcpy((ma_uint8*)pOutput + (first * ctx->bytes_per_frame),
               ctx->pcm_buffer,
               (size_t)((framesToRead - first) * ctx->bytes_per_frame));
    }

    ctx->pcm_buffer_read_frame = (ctx->pcm_buffer_read_frame + framesToRead) % ctx->pcm_buffer_capacity_frames;
    ctx->pcm_buffer_available_frames -= framesToRead;
    if (ctx->pcm_buffer_available_frames == 0) {
        ctx->pcm_buffer_read_frame = 0;
        ctx->pcm_buffer_write_frame = 0;
    }

    return framesToRead;
}

static ma_uint64 audio_buffer_write_locked(AudioContext* ctx, const void* pInput, ma_uint64 frameCount)
{
    if (!ctx->pcm_buffer || ctx->bytes_per_frame == 0 || frameCount == 0) {
        return 0;
    }

    ma_uint64 framesToWrite = frameCount < audio_buffer_space_locked(ctx) ? frameCount : audio_buffer_space_locked(ctx);
    ma_uint64 first = framesToWrite;
    if (ctx->pcm_buffer_write_frame + first > ctx->pcm_buffer_capacity_frames) {
        first = ctx->pcm_buffer_capacity_frames - ctx->pcm_buffer_write_frame;
    }

    if (first > 0) {
        memcpy(ctx->pcm_buffer + (ctx->pcm_buffer_write_frame * ctx->bytes_per_frame),
               pInput,
               (size_t)(first * ctx->bytes_per_frame));
    }
    if (first < framesToWrite) {
        memcpy(ctx->pcm_buffer,
               (const ma_uint8*)pInput + (first * ctx->bytes_per_frame),
               (size_t)((framesToWrite - first) * ctx->bytes_per_frame));
    }

    ctx->pcm_buffer_write_frame = (ctx->pcm_buffer_write_frame + framesToWrite) % ctx->pcm_buffer_capacity_frames;
    ctx->pcm_buffer_available_frames += framesToWrite;
    return framesToWrite;
}

static ma_result audio_buffer_init(AudioContext* ctx, ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    ctx->bytes_per_frame = ma_get_bytes_per_frame(format, channels);
    if (ctx->bytes_per_frame == 0 || channels == 0) {
        return MA_INVALID_ARGS;
    }

    ma_uint64 capacityFrames = (ma_uint64)(sampleRate > 0 ? sampleRate : 44100) * AUDIO_DECODE_BUFFER_SECONDS;
    if (capacityFrames < AUDIO_DECODE_MIN_BUFFER_FRAMES) {
        capacityFrames = AUDIO_DECODE_MIN_BUFFER_FRAMES;
    }

    ctx->pcm_buffer = (ma_uint8*)ma_malloc((size_t)(capacityFrames * ctx->bytes_per_frame), NULL);
    if (!ctx->pcm_buffer) {
        return MA_OUT_OF_MEMORY;
    }

    ctx->decode_scratch_capacity_frames = AUDIO_DECODE_CHUNK_FRAMES;
    ctx->decode_scratch = (ma_uint8*)ma_malloc((size_t)(ctx->decode_scratch_capacity_frames * ctx->bytes_per_frame), NULL);
    if (!ctx->decode_scratch) {
        ma_free(ctx->pcm_buffer, NULL);
        ctx->pcm_buffer = NULL;
        return MA_OUT_OF_MEMORY;
    }

    ctx->pcm_buffer_capacity_frames = capacityFrames;
    audio_buffer_reset_locked(ctx);
    return MA_SUCCESS;
}

static void audio_buffer_uninit(AudioContext* ctx)
{
    ma_free(ctx->pcm_buffer, NULL);
    ctx->pcm_buffer = NULL;
    ma_free(ctx->decode_scratch, NULL);
    ctx->decode_scratch = NULL;
    ctx->pcm_buffer_capacity_frames = 0;
    ctx->decode_scratch_capacity_frames = 0;
    ctx->bytes_per_frame = 0;
    audio_buffer_reset_locked(ctx);
}

static ma_thread_result MA_THREADCALL decode_thread_proc(void* pData)
{
    AudioContext* ctx = (AudioContext*)pData;

    for (;;) {
        ma_bool32 shouldStop;
        ma_bool32 canDecode;
        ma_uint64 spaceFrames;

        ma_mutex_lock(&ctx->buffer_mutex);
        shouldStop = ctx->decode_thread_stop ? MA_TRUE : MA_FALSE;
        canDecode = (ctx->decoder_initialized && ctx->decode_active && !ctx->decode_eof && !ctx->decode_failed) ? MA_TRUE : MA_FALSE;
        spaceFrames = audio_buffer_space_locked(ctx);
        ma_mutex_unlock(&ctx->buffer_mutex);

        if (shouldStop) {
            break;
        }

        if (!canDecode || spaceFrames == 0) {
            ma_event_wait(&ctx->decode_event);
            continue;
        }

        ma_uint64 framesToRead = spaceFrames < ctx->decode_scratch_capacity_frames ? spaceFrames : ctx->decode_scratch_capacity_frames;
        ma_uint64 framesRead = 0;
        ma_result result = MA_INVALID_OPERATION;

        ma_mutex_lock(&ctx->mutex);
        if (ctx->decoder.pBackend != NULL) {
            result = ma_data_source_read_pcm_frames(&ctx->decoder, ctx->decode_scratch, framesToRead, &framesRead);
        }
        ma_mutex_unlock(&ctx->mutex);

        if (framesRead > 0) {
            ma_mutex_lock(&ctx->buffer_mutex);
            audio_buffer_write_locked(ctx, ctx->decode_scratch, framesRead);
            ma_mutex_unlock(&ctx->buffer_mutex);
        }

        if (result == MA_AT_END) {
            ma_mutex_lock(&ctx->buffer_mutex);
            ctx->decode_eof = true;
            ctx->decode_failed = false;
            ctx->decode_active = false;
            ctx->decode_result = MA_SUCCESS;
            ma_mutex_unlock(&ctx->buffer_mutex);
        } else if (result != MA_SUCCESS) {
            ma_mutex_lock(&ctx->buffer_mutex);
            ctx->decode_result = result;
            ctx->decode_failed = true;
            ctx->decode_active = false;
            ma_mutex_unlock(&ctx->buffer_mutex);
        } else if (framesRead == 0) {
            ma_sleep(1);
        }
    }

    return (ma_thread_result)0;
}

static ma_result audio_decode_thread_start(AudioContext* ctx)
{
    if (ctx->decode_thread_started) {
        return MA_SUCCESS;
    }

    ctx->decode_thread_stop = false;
    ma_result result = ma_thread_create(&ctx->decode_thread, ma_thread_priority_normal, 0, decode_thread_proc, ctx, NULL);
    if (result != MA_SUCCESS) {
        return result;
    }

    ctx->decode_thread_started = true;
    return MA_SUCCESS;
}

static void audio_decode_thread_stop(AudioContext* ctx)
{
    if (!ctx->decode_thread_started) {
        return;
    }

    ma_mutex_lock(&ctx->buffer_mutex);
    ctx->decode_thread_stop = true;
    ma_mutex_unlock(&ctx->buffer_mutex);
    ma_event_signal(&ctx->decode_event);
    ma_thread_wait(&ctx->decode_thread);
    ctx->decode_thread_started = false;
}

static void audio_set_decode_active(AudioContext* ctx, bool active)
{
    ma_mutex_lock(&ctx->buffer_mutex);
    ctx->decode_active = active && ctx->decoder_initialized && !ctx->decode_eof && !ctx->decode_failed;
    ma_mutex_unlock(&ctx->buffer_mutex);
    ma_event_signal(&ctx->decode_event);
}

// 数据回调函数
static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    AudioContext* ctx = (AudioContext*)pDevice->pUserData;
    AudioStopCallback callback = NULL;
    ma_bool32 notify_completed = MA_FALSE;
    ma_uint64 framesRead = 0;

    if (ctx) {
        ma_mutex_lock(&ctx->buffer_mutex);
        framesRead = audio_buffer_read_locked(ctx, pOutput, frameCount);
        ctx->playback_cursor_frames += framesRead;
        if ((ctx->decode_eof || ctx->decode_failed) && ctx->pcm_buffer_available_frames == 0 && !ctx->playback_completed) {
            ctx->playback_completed = true;
            callback = ctx->managedCallback;
            notify_completed = MA_TRUE;
        }
    	ma_mutex_unlock(&ctx->buffer_mutex);
        ma_event_signal(&ctx->decode_event);
    }

    if (framesRead < frameCount) {
        void* pSilence = ma_offset_pcm_frames_ptr(pOutput, framesRead, pDevice->playback.format, pDevice->playback.channels);
        ma_silence_pcm_frames(pSilence, frameCount - framesRead, pDevice->playback.format, pDevice->playback.channels);
    }

    if (notify_completed && callback) {
        callback();
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

AUDIO_PLAYER_API AudioContext* audio_context_create(){
	AudioContext* ctx = (AudioContext*)ma_malloc(sizeof(AudioContext), NULL);
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(AudioContext));
    if (ma_mutex_init(&ctx->mutex) != MA_SUCCESS) {
        ma_free(ctx, NULL);
        return NULL;
    }
    if (ma_mutex_init(&ctx->buffer_mutex) != MA_SUCCESS) {
        ma_mutex_uninit(&ctx->mutex);
        ma_free(ctx, NULL);
        return NULL;
    }
    if (ma_event_init(&ctx->decode_event) != MA_SUCCESS) {
        ma_mutex_uninit(&ctx->buffer_mutex);
        ma_mutex_uninit(&ctx->mutex);
        ma_free(ctx, NULL);
        return NULL;
    }
    return ctx;
}

static ma_bool32 audio_device_config_is_valid(const AudioDeviceConfig* config)
{
    const size_t minimumSize = offsetof(AudioDeviceConfig, share_mode) + sizeof(config->share_mode);
    if (config == NULL || config->struct_size < minimumSize || config->version != AUDIO_DEVICE_CONFIG_VERSION) {
        return MA_FALSE;
    }

    if (config->performance_profile < AUDIO_PERFORMANCE_PROFILE_LOW_LATENCY ||
        config->performance_profile > AUDIO_PERFORMANCE_PROFILE_CONSERVATIVE ||
        config->usage < AUDIO_PLAYBACK_USAGE_DEFAULT ||
        config->usage > AUDIO_PLAYBACK_USAGE_ALARM ||
        config->content_type < AUDIO_CONTENT_TYPE_DEFAULT ||
        config->content_type > AUDIO_CONTENT_TYPE_SONIFICATION ||
        config->share_mode < AUDIO_SHARE_MODE_SHARED ||
        config->share_mode > AUDIO_SHARE_MODE_EXCLUSIVE) {
        return MA_FALSE;
    }

    return MA_TRUE;
}

static void audio_apply_usage_config(ma_device_config* deviceConfig, AudioPlaybackUsage usage)
{
    switch (usage) {
        case AUDIO_PLAYBACK_USAGE_MEDIA:
            deviceConfig->aaudio.usage = ma_aaudio_usage_media;
            deviceConfig->opensl.streamType = ma_opensl_stream_type_media;
            break;
        case AUDIO_PLAYBACK_USAGE_GAME:
            deviceConfig->aaudio.usage = ma_aaudio_usage_game;
            deviceConfig->opensl.streamType = ma_opensl_stream_type_media;
            deviceConfig->wasapi.usage = ma_wasapi_usage_games;
            break;
        case AUDIO_PLAYBACK_USAGE_VOICE_COMMUNICATION:
            deviceConfig->aaudio.usage = ma_aaudio_usage_voice_communication;
            deviceConfig->opensl.streamType = ma_opensl_stream_type_voice;
            break;
        case AUDIO_PLAYBACK_USAGE_NOTIFICATION:
            deviceConfig->aaudio.usage = ma_aaudio_usage_notification;
            deviceConfig->opensl.streamType = ma_opensl_stream_type_notification;
            break;
        case AUDIO_PLAYBACK_USAGE_ALARM:
            deviceConfig->aaudio.usage = ma_aaudio_usage_alarm;
            deviceConfig->opensl.streamType = ma_opensl_stream_type_alarm;
            break;
        case AUDIO_PLAYBACK_USAGE_DEFAULT:
        default:
            break;
    }
}

static void audio_apply_content_type_config(ma_device_config* deviceConfig, AudioContentType contentType)
{
    switch (contentType) {
        case AUDIO_CONTENT_TYPE_MUSIC:
            deviceConfig->aaudio.contentType = ma_aaudio_content_type_music;
            break;
        case AUDIO_CONTENT_TYPE_SPEECH:
            deviceConfig->aaudio.contentType = ma_aaudio_content_type_speech;
            break;
        case AUDIO_CONTENT_TYPE_MOVIE:
            deviceConfig->aaudio.contentType = ma_aaudio_content_type_movie;
            break;
        case AUDIO_CONTENT_TYPE_SONIFICATION:
            deviceConfig->aaudio.contentType = ma_aaudio_content_type_sonification;
            break;
        case AUDIO_CONTENT_TYPE_DEFAULT:
        default:
            break;
    }
}

AUDIO_PLAYER_API ma_result audio_init_device_ex(AudioContext* ctx, AudioStopCallback managedCallback,
    ma_device_notification_proc notification, const AudioDeviceConfig* config)
{
    if (!ctx || !audio_device_config_is_valid(config)) return MA_INVALID_ARGS;
	if (ctx->device_initialized) return MA_SUCCESS;

    // 配置音频设备
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = config->format;
    deviceConfig.playback.channels = config->channels;
    deviceConfig.playback.shareMode = config->share_mode == AUDIO_SHARE_MODE_EXCLUSIVE
        ? ma_share_mode_exclusive
        : ma_share_mode_shared;
    deviceConfig.sampleRate        = config->sample_rate;
    deviceConfig.periodSizeInMilliseconds = config->period_size_in_milliseconds;
    deviceConfig.periods           = config->periods;
    deviceConfig.performanceProfile = config->performance_profile == AUDIO_PERFORMANCE_PROFILE_CONSERVATIVE
        ? ma_performance_profile_conservative
        : ma_performance_profile_low_latency;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = ctx;
	deviceConfig.notificationCallback = notification;
    audio_apply_usage_config(&deviceConfig, config->usage);
    audio_apply_content_type_config(&deviceConfig, config->content_type);

    ma_result result = ma_device_init(NULL, &deviceConfig, &ctx->device);
    if (result != MA_SUCCESS) {
        return MA_ERROR;
    }

    result = audio_buffer_init(ctx, ctx->device.playback.format, ctx->device.playback.channels, ctx->device.sampleRate);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&ctx->device);
        return result;
    }

    result = audio_decode_thread_start(ctx);
    if (result != MA_SUCCESS) {
        audio_buffer_uninit(ctx);
        ma_device_uninit(&ctx->device);
        return result;
    }

	ctx->managedCallback = managedCallback;

    ctx->device_initialized = true;
    return MA_SUCCESS;
}

AUDIO_PLAYER_API ma_result audio_init_device(AudioContext* ctx, AudioStopCallback managedCallback,
	ma_device_notification_proc notification, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate)
{
    AudioDeviceConfig config;
    memset(&config, 0, sizeof(config));
    config.struct_size = sizeof(config);
    config.version = AUDIO_DEVICE_CONFIG_VERSION;
    config.format = format;
    config.channels = channels;
    config.sample_rate = sampleRate;
    config.performance_profile = AUDIO_PERFORMANCE_PROFILE_LOW_LATENCY;
    config.usage = AUDIO_PLAYBACK_USAGE_DEFAULT;
    config.content_type = AUDIO_CONTENT_TYPE_DEFAULT;
    config.share_mode = AUDIO_SHARE_MODE_SHARED;

    return audio_init_device_ex(ctx, managedCallback, notification, &config);
}



AUDIO_PLAYER_API ma_result audio_init_decoder(AudioContext* ctx, ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek,
	ma_decoder_tell_proc onTell, AudioLengthCallback onGetLength, ma_bool32 canSeek, void* userdata)
{
    if (!ctx) return MA_INVALID_ARGS;
    if (!ctx->device_initialized) return MA_DEVICE_NOT_INITIALIZED;
    (void)userdata;

	audio_set_decode_active(ctx, false);

	ma_mutex_lock(&ctx->mutex);
    if (ctx->decoder.pBackend != NULL){
		ma_decoder_uninit(&ctx->decoder);
	}

    ma_mutex_lock(&ctx->buffer_mutex);
    ctx->decoder_initialized = false;
    ctx->decode_eof = false;
    ctx->decode_failed = false;
    ctx->decode_result = MA_SUCCESS;
    ctx->playback_completed = false;
    ctx->playback_cursor_frames = 0;
    audio_buffer_reset_locked(ctx);
    ma_mutex_unlock(&ctx->buffer_mutex);

    // 配置自定义后端
    ma_decoding_backend_vtable* backends[] = { &g_ma_decoding_backend_vtable_ffmpeg };

    ma_decoder_config decoderConfig = ma_decoder_config_init(ctx->device.playback.format, ctx->device.playback.channels, ctx->device.sampleRate);
    decoderConfig.ppCustomBackendVTables = backends;
    decoderConfig.customBackendCount = 1;
    ctx->decoder_backend_config.onGetLength = onGetLength;
    ctx->decoder_backend_config.canSeek = canSeek;
    ctx->decoder_callback_state.onRead = onRead;
    ctx->decoder_callback_state.onSeek = onSeek;
    ctx->decoder_callback_state.onTell = onTell;
    ctx->decoder_callback_state.canSeek = canSeek;
    decoderConfig.pCustomBackendUserData = &ctx->decoder_backend_config;

    // 初始化解码器
    ma_result result = ma_decoder_init_with_tell(
        audio_decoder_read_proxy,
        audio_decoder_seek_proxy,
        audio_decoder_tell_proxy,
        &ctx->decoder_callback_state,
        &decoderConfig,
        &ctx->decoder);
	if (result == MA_SUCCESS) {
        ma_mutex_lock(&ctx->buffer_mutex);
        ctx->decoder_initialized = true;
        ctx->decode_active = true;
        ctx->decode_eof = false;
        ctx->decode_failed = false;
        ctx->decode_result = MA_SUCCESS;
        ma_mutex_unlock(&ctx->buffer_mutex);
    }

	ma_mutex_unlock(&ctx->mutex);
	ma_event_signal(&ctx->decode_event);

    return result;
}

AUDIO_PLAYER_API ma_result audio_play(AudioContext* ctx)
{
    if (!ctx) return MA_INVALID_ARGS;
	if (!ctx->device_initialized) return MA_DEVICE_NOT_INITIALIZED;

	audio_set_decode_active(ctx, true);

    if (ma_device_start(&ctx->device) == MA_SUCCESS) {
        return MA_SUCCESS;
    }
	return MA_ERROR;
}

AUDIO_PLAYER_API ma_result audio_stop(AudioContext* ctx)
{
    if (!ctx) return MA_INVALID_ARGS;
	if (!ctx->device_initialized) return MA_DEVICE_NOT_INITIALIZED;

	audio_set_decode_active(ctx, false);

    ma_device_stop(&ctx->device);
    ma_mutex_lock(&ctx->mutex);
    ma_mutex_unlock(&ctx->mutex);
    return MA_SUCCESS;
}

AUDIO_PLAYER_API void audio_cleanup(AudioContext* ctx)
{
    if (!ctx) return;

    if (ctx->device_initialized) {
        ma_device_stop(&ctx->device);
        ma_device_uninit(&ctx->device);
        ctx->device_initialized = false;
    }

	audio_decode_thread_stop(ctx);

	ma_mutex_lock(&ctx->mutex);
    if (ctx->decoder.pBackend != NULL) {
        ma_decoder_uninit(&ctx->decoder);
    }

	ma_mutex_unlock(&ctx->mutex);
    ma_mutex_lock(&ctx->buffer_mutex);
    ctx->decoder_initialized = false;
    ctx->decode_active = false;
    ma_mutex_unlock(&ctx->buffer_mutex);
	audio_buffer_uninit(ctx);
	ma_event_uninit(&ctx->decode_event);
	ma_mutex_uninit(&ctx->buffer_mutex);
	ma_mutex_uninit(&ctx->mutex);
    ma_free(ctx, NULL);
}

AUDIO_PLAYER_API ma_result seek_to_time(AudioContext* ctx, const double timeInSec) {
    if (ctx == NULL || timeInSec < 0) {
        return MA_INVALID_ARGS;
    }

    ma_bool32 wasActive;
    ma_mutex_lock(&ctx->buffer_mutex);
    wasActive = ctx->decode_active ? MA_TRUE : MA_FALSE;
    ma_mutex_unlock(&ctx->buffer_mutex);

    audio_set_decode_active(ctx, false);

	ma_mutex_lock(&ctx->mutex);
    if (ctx->decoder.pBackend == NULL || ctx->decoder.outputSampleRate == 0) {
		ma_mutex_unlock(&ctx->mutex);
        ma_mutex_lock(&ctx->buffer_mutex);
        ctx->decode_active = wasActive && ctx->decoder_initialized && !ctx->decode_eof && !ctx->decode_failed;
        ma_mutex_unlock(&ctx->buffer_mutex);
        ma_event_signal(&ctx->decode_event);
		return MA_INVALID_OPERATION;
    }
    ma_uint64 target_frame = (ma_uint64)(timeInSec * ctx->decoder.outputSampleRate);
    ma_result result = ma_decoder_seek_to_pcm_frame(&ctx->decoder, target_frame);
    ma_mutex_lock(&ctx->buffer_mutex);
	if (result == MA_SUCCESS) {
        audio_buffer_reset_locked(ctx);
		ctx->playback_completed = false;
        ctx->decode_eof = false;
        ctx->decode_failed = false;
        ctx->decode_result = MA_SUCCESS;
        ctx->playback_cursor_frames = target_frame;
        ctx->decode_active = true;
    } else {
        ctx->decode_active = wasActive && ctx->decoder_initialized && !ctx->decode_eof && !ctx->decode_failed;
	}
    ma_mutex_unlock(&ctx->buffer_mutex);
	ma_mutex_unlock(&ctx->mutex);
    ma_event_signal(&ctx->decode_event);
	return result;
}

AUDIO_PLAYER_API ma_result get_decoder(AudioContext* ctx, ma_decoder **decoder) {
	if (ctx == NULL || decoder == NULL){
		return MA_INVALID_ARGS;
	}

	ma_mutex_lock(&ctx->mutex);
    if (ctx->decoder.pBackend == NULL) {
        ma_mutex_unlock(&ctx->mutex);
        return MA_INVALID_OPERATION;
    }
    *decoder = &ctx->decoder;
    ma_mutex_unlock(&ctx->mutex);

	return MA_SUCCESS;
}

AUDIO_PLAYER_API ma_result get_length_in_pcm_frames(AudioContext* ctx, ma_uint64* frames){
    if (ctx == NULL || frames == NULL){
		return MA_INVALID_ARGS;
	}

    ma_mutex_lock(&ctx->mutex);
    if (ctx->decoder.pBackend == NULL) {
        ma_mutex_unlock(&ctx->mutex);
        return MA_INVALID_OPERATION;
    }
    ma_result result = ma_decoder_get_length_in_pcm_frames(&ctx->decoder, frames);
    ma_mutex_unlock(&ctx->mutex);
    return result;
}

AUDIO_PLAYER_API ma_result get_cursor_in_pcm_frames(AudioContext* ctx, ma_uint64* frames){
    if (ctx == NULL || frames == NULL){
		return MA_INVALID_ARGS;
	}

    ma_mutex_lock(&ctx->buffer_mutex);
    if (!ctx->decoder_initialized) {
        ma_mutex_unlock(&ctx->buffer_mutex);
        return MA_INVALID_OPERATION;
    }
    *frames = ctx->playback_cursor_frames;
    ma_mutex_unlock(&ctx->buffer_mutex);
    return MA_SUCCESS;
}

AUDIO_PLAYER_API ma_result get_time(AudioContext* ctx, double* time){
    if (ctx == NULL || time == NULL){
		return MA_INVALID_ARGS;
	}
    ma_uint64 cursor;
    ma_uint32 sampleRate;
    ma_mutex_lock(&ctx->buffer_mutex);
    if (!ctx->decoder_initialized) {
        ma_mutex_unlock(&ctx->buffer_mutex);
        return MA_INVALID_OPERATION;
    }
    cursor = ctx->playback_cursor_frames;
    ma_mutex_unlock(&ctx->buffer_mutex);

    ma_mutex_lock(&ctx->mutex);
    if (ctx->decoder.pBackend == NULL || ctx->decoder.outputSampleRate == 0) {
        ma_mutex_unlock(&ctx->mutex);
        return MA_INVALID_OPERATION;
    }
	sampleRate = ctx->decoder.outputSampleRate;
	ma_mutex_unlock(&ctx->mutex);

    *time = (double)cursor / sampleRate;
    return MA_SUCCESS;
}

AUDIO_PLAYER_API ma_result get_duration(AudioContext* ctx, double* duration){
    if (ctx == NULL || duration == NULL){
		return MA_INVALID_ARGS;
	}
    ma_uint64 cursor;
    ma_mutex_lock(&ctx->mutex);
    if (ctx->decoder.pBackend == NULL || ctx->decoder.outputSampleRate == 0) {
        ma_mutex_unlock(&ctx->mutex);
        return MA_INVALID_OPERATION;
    }
    ma_result result = ma_decoder_get_length_in_pcm_frames(&ctx->decoder, &cursor);
    if (result != MA_SUCCESS) {
        ma_mutex_unlock(&ctx->mutex);
		return result;
	}

    *duration = (double)cursor / ctx->decoder.outputSampleRate;
    ma_mutex_unlock(&ctx->mutex);
    return MA_SUCCESS;
}

AUDIO_PLAYER_API ma_result set_volume(AudioContext* ctx, float volume){
    if (ctx == NULL){
        return MA_INVALID_ARGS;
    }
    if (!ctx->device_initialized) return MA_DEVICE_NOT_INITIALIZED;

    ma_result result = ma_device_set_master_volume(&ctx->device, volume);
    if (result != MA_SUCCESS){
        return result;
    }

    return MA_SUCCESS;
}

AUDIO_PLAYER_API float get_volume(AudioContext* ctx){
    if (ctx == NULL){
        return -1;
    }
	if (!ctx->device_initialized) return -1;

	float volume = 0;
    ma_result result = ma_device_get_master_volume(&ctx->device, &volume);
    if (result != MA_SUCCESS){
        return -1;
    }

    return volume;
}

AUDIO_PLAYER_API ma_device_state get_play_state(AudioContext* ctx){
    if (ctx == NULL || !ctx->device_initialized){
        return ma_device_state_uninitialized;
    }

    return ma_device_get_state(&ctx->device);

}

AUDIO_PLAYER_API ma_result get_decode_result(AudioContext* ctx){
    if (ctx == NULL){
        return MA_INVALID_ARGS;
    }

    ma_mutex_lock(&ctx->buffer_mutex);
    ma_result result = ctx->decode_result;
    ma_mutex_unlock(&ctx->buffer_mutex);
    return result;
}

#include "audio_recorder.c"
