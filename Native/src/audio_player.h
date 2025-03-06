#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdint.h>
#include "miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 不透明句柄类型
typedef struct AudioContext AudioContext;

// 错误代码
typedef enum {
    AUDIO_SUCCESS = 0,
    AUDIO_ERROR_INIT_DECODER = -1,
    AUDIO_ERROR_INIT_DEVICE  = -2,
    AUDIO_ERROR_INVALID_FILE = -3,
    AUDIO_ERROR_DEVICE_START = -4,
	AUDIO_ERROR_MEMORY       = -5
} AudioError;

// 导出函数
MA_API AudioContext* audio_context_create(void);
MA_API AudioError audio_init_device(AudioContext* ctx, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate);
MA_API AudioError audio_init_decoder(AudioContext* ctx, ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek, ma_decoder_tell_proc onTell, void* userdata);
MA_API AudioError audio_play(AudioContext* ctx);
MA_API AudioError audio_stop(AudioContext* ctx);
MA_API void audio_cleanup(AudioContext* ctx);
MA_API ma_result seek_to_time(AudioContext* ctx, const double timeInSec);
MA_API ma_result get_decoder(AudioContext* ctx, ma_decoder *decoder);
MA_API ma_result get_length_in_pcm_frames(AudioContext* ctx, ma_int64* frames);
MA_API ma_result get_cursor_in_pcm_frames(AudioContext* ctx, ma_int64* frames);
MA_API ma_result get_time(AudioContext* ctx, double* time);
MA_API ma_result get_duration(AudioContext* ctx, double* duration);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_H

