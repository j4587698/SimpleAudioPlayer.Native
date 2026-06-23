#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <stddef.h>
#include <stdint.h>
#include "audio_player.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AudioRecorderContext AudioRecorderContext;

typedef enum AudioRecorderContainer {
    AUDIO_RECORDER_CONTAINER_M4A = 1,
    AUDIO_RECORDER_CONTAINER_AAC = 2,
    AUDIO_RECORDER_CONTAINER_WAV = 3,
    AUDIO_RECORDER_CONTAINER_PCM = 4
} AudioRecorderContainer;

typedef ma_result (*AudioRecorderWriteCallback)(void* userdata, const void* buffer, size_t bytesToWrite, size_t* bytesWritten);
typedef ma_result (*AudioRecorderSeekCallback)(void* userdata, int64_t offset, int origin, int64_t* cursor);

AUDIO_PLAYER_API AudioRecorderContext* audio_recorder_context_create(void);
AUDIO_PLAYER_API ma_result audio_recorder_init_file(AudioRecorderContext* ctx, const char* outputPath, AudioRecorderContainer container, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate, const ma_uint32 bitRate);
AUDIO_PLAYER_API ma_result audio_recorder_init_stream(AudioRecorderContext* ctx, AudioRecorderContainer container, AudioRecorderWriteCallback onWrite, AudioRecorderSeekCallback onSeek, void* userdata, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate, const ma_uint32 bitRate);
AUDIO_PLAYER_API ma_result audio_recorder_start(AudioRecorderContext* ctx);
AUDIO_PLAYER_API ma_result audio_recorder_stop(AudioRecorderContext* ctx);
AUDIO_PLAYER_API void audio_recorder_cleanup(AudioRecorderContext* ctx);
AUDIO_PLAYER_API ma_uint64 audio_recorder_get_captured_frames(AudioRecorderContext* ctx);
AUDIO_PLAYER_API ma_uint64 audio_recorder_get_dropped_frames(AudioRecorderContext* ctx);
AUDIO_PLAYER_API ma_result audio_recorder_get_result(AudioRecorderContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_RECORDER_H
