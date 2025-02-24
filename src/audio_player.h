#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <stdint.h>
#include "miniaudio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 不透明句柄类型
typedef void* AudioHandle;

// 错误代码
typedef enum {
    AUDIO_SUCCESS = 0,
    AUDIO_ERROR_INIT_DECODER = -1,
    AUDIO_ERROR_INIT_DEVICE  = -2,
    AUDIO_ERROR_INVALID_FILE = -3,
    AUDIO_ERROR_DEVICE_START = -4
} AudioError;

// 导出函数
MA_API AudioError audio_init(const char* filePath, AudioHandle* handle);
MA_API void audio_play(AudioHandle handle);
MA_API void audio_stop(AudioHandle handle);
MA_API void audio_cleanup(AudioHandle handle);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_PLAYER_H

