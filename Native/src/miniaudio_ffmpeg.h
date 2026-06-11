/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2025 Mr-Ojii

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef miniaudio_ffmpeg_h
#define miniaudio_ffmpeg_h

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MA_NO_FFMPEG)
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

typedef struct
{
    ma_uint8* data;
    size_t capacity;
    size_t size;
    size_t readPos;
    size_t writePos;
} ma_ffmpeg_queue;

static void ma_ffmpeg_queue_init(ma_ffmpeg_queue* queue);
static void ma_ffmpeg_queue_clear(ma_ffmpeg_queue* queue);
static void ma_ffmpeg_queue_uninit(ma_ffmpeg_queue* queue);
static ma_result ma_ffmpeg_queue_enqueue(ma_ffmpeg_queue* queue, void* data, size_t size);
static ma_result ma_ffmpeg_queue_read(ma_ffmpeg_queue* queue, void* data, size_t size, size_t* readSize);
#endif // !MA_NO_FFMPEG

typedef ma_result (*ma_ffmpeg_length_proc)(void* pUserData, ma_int64* pLength);

typedef struct
{
    ma_data_source_base ds;
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    ma_ffmpeg_length_proc onGetLength;
    void* pReadSeekTellUserData;
    ma_bool32 isSeekable;
    ma_format format;
#if !defined(MA_NO_FFMPEG)
    uint8_t bitsPerSample;
    enum AVSampleFormat avFormat;
    AVFormatContext* formatCtx;
    AVIOContext* avioCtx;
    AVCodecContext* codecCtx;
    AVStream* stream;
    AVFrame* frame;
    AVPacket* packet;
    SwrContext* swrCtx;
    AVBufferRef* swrBuf;       // 改为AVBufferRef类型
    int swrBufSize;   // 新增缓冲区大小跟踪
    uint64_t cursor;
    ma_ffmpeg_queue queue;
	ma_mutex lock;
    ma_bool32 eofReached;
#endif
} ma_ffmpeg;

MA_API ma_result ma_ffmpeg_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, ma_ffmpeg_length_proc onGetLength, ma_bool32 isSeekable, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_ffmpeg* pFFmpeg);
MA_API void ma_ffmpeg_uninit(ma_ffmpeg* pFFmpeg, const ma_allocation_callbacks* pAllocationCallbacks);
MA_API ma_result ma_ffmpeg_read_pcm_frames(ma_ffmpeg* pFFmpeg, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead);
MA_API ma_result ma_ffmpeg_seek_to_pcm_frame(ma_ffmpeg* pFFmpeg, ma_uint64 frameIndex);
MA_API ma_result ma_ffmpeg_get_data_format(ma_ffmpeg* pFFmpeg, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap);
MA_API ma_result ma_ffmpeg_get_cursor_in_pcm_frames(ma_ffmpeg* pFFmpeg, ma_uint64* pCursor);
MA_API ma_result ma_ffmpeg_get_length_in_pcm_frames(ma_ffmpeg* pFFmpeg, ma_uint64* pLength);

#ifdef __cplusplus
}
#endif
#endif // miniaudio_ffmpeg_h


#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)


static ma_result ma_ffmpeg_ds_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
{
    ma_result res = ma_ffmpeg_read_pcm_frames((ma_ffmpeg*)pDataSource, pFramesOut, frameCount, pFramesRead);
    return res;
}

static ma_result ma_ffmpeg_ds_seek(ma_data_source* pDataSource, ma_uint64 frameIndex)
{
    return ma_ffmpeg_seek_to_pcm_frame((ma_ffmpeg*)pDataSource, frameIndex);
}

