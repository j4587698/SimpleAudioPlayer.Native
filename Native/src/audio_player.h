#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdint.h>
#include "miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 不透明句柄类型
typedef struct AudioContext AudioContext;

// 导出函数
MA_API AudioContext* audio_context_create(void);
MA_API ma_result audio_init_device(AudioContext* ctx, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate);
MA_API ma_result audio_init_decoder(AudioContext* ctx, ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek, ma_decoder_tell_proc onTell, void* userdata);
MA_API ma_result audio_play(AudioContext* ctx);
MA_API ma_result audio_stop(AudioContext* ctx);
MA_API void audio_cleanup(AudioContext* ctx);
MA_API ma_result seek_to_time(AudioContext* ctx, const double timeInSec);
MA_API ma_result get_decoder(AudioContext* ctx, ma_decoder *decoder);
MA_API ma_result get_length_in_pcm_frames(AudioContext* ctx, ma_int64* frames);
MA_API ma_result get_cursor_in_pcm_frames(AudioContext* ctx, ma_int64* frames);
MA_API ma_result get_time(AudioContext* ctx, double* time);
MA_API ma_result get_duration(AudioContext* ctx, double* duration);
MA_API float get_volume(AudioContext* ctx);
MA_API ma_result set_volume(AudioContext* ctx, float volume);
MA_API ma_bool32 is_playing(AudioContext* ctx);
MA_API ma_device_state get_play_state(AudioContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_H

