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

typedef enum AudioPerformanceProfile {
    AUDIO_PERFORMANCE_PROFILE_LOW_LATENCY = 0,
    AUDIO_PERFORMANCE_PROFILE_CONSERVATIVE = 1
} AudioPerformanceProfile;

typedef enum AudioPlaybackUsage {
    AUDIO_PLAYBACK_USAGE_DEFAULT = 0,
    AUDIO_PLAYBACK_USAGE_MEDIA = 1,
    AUDIO_PLAYBACK_USAGE_GAME = 2,
    AUDIO_PLAYBACK_USAGE_VOICE_COMMUNICATION = 3,
    AUDIO_PLAYBACK_USAGE_NOTIFICATION = 4,
    AUDIO_PLAYBACK_USAGE_ALARM = 5
} AudioPlaybackUsage;

typedef enum AudioContentType {
    AUDIO_CONTENT_TYPE_DEFAULT = 0,
    AUDIO_CONTENT_TYPE_MUSIC = 1,
    AUDIO_CONTENT_TYPE_SPEECH = 2,
    AUDIO_CONTENT_TYPE_MOVIE = 3,
    AUDIO_CONTENT_TYPE_SONIFICATION = 4
} AudioContentType;

typedef enum AudioShareMode {
    AUDIO_SHARE_MODE_SHARED = 0,
    AUDIO_SHARE_MODE_EXCLUSIVE = 1
} AudioShareMode;

#define AUDIO_DEVICE_CONFIG_VERSION 1

typedef struct AudioDeviceConfig {
    uint32_t struct_size;
    uint32_t version;
    ma_format format;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t period_size_in_milliseconds;
    uint32_t periods;
    AudioPerformanceProfile performance_profile;
    AudioPlaybackUsage usage;
    AudioContentType content_type;
    AudioShareMode share_mode;
} AudioDeviceConfig;

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
AUDIO_PLAYER_API ma_result audio_init_device_ex(AudioContext* ctx, AudioStopCallback managedCallback, ma_device_notification_proc notification, const AudioDeviceConfig* config);
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