static ma_result ma_ffmpeg_ds_get_data_format(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
{
    return ma_ffmpeg_get_data_format((ma_ffmpeg*)pDataSource, pFormat, pChannels, pSampleRate, pChannelMap, channelMapCap);
}

static ma_result ma_ffmpeg_ds_get_cursor(ma_data_source* pDataSource, ma_uint64* pCursor)
{
    return ma_ffmpeg_get_cursor_in_pcm_frames((ma_ffmpeg*)pDataSource, pCursor);
}

static ma_result ma_ffmpeg_ds_get_length(ma_data_source* pDataSource, ma_uint64* pLength)
{
    return ma_ffmpeg_get_length_in_pcm_frames((ma_ffmpeg*)pDataSource, pLength);
}

static ma_data_source_vtable g_ma_ffmpeg_ds_vtable =
{
    ma_ffmpeg_ds_read,
    ma_ffmpeg_ds_seek,
    ma_ffmpeg_ds_get_data_format,
    ma_ffmpeg_ds_get_cursor,
    ma_ffmpeg_ds_get_length
};

#if !defined(MA_NO_FFMPEG)

// avio callback
static int ma_ffmpeg_avio_callback__read(void* opaque, uint8_t* buf, int buf_size) {
    ma_ffmpeg* pFFmpeg = (ma_ffmpeg*)opaque;
    ma_result result;
	size_t bytesToRead = 0;

    if (!pFFmpeg || !pFFmpeg->onRead || !buf || buf_size <= 0) {
        return -1;
    }

    result = pFFmpeg->onRead(pFFmpeg->pReadSeekTellUserData, buf, buf_size, &bytesToRead);
    if (result == MA_AT_END) {
        return AVERROR_EOF;
    }
    if (result != MA_SUCCESS) {
        return AVERROR(EIO);
    }
    if (bytesToRead == 0) {
        return AVERROR_EOF;
    }
    if (bytesToRead > (size_t)buf_size) {
        bytesToRead = (size_t)buf_size;
    }
    return (int)bytesToRead;
}

static int64_t ma_ffmpeg_avio_callback__seek(void* opaque, int64_t offset, int whence) {
    ma_ffmpeg* pFFmpeg = (ma_ffmpeg*)opaque;
    ma_result result;
    ma_seek_origin origin;
    ma_int64 cursor;

    if (whence == AVSEEK_SIZE) {
        ma_int64 length = 0;
        if (pFFmpeg->onGetLength == NULL) {
            return AVERROR(ENOSYS);
        }

        result = pFFmpeg->onGetLength(pFFmpeg->pReadSeekTellUserData, &length);
        if (result != MA_SUCCESS || length < 0) {
            return AVERROR(ENOSYS);
        }

        return length;
    }

    if (!pFFmpeg->isSeekable || !pFFmpeg->onSeek || !pFFmpeg->onTell) {
        return AVERROR(ENOSYS);
    }

    whence &= ~AVSEEK_FORCE;
    if (whence == SEEK_SET) {
        origin = ma_seek_origin_start;
    } else if (whence == SEEK_CUR) {
        origin = ma_seek_origin_current;
    } else if (whence == SEEK_END) {
        origin = ma_seek_origin_end;
    } else {
        return AVERROR(ENOSYS);
    }

    result = pFFmpeg->onSeek(pFFmpeg->pReadSeekTellUserData, offset, origin);
    if (result != MA_SUCCESS) {
        return AVERROR(EIO);
    }

    result = pFFmpeg->onTell(pFFmpeg->pReadSeekTellUserData, &cursor);
    if (result != MA_SUCCESS) {
        return AVERROR(EIO);
    }

    return cursor;
}

static void ma_ffmpeg_free_avio(ma_ffmpeg* pFFmpeg) {
    if (pFFmpeg && pFFmpeg->avioCtx) {
        av_freep(&pFFmpeg->avioCtx->buffer);
        avio_context_free(&pFFmpeg->avioCtx);
    }
}

static void ma_ffmpeg_close_input(ma_ffmpeg* pFFmpeg) {
    if (!pFFmpeg) {
        return;
    }

    if (pFFmpeg->formatCtx) {
        avformat_close_input(&pFFmpeg->formatCtx);
    }
    ma_ffmpeg_free_avio(pFFmpeg);
}
// avio callback

// queue
static void ma_ffmpeg_queue_init(ma_ffmpeg_queue* queue) {
    queue->data = NULL;
    queue->capacity = 0;
    queue->size = 0;
    queue->readPos = 0;
    queue->writePos = 0;
}

static void ma_ffmpeg_queue_clear(ma_ffmpeg_queue* queue) {
    queue->size = 0;
    queue->readPos = 0;
    queue->writePos = 0;
}

static void ma_ffmpeg_queue_uninit(ma_ffmpeg_queue* queue) {
    if (!queue) {
        return;
    }

    free(queue->data);
    queue->data = NULL;
    queue->capacity = 0;
    ma_ffmpeg_queue_clear(queue);
}

static ma_result ma_ffmpeg_queue_ensure_capacity(ma_ffmpeg_queue* queue, size_t required) {
    if (required <= queue->capacity) {
        return MA_SUCCESS;
    }

    size_t newCapacity = queue->capacity > 0 ? queue->capacity : 4096;
    while (newCapacity < required) {
        if (newCapacity > ((size_t)-1) / 2) {
            return MA_OUT_OF_MEMORY;
        }
        newCapacity *= 2;
    }

    ma_uint8* newData = (ma_uint8*)malloc(newCapacity);
    if (!newData) {
        return MA_OUT_OF_MEMORY;
    }

    if (queue->size > 0 && queue->data) {
        size_t first = queue->size;
        if (queue->readPos + first > queue->capacity) {
            first = queue->capacity - queue->readPos;
        }
        memcpy(newData, queue->data + queue->readPos, first);
        if (first < queue->size) {
            memcpy(newData + first, queue->data, queue->size - first);
        }
    }

    free(queue->data);
    queue->data = newData;
    queue->capacity = newCapacity;
    queue->readPos = 0;
    queue->writePos = queue->size;
    if (queue->writePos == queue->capacity) {
        queue->writePos = 0;
    }

    return MA_SUCCESS;
}

static ma_result ma_ffmpeg_queue_enqueue(ma_ffmpeg_queue* queue, void* data, size_t size) {
    if (!queue || (!data && size > 0)) {
        return MA_INVALID_ARGS;
    }
    if (size == 0) {
        return MA_SUCCESS;
    }

    ma_result result = ma_ffmpeg_queue_ensure_capacity(queue, queue->size + size);
    if (result != MA_SUCCESS) {
        return result;
    }

    size_t first = size;
    if (queue->writePos + first > queue->capacity) {
        first = queue->capacity - queue->writePos;
    }
    memcpy(queue->data + queue->writePos, data, first);
    if (first < size) {
        memcpy(queue->data, (ma_uint8*)data + first, size - first);
    }

    queue->writePos = (queue->writePos + size) % queue->capacity;
    queue->size += size;

    return MA_SUCCESS;
}

static ma_result ma_ffmpeg_queue_read(ma_ffmpeg_queue* queue, void* data, size_t size, size_t* readSize) {
    if (readSize)
        *readSize = 0;

    if (!queue || queue->size == 0)
        return MA_ERROR;

    size_t toRead = size < queue->size ? size : queue->size;
    size_t first = toRead;
    if (queue->readPos + first > queue->capacity) {
        first = queue->capacity - queue->readPos;
    }

    if (data && first > 0) {
        memcpy(data, queue->data + queue->readPos, first);
    }
    if (data && first < toRead) {
        memcpy((ma_uint8*)data + first, queue->data, toRead - first);
    }

    queue->readPos = (queue->readPos + toRead) % queue->capacity;
    queue->size -= toRead;
    if (queue->size == 0) {
        queue->readPos = 0;
        queue->writePos = 0;
    }

    if (readSize)
        *readSize = toRead;

    return MA_SUCCESS;
}
// queue

static ma_result decode_one_cycle(ma_ffmpeg* pFFmpeg) {
    if (!pFFmpeg) return MA_INVALID_ARGS;

    int ret = AVERROR_UNKNOWN;
    AVFrame* frame = pFFmpeg->frame;

    /*-----------------------------------------------------------
    第一部分：读取数据包
    -----------------------------------------------------------*/
    if (!pFFmpeg->eofReached) { // 只有未到EOF时才读取新包
        ret = av_read_frame(pFFmpeg->formatCtx, pFFmpeg->packet);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                // 标记EOF并发送空包刷新解码器
                pFFmpeg->eofReached = MA_TRUE;
                avcodec_send_packet(pFFmpeg->codecCtx, NULL);
            } else {
                return MA_ERROR;
            }
        } else {
            if (pFFmpeg->packet->stream_index != pFFmpeg->stream->index) {
                av_packet_unref(pFFmpeg->packet);
                return MA_SUCCESS;
            }

            // 正常发送数据包
            if (avcodec_send_packet(pFFmpeg->codecCtx, pFFmpeg->packet) < 0) {
                av_packet_unref(pFFmpeg->packet);
                return MA_ERROR;
            }
            av_packet_unref(pFFmpeg->packet);
        }
    }

    /*-----------------------------------------------------------
    第二部分：处理解码帧（含残留帧）
    -----------------------------------------------------------*/
    while (1) {
        ret = avcodec_receive_frame(pFFmpeg->codecCtx, frame);
        if (ret == AVERROR(EAGAIN)) {
            return MA_SUCCESS; // 需要更多输入数据
        } else if (ret == AVERROR_EOF) {
            return MA_AT_END; // 所有残留帧处理完毕
        } else if (ret < 0) {
            return MA_ERROR;
        }

        /*-------------------------------------------------------
        第三部分：处理有效帧
        -------------------------------------------------------*/
        // 更新播放光标（带PTS补偿）
        if (frame->pts != AV_NOPTS_VALUE) {
            pFFmpeg->cursor = (frame->pts * pFFmpeg->codecCtx->sample_rate
                             * pFFmpeg->stream->time_base.num)
                             / pFFmpeg->stream->time_base.den;
        }
        pFFmpeg->cursor += frame->nb_samples;

        // 格式转换处理
        if (frame->format != pFFmpeg->avFormat) {
            if (!pFFmpeg->swrCtx) {
                // 先释放旧的swrCtx（如果存在）
                swr_free(&pFFmpeg->swrCtx);

                pFFmpeg->swrCtx = swr_alloc();
                if (pFFmpeg->swrCtx == NULL) {
                    return MA_ERROR;
                }
                av_opt_set_chlayout(pFFmpeg->swrCtx, "in_chlayout", &pFFmpeg->frame->ch_layout, 0);
                av_opt_set_chlayout(pFFmpeg->swrCtx, "out_chlayout", &pFFmpeg->frame->ch_layout, 0);
                av_opt_set_int(pFFmpeg->swrCtx, "in_sample_rate", pFFmpeg->frame->sample_rate, 0);
                av_opt_set_int(pFFmpeg->swrCtx, "out_sample_rate", pFFmpeg->frame->sample_rate, 0);
                av_opt_set_sample_fmt(pFFmpeg->swrCtx, "in_sample_fmt", (enum AVSampleFormat)pFFmpeg->frame->format, 0);
                av_opt_set_sample_fmt(pFFmpeg->swrCtx, "out_sample_fmt", pFFmpeg->avFormat, 0);
                if (swr_init(pFFmpeg->swrCtx) < 0) {
                    return MA_ERROR;
                }
            }

            // 计算目标缓冲区大小
            int dst_nb_samples = swr_get_out_samples(pFFmpeg->swrCtx, frame->nb_samples);
            int buf_size = av_samples_get_buffer_size(
                NULL, pFFmpeg->codecCtx->ch_layout.nb_channels,
                dst_nb_samples, pFFmpeg->avFormat, 1
            );

            // 检查并重新分配缓冲区
            if (buf_size > pFFmpeg->swrBufSize) {
                av_buffer_unref(&pFFmpeg->swrBuf);
                pFFmpeg->swrBuf = av_buffer_alloc(buf_size);
                if (!pFFmpeg->swrBuf) {
                    return MA_OUT_OF_MEMORY;
                }
                pFFmpeg->swrBufSize = buf_size;
            }

            // 执行格式转换
            uint8_t* swr_data = pFFmpeg->swrBuf ? pFFmpeg->swrBuf->data : NULL;
            int converted = swr_convert(
                pFFmpeg->swrCtx,
                &swr_data,
                dst_nb_samples,
                (const uint8_t**)frame->extended_data, frame->nb_samples
            );

            if (converted < 0) {
                return MA_ERROR;
            }

            // 将转换后的数据入队
            size_t data_size = converted * pFFmpeg->bitsPerSample
                             * pFFmpeg->codecCtx->ch_layout.nb_channels;
            ma_result queueResult = ma_ffmpeg_queue_enqueue(&pFFmpeg->queue, swr_data, data_size);
            if (queueResult != MA_SUCCESS) {
                av_frame_unref(frame);
                return queueResult;
            }
        } else {
            // 直接入队原始数据
            size_t data_size = frame->nb_samples * pFFmpeg->bitsPerSample
                             * pFFmpeg->codecCtx->ch_layout.nb_channels;
            ma_result queueResult = ma_ffmpeg_queue_enqueue(&pFFmpeg->queue, frame->extended_data[0], data_size);
            if (queueResult != MA_SUCCESS) {
                av_frame_unref(frame);
                return queueResult;
            }
        }

        av_frame_unref(frame); // 重要：释放帧引用
    }

    return MA_SUCCESS; // 永远不会执行到这里
}

