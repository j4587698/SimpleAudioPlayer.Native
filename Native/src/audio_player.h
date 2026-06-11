#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdint.h>
#include "miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 不透明句柄类型
typedef struct AudioContext AudioContext;
typedef void (*AudioStopCallback)(void);
typedef ma_result (*AudioLengthCallback)(void* userdata, int64_t* length);

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(AUDIO_PLAYER_BUILD)
#    define AUDIO_PLAYER_API __declspec(dllexport)
#  else
#    define AUDIO_PLAYER_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define AUDIO_PLAYER_API __attribute__((visibility("default")))
#else
#  define AUDIO_PLAYER_API
#endif

// 导出函数
AUDIO_PLAYER_API AudioContext* audio_context_create(void);
AUDIO_PLAYER_API ma_result audio_init_device(AudioContext* ctx, AudioStopCallback managedCallback, ma_device_notification_proc notification, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate);
AUDIO_PLAYER_API ma_result audio_init_decoder(AudioContext* ctx, ma_decoder_read_proc onRead, ma_decoder_seek_proc onSeek, ma_decoder_tell_proc onTell, AudioLengthCallback onGetLength, ma_bool32 canSeek, void* userdata);
AUDIO_PLAYER_API ma_result audio_play(AudioContext* ctx);
AUDIO_PLAYER_API ma_result audio_stop(AudioContext* ctx);
AUDIO_PLAYER_API void audio_cleanup(AudioContext* ctx);
AUDIO_PLAYER_API ma_result seek_to_time(AudioContext* ctx, const double timeInSec);
AUDIO_PLAYER_API ma_result get_decoder(AudioContext* ctx, ma_decoder **decoder);
AUDIO_PLAYER_API ma_result get_length_in_pcm_frames(AudioContext* ctx, ma_uint64* frames);
AUDIO_PLAYER_API ma_result get_cursor_in_pcm_frames(AudioContext* ctx, ma_uint64* frames);
AUDIO_PLAYER_API ma_result get_time(AudioContext* ctx, double* time);
AUDIO_PLAYER_API ma_result get_duration(AudioContext* ctx, double* duration);
AUDIO_PLAYER_API float get_volume(AudioContext* ctx);
AUDIO_PLAYER_API ma_result set_volume(AudioContext* ctx, float volume);
AUDIO_PLAYER_API ma_device_state get_play_state(AudioContext* ctx);
AUDIO_PLAYER_API ma_result get_decode_result(AudioContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_H