#endif // !MA_NO_FFMPEG

static ma_result ma_ffmpeg_init_internal(const ma_decoding_backend_config *pConfig, ma_ffmpeg* pFFmpeg) {
    ma_result result;
    ma_data_source_config dataSourceConfig;

    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    MA_ZERO_OBJECT(pFFmpeg);
    pFFmpeg->format = ma_format_f32;

    #if !defined(MA_NO_FFMPEG)
    {
        pFFmpeg->avFormat = AV_SAMPLE_FMT_FLT;
        pFFmpeg->bitsPerSample = 4;

        if (pConfig != NULL) {
            switch(pConfig->preferredFormat) {
                case ma_format_u8:
                    pFFmpeg->format = ma_format_u8;
                    pFFmpeg->avFormat = AV_SAMPLE_FMT_U8;
                    pFFmpeg->bitsPerSample = 1;
                    break;
                case ma_format_s16:
                    pFFmpeg->format = ma_format_s16;
                    pFFmpeg->avFormat = AV_SAMPLE_FMT_S16;
                    pFFmpeg->bitsPerSample = 2;
                    break;
                case ma_format_s32:
                    pFFmpeg->format = ma_format_s32;
                    pFFmpeg->avFormat = AV_SAMPLE_FMT_S32;
                    pFFmpeg->bitsPerSample = 4;
                    break;
                default:
                    break;
            }
        }
    }
    #endif

    dataSourceConfig = ma_data_source_config_init();
    dataSourceConfig.vtable = &g_ma_ffmpeg_ds_vtable;

    result = ma_data_source_init(&dataSourceConfig, &pFFmpeg->ds);
    if (result != MA_SUCCESS) {
        return result;
    }

    return MA_SUCCESS;
}


MA_API ma_result ma_ffmpeg_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, ma_ffmpeg_length_proc onGetLength, ma_bool32 isSeekable, void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_ffmpeg *pFFmpeg) {
    ma_result result;

    (void)pAllocationCallbacks;
    (void)pConfig;

    if (pFFmpeg == NULL || onRead == NULL || (isSeekable && (onSeek == NULL || onTell == NULL))) {
        return MA_INVALID_ARGS;
    }

    result = ma_ffmpeg_init_internal(pConfig, pFFmpeg);
    if (result != MA_SUCCESS) {
        return result;
    }

    pFFmpeg->onRead = onRead;
    pFFmpeg->onSeek = onSeek;
    pFFmpeg->onTell = onTell;
    pFFmpeg->onGetLength = onGetLength;
    pFFmpeg->pReadSeekTellUserData = pReadSeekTellUserData;
    pFFmpeg->isSeekable = isSeekable;

    #if !defined(MA_NO_FFMPEG)
    {
        ma_ffmpeg_queue_init(&pFFmpeg->queue); // 一応
        pFFmpeg->formatCtx = avformat_alloc_context();
        if (!pFFmpeg->formatCtx) {
            return MA_ERROR;
        }
        const size_t buf_size = 1024;
        unsigned char* buffer = (unsigned char*)av_malloc(buf_size);
        if (!buffer) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        AVIOContext* avio_ctx = avio_alloc_context(buffer, buf_size, 0, pFFmpeg, ma_ffmpeg_avio_callback__read, NULL, ma_ffmpeg_avio_callback__seek);
        if (!avio_ctx) {
            av_free(buffer);
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        pFFmpeg->formatCtx->pb = avio_ctx;
        avio_ctx->seekable = pFFmpeg->isSeekable ? AVIO_SEEKABLE_NORMAL : 0;
        pFFmpeg->avioCtx = avio_ctx;
        AVDictionary* options = NULL;
        av_dict_set(&options, "fflags", "fastseek+discardcorrupt", 0);
        av_dict_set(&options, "ignore_io_errors", "1", 0);
        av_dict_set_int(&options, "probesize", 1024*1024, 0); // 限制探测大小
        av_dict_set_int(&options, "max_analyze_duration", 1*AV_TIME_BASE, 0); // 限制分析时间1秒
        int open_result = avformat_open_input(&pFFmpeg->formatCtx, NULL, NULL, &options);
        if (open_result < 0) {
            av_dict_free(&options);
            ma_ffmpeg_free_avio(pFFmpeg);
			avformat_free_context(pFFmpeg->formatCtx);
            pFFmpeg->formatCtx = NULL;
            return MA_ERROR;
        }
        av_dict_free(&options);
        for (unsigned int i = 0; i < pFFmpeg->formatCtx->nb_streams; i++) {
            AVStream* stream = pFFmpeg->formatCtx->streams[i];
            if (stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
                stream->discard = AVDISCARD_ALL;
            }
        }

        int stream_info_result = avformat_find_stream_info(pFFmpeg->formatCtx, NULL);
        if (stream_info_result < 0) {
            ma_ffmpeg_close_input(pFFmpeg);
            return MA_ERROR;
        }

        for (unsigned int i = 0; i < pFFmpeg->formatCtx->nb_streams; i++) {
            AVStream* stream = pFFmpeg->formatCtx->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                pFFmpeg->stream = stream;
                break;
            }
        }

        if (!pFFmpeg->stream) {
            ma_ffmpeg_close_input(pFFmpeg);
            return MA_ERROR;
        }
        const AVCodec* codec = avcodec_find_decoder(pFFmpeg->stream->codecpar->codec_id);
        if (!codec) {
            ma_ffmpeg_close_input(pFFmpeg);
            return MA_ERROR;
        }
        pFFmpeg->codecCtx = avcodec_alloc_context3(codec);
        if (!pFFmpeg->codecCtx) {
            ma_ffmpeg_close_input(pFFmpeg);
            return MA_ERROR;
        }
        int params_result = avcodec_parameters_to_context(pFFmpeg->codecCtx, pFFmpeg->stream->codecpar);
        if (params_result < 0) {
            ma_ffmpeg_close_input(pFFmpeg);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        int codec_open_result = avcodec_open2(pFFmpeg->codecCtx, codec, NULL);
        if (codec_open_result < 0) {
            ma_ffmpeg_close_input(pFFmpeg);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        pFFmpeg->frame = av_frame_alloc();
        if (!pFFmpeg->frame) {
            ma_ffmpeg_close_input(pFFmpeg);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        pFFmpeg->packet = av_packet_alloc();
        if (!pFFmpeg->packet) {
            ma_ffmpeg_close_input(pFFmpeg);
            avcodec_free_context(&pFFmpeg->codecCtx);
            av_frame_free(&pFFmpeg->frame);
            return MA_ERROR;
        }
		// 初始化互斥锁
		ma_result result = ma_mutex_init(&pFFmpeg->lock);
		if (result != MA_SUCCESS) {
			av_packet_free(&pFFmpeg->packet);
			av_frame_free(&pFFmpeg->frame);
			avcodec_free_context(&pFFmpeg->codecCtx);
			ma_ffmpeg_close_input(pFFmpeg);
			return result;
		}
        pFFmpeg->eofReached = MA_FALSE;
        pFFmpeg->swrBuf = NULL;
        pFFmpeg->swrBufSize = 0;
        return MA_SUCCESS;
    }
    #else
    {
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}


MA_API void ma_ffmpeg_uninit(ma_ffmpeg *pFFmpeg, const ma_allocation_callbacks *pAllocationCallbacks) {
    if (!pFFmpeg) {
        return;
    }

    (void)pAllocationCallbacks;

    #if !defined(MA_NO_FFMPEG)
    {
        swr_free(&pFFmpeg->swrCtx);
        av_packet_unref(pFFmpeg->packet);
        av_frame_free(&pFFmpeg->frame);
        avcodec_free_context(&pFFmpeg->codecCtx);
        ma_ffmpeg_close_input(pFFmpeg);
        ma_ffmpeg_queue_uninit(&pFFmpeg->queue);
        // 释放重采样缓冲区
        if (pFFmpeg->swrBuf) {
            av_buffer_unref(&pFFmpeg->swrBuf);  // 安全减少引用计数
            pFFmpeg->swrBuf = NULL;             // 防止悬垂指针
            pFFmpeg->swrBufSize = 0;
        }
		ma_mutex_uninit(&pFFmpeg->lock);
    }
    #else
    {
        MA_ASSERT(MA_FALSE);
    }
    #endif

    ma_data_source_uninit(&pFFmpeg->ds);
}


MA_API ma_result ma_ffmpeg_read_pcm_frames(ma_ffmpeg *pFFmpeg, void *pFramesOut, ma_uint64 frameCount, ma_uint64 *pFramesRead) {
    if (pFramesRead) {
        *pFramesRead = 0;
    }
    if (!frameCount || !pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FFMPEG)
    {
		 ma_mutex_lock(&pFFmpeg->lock);  // 加锁
        if (pFFmpeg->eofReached) { // 已到 EOF 直接返回
            ma_mutex_unlock(&pFFmpeg->lock);
            if (pFramesRead) *pFramesRead = 0;
            return MA_AT_END;
        }
        size_t bytesCount = frameCount * pFFmpeg->bitsPerSample * pFFmpeg->codecCtx->ch_layout.nb_channels;
        ma_result ret;
		ma_result finalResult = MA_SUCCESS;
        while (pFFmpeg->queue.size < bytesCount) {
            ret = decode_one_cycle(pFFmpeg);
            if (ret == MA_AT_END){
                finalResult = MA_AT_END;
                pFFmpeg->eofReached = MA_TRUE; // 设置永久标志
				break;
			} else if (ret != MA_SUCCESS) {
				ma_mutex_unlock(&pFFmpeg->lock);
				return ret;
			}
        }

        size_t readBytes;
        ma_result readResult = ma_ffmpeg_queue_read(&pFFmpeg->queue, pFramesOut, bytesCount, &readBytes);
        if (readResult != MA_SUCCESS) {
            ma_mutex_unlock(&pFFmpeg->lock);
            return finalResult == MA_AT_END ? MA_AT_END : readResult;
        }
		ma_uint64 framesRead = readBytes / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels;
        if (pFramesRead) {
            *pFramesRead = framesRead;
        }
		ma_mutex_unlock(&pFFmpeg->lock);  // 解锁
        return (framesRead > 0) ? MA_SUCCESS : finalResult;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);

        (void)pFramesOut;
        (void)frameCount;
        (void)pFramesRead;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}



MA_API ma_result ma_ffmpeg_seek_to_pcm_frame(ma_ffmpeg *pFFmpeg, ma_uint64 frameIndex) {
    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FFMPEG)
    {
		ma_result result = MA_SUCCESS;
		// 加锁
		ma_mutex_lock(&pFFmpeg->lock);
        pFFmpeg->eofReached = MA_FALSE;
        ma_uint64 length = 0;
        result = ma_ffmpeg_get_length_in_pcm_frames(pFFmpeg, &length);
        if (result != MA_SUCCESS || !pFFmpeg->formatCtx || !pFFmpeg->codecCtx || !pFFmpeg->stream || frameIndex > length) {
			result = MA_INVALID_ARGS;
			goto done;
        }

        AVRational avr = { 1, pFFmpeg->codecCtx->sample_rate };
        int64_t timestamp = av_rescale_q(frameIndex, avr, pFFmpeg->stream->time_base);
        if (av_seek_frame(pFFmpeg->formatCtx, pFFmpeg->stream->index, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
			result = MA_ERROR;
			goto done;
        }

        avcodec_flush_buffers(pFFmpeg->codecCtx);

		ma_ffmpeg_queue_clear(&pFFmpeg->queue);

        // AVSEEK_FLAG_BACKWARDをおいたので、少し前サンプルまで拾ってしまう
        // 一回デコードして、queueからいらない分を抜いて補正する
        ma_result ret;
		do {
			ret = decode_one_cycle(pFFmpeg);
			if (ret != MA_SUCCESS) {
				if (ret == MA_AT_END) break;
                result = ret;
                goto done;
			}
		} while (pFFmpeg->cursor < frameIndex);
        ma_uint64 queueFrames = pFFmpeg->queue.size / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels;
        ma_uint64 queueStart = (pFFmpeg->cursor > queueFrames) ? (pFFmpeg->cursor - queueFrames) : 0;
        if (frameIndex > queueStart) {
            ma_uint64 popFrames = frameIndex - queueStart;
            size_t popBytes = (size_t)(popFrames * pFFmpeg->bitsPerSample * pFFmpeg->codecCtx->ch_layout.nb_channels);
            ma_ffmpeg_queue_read(&pFFmpeg->queue, NULL, popBytes, NULL);
        }

    done:
		// 解锁
		ma_mutex_unlock(&pFFmpeg->lock);
        return result;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);

        (void)frameIndex;

        return MA_NOT_IMPLEMENTED;
    }
    #endif
}



MA_API ma_result ma_ffmpeg_get_data_format(ma_ffmpeg *pFFmpeg, ma_format *pFormat, ma_uint32 *pChannels, ma_uint32 *pSampleRate, ma_channel *pChannelMap, size_t channelMapCap) {
    if (pFormat) {
        *pFormat = ma_format_unknown;
    }
    if (pChannels) {
        *pChannels = 0;
    }
    if (pSampleRate) {
        *pSampleRate = 0;
    }
    if (pChannelMap) {
        MA_ZERO_MEMORY(pChannelMap, sizeof(*pChannelMap) * channelMapCap);
    }

    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    if (pFormat) {
        *pFormat = pFFmpeg->format;
    }

    #if !defined(MA_NO_FFMPEG)
    {
        if (!pFFmpeg->codecCtx) {
            return MA_INVALID_ARGS;
        }
        if (pChannels) {
            *pChannels = pFFmpeg->codecCtx->ch_layout.nb_channels;
        }
        if (pSampleRate) {
            *pSampleRate = pFFmpeg->codecCtx->sample_rate;
        }
        if (pChannelMap) {
            ma_channel_map_init_standard(ma_standard_channel_map_vorbis, pChannelMap, channelMapCap, pFFmpeg->codecCtx->ch_layout.nb_channels);
        }
        return MA_SUCCESS;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif

}


MA_API ma_result ma_ffmpeg_get_cursor_in_pcm_frames(ma_ffmpeg* pFFmpeg, ma_uint64* pCursor) {
    if (!pCursor) {
        return MA_INVALID_ARGS;
    }

    *pCursor = 0;

    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FFMPEG)
    {
        if (!pFFmpeg->codecCtx) {
            return MA_INVALID_ARGS;
        }

        // queueに積んである分だけcursorがズレてる
        ma_uint64 queueFrames = pFFmpeg->queue.size / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels;
        *pCursor = (pFFmpeg->cursor > queueFrames) ? (pFFmpeg->cursor - queueFrames) : 0;

        return MA_SUCCESS;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif

}


MA_API ma_result ma_ffmpeg_get_length_in_pcm_frames(ma_ffmpeg *pFFmpeg, ma_uint64 *pLength) {
    if (!pLength) {
        return MA_INVALID_ARGS;
    }
    *pLength = 0;

    if (!pFFmpeg) {
        return MA_INVALID_ARGS;
    }

    #if !defined(MA_NO_FFMPEG)
    {
        // 参数检查
        if (!pFFmpeg || !pFFmpeg->formatCtx || !pFFmpeg->stream || !pLength) {
            return MA_INVALID_ARGS;
        }

        AVFormatContext *fmt_ctx = pFFmpeg->formatCtx;
        AVStream *stream = pFFmpeg->stream;
        AVCodecParameters *codecpar = stream->codecpar;

        // 方法 1：优先使用 duration × sample_rate
        if (stream->duration != AV_NOPTS_VALUE && codecpar->sample_rate > 0) {
            *pLength = (ma_uint64)(av_q2d(stream->time_base) * stream->duration * codecpar->sample_rate);
            return MA_SUCCESS;
        }

	    // 流时长无效时，回退到容器时长
        if (fmt_ctx->duration != AV_NOPTS_VALUE && codecpar->sample_rate > 0) {
            double duration_seconds = (double)fmt_ctx->duration / AV_TIME_BASE;
            *pLength = (ma_uint64)(duration_seconds * codecpar->sample_rate);
            return MA_SUCCESS;
        }

        // 方法 2：基于 nb_frames 和每帧采样数
        ma_uint64 samples_per_frame = 0;
        switch (codecpar->codec_id) {
            case AV_CODEC_ID_AAC:  samples_per_frame = 1024; break;
            case AV_CODEC_ID_MP3:  samples_per_frame = 1152; break;
            case AV_CODEC_ID_FLAC: samples_per_frame = codecpar->frame_size; break;
            case AV_CODEC_ID_ALAC: samples_per_frame = 4096; break;
            case AV_CODEC_ID_OPUS: samples_per_frame = 120 * codecpar->sample_rate / 1000; break;
            default: samples_per_frame = 0;
        }

        if (samples_per_frame > 0 && stream->nb_frames > 0) {
            *pLength = stream->nb_frames * samples_per_frame;
            return MA_SUCCESS;
        }

        return MA_ERROR;
    }
    #else
    {
        MA_ASSERT(MA_FALSE);
        return MA_NOT_IMPLEMENTED;
    }
    #endif
}

#endif // MA_IMPLEMENTATION

