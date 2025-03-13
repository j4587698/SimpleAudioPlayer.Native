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

// queue
typedef struct ma_ffmpeg_queue_node
{
    void* data;
    size_t size;
    struct ma_ffmpeg_queue_node* next;
} ma_ffmpeg_queue_node;

typedef struct
{
    size_t size;
    size_t cursor;
    ma_ffmpeg_queue_node* front;
    ma_ffmpeg_queue_node* rear;
} ma_ffmpeg_queue;

static void ma_ffmpeg_queue_init(ma_ffmpeg_queue* queue);
static void ma_ffmpeg_queue_clear(ma_ffmpeg_queue* queue);
static ma_result ma_ffmpeg_queue_enqueue(ma_ffmpeg_queue* queue, void* data, size_t size);
static ma_result ma_ffmpeg_queue_read(ma_ffmpeg_queue* queue, void* data, size_t size, size_t* readSize);
#endif // !MA_NO_FFMPEG


typedef struct
{
    ma_data_source_base ds;
    ma_read_proc onRead;
    ma_seek_proc onSeek;
    ma_tell_proc onTell;
    void* pReadSeekTellUserData;
    ma_format format;
#if !defined(MA_NO_FFMPEG)
    uint8_t bitsPerSample;
    enum AVSampleFormat avFormat;
    AVFormatContext* formatCtx;
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

MA_API ma_result ma_ffmpeg_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void* pReadSeekTellUserData, const ma_decoding_backend_config* pConfig, const ma_allocation_callbacks* pAllocationCallbacks, ma_ffmpeg* pFFmpeg);
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
    size_t bytesToRead;
	
    if (!pFFmpeg->onRead || !buf_size) {
        return -1;
    }

    result = pFFmpeg->onRead(pFFmpeg->pReadSeekTellUserData, buf, buf_size, &bytesToRead);
    if (result == MA_AT_END) {
        return 0;
    }
    return (result == MA_SUCCESS) ? bytesToRead : -1;
}

static int64_t ma_ffmpeg_avio_callback__seek(void* opaque, int64_t offset, int whence) {
    ma_ffmpeg* pFFmpeg = (ma_ffmpeg*)opaque;
    ma_result result;
    ma_seek_origin origin;
    ma_int64 cursor;

    if (!pFFmpeg->onSeek || !pFFmpeg->onTell)
        return AVERROR(EINVAL);

    if (whence == SEEK_SET) {
        origin = ma_seek_origin_start;
    } else if (whence == SEEK_END) {
        origin = ma_seek_origin_end;
    } else if (whence == SEEK_CUR) {
        origin = ma_seek_origin_current;
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
// avio callback

// queue
static void ma_ffmpeg_queue_init(ma_ffmpeg_queue* queue) {
    queue->size = 0;
    queue->cursor = 0;
    queue->front = NULL;
    queue->rear = NULL;
}

static void ma_ffmpeg_queue_clear(ma_ffmpeg_queue* queue) {
    ma_ffmpeg_queue_node* node;

    while(queue->front) {
        node = queue->front;
        queue->front = node->next;
        free(node->data);
        free(node);
    }

    queue->size = 0;
    queue->cursor = 0;
    queue->front = NULL;
    queue->rear = NULL;
}

static ma_result ma_ffmpeg_queue_enqueue(ma_ffmpeg_queue* queue, void* data, size_t size) {
    ma_ffmpeg_queue_node* node = (ma_ffmpeg_queue_node*)malloc(sizeof(ma_ffmpeg_queue_node));

    if (!node) {
        return MA_OUT_OF_MEMORY;
    }
    node->data = malloc(size);
    if (!node->data) {
        free(node);
        return MA_OUT_OF_MEMORY;
    }

    memcpy(node->data, data, size);
    node->size = size;
    node->next = NULL;

    if (!queue->rear) {
        queue->front = node;
        queue->rear = node;
    } else {
        queue->rear->next = node;
        queue->rear = node;
    }

    queue->size += size;

    return MA_SUCCESS;
}

static ma_result ma_ffmpeg_queue_read(ma_ffmpeg_queue* queue, void* data, size_t size, size_t* readSize) {
    size_t readed = 0;
    if (readSize)
        *readSize = 0;

    if (!queue->front)
        return MA_ERROR;

    while(queue->front) {
        ma_ffmpeg_queue_node* node = queue->front;

        if ((node->size - queue->cursor) + readed > size) {
            // 途中まで読む
            size_t rSize = size - readed;

            if (data)
                memcpy((ma_uint8*)data + readed, (ma_uint8*)node->data + queue->cursor, rSize);

            queue->size -= rSize;
            queue->cursor = queue->cursor + rSize;
            readed += rSize;

            break;
        } else {
            size_t rSize = node->size - queue->cursor;
            // 完全に読んで、抜ける

            if (data)
                memcpy((ma_uint8*)data + readed, (ma_uint8*)node->data + queue->cursor, rSize);

            queue->front = node->next;
            queue->size -= rSize;
            queue->cursor = 0;
            readed += rSize;

            free(node->data);
            free(node);

            if (queue->front == NULL)
                queue->rear = NULL;
        }
    }

    if (readSize)
        *readSize = readed;

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
            ma_ffmpeg_queue_enqueue(&pFFmpeg->queue, swr_data, data_size);
        } else {
            // 直接入队原始数据
            size_t data_size = frame->nb_samples * pFFmpeg->bitsPerSample
                             * pFFmpeg->codecCtx->ch_layout.nb_channels;
            ma_ffmpeg_queue_enqueue(&pFFmpeg->queue, frame->extended_data[0], data_size);
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


MA_API ma_result ma_ffmpeg_init(ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_ffmpeg *pFFmpeg) {
    ma_result result;

    (void)pAllocationCallbacks;
    (void)pConfig;

    result = ma_ffmpeg_init_internal(pConfig, pFFmpeg);
    if (result != MA_SUCCESS) {
        return result;
    }

    if (onRead == NULL || onSeek == NULL || onTell == NULL) {
        return MA_INVALID_ARGS;
    }

    pFFmpeg->onRead = onRead;
    pFFmpeg->onSeek = onSeek;
    pFFmpeg->onTell = onTell;
    pFFmpeg->pReadSeekTellUserData = pReadSeekTellUserData;

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
        AVDictionary* options = NULL;
        av_dict_set_int(&options, "scan_all_pmts", 0, AV_DICT_MATCH_CASE);
        av_dict_set_int(&options, "probesize", 1024*1024, 0); // 限制探测大小
        av_dict_set_int(&options, "max_analyze_duration", 1*AV_TIME_BASE, 0); // 限制分析时间1秒
        if (avformat_open_input(&pFFmpeg->formatCtx, NULL, NULL, &options) < 0) {
            av_dict_free(&options);
            av_freep(&avio_ctx->buffer);  // 释放AVIOContext的buffer
			avio_context_free(&avio_ctx); // 显式释放AVIOContext
			avformat_free_context(pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        av_dict_free(&options);
        for (unsigned int i = 0; i < pFFmpeg->formatCtx->nb_streams; i++) {
            AVStream* stream = pFFmpeg->formatCtx->streams[i];
            if (stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
                stream->discard = AVDISCARD_ALL;
            }
        }

        AVDictionary* find_stream_opts = NULL;
        av_dict_set(&find_stream_opts, "enable_streams", "audio", 0);
        if (avformat_find_stream_info(pFFmpeg->formatCtx, &find_stream_opts) < 0) {
            avformat_close_input(&pFFmpeg->formatCtx);
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
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        const AVCodec* codec = avcodec_find_decoder(pFFmpeg->stream->codecpar->codec_id);
        if (!codec) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        pFFmpeg->codecCtx = avcodec_alloc_context3(codec);
        if (!pFFmpeg->codecCtx) {
            avformat_close_input(&pFFmpeg->formatCtx);
            return MA_ERROR;
        }
        if (avcodec_parameters_to_context(pFFmpeg->codecCtx, pFFmpeg->stream->codecpar) < 0) {
            avformat_close_input(&pFFmpeg->formatCtx);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        if (avcodec_open2(pFFmpeg->codecCtx, codec, NULL) < 0) {
            avformat_close_input(&pFFmpeg->formatCtx);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        pFFmpeg->frame = av_frame_alloc();
        if (!pFFmpeg->frame) {
            avformat_close_input(&pFFmpeg->formatCtx);
            avcodec_free_context(&pFFmpeg->codecCtx);
            return MA_ERROR;
        }
        pFFmpeg->packet = av_packet_alloc();
        if (!pFFmpeg->packet) {
            avformat_close_input(&pFFmpeg->formatCtx);
            avcodec_free_context(&pFFmpeg->codecCtx);
            av_frame_free(&pFFmpeg->frame);
            return MA_ERROR;
        }
		// 初始化互斥锁
		ma_result result = ma_mutex_init(&pFFmpeg->lock);
		if (result != MA_SUCCESS) {
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
        avformat_close_input(&pFFmpeg->formatCtx); // AVIOContextもこれで解放される
        ma_ffmpeg_queue_clear(&pFFmpeg->queue);
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
			}
        }

        size_t readBytes;
        ma_ffmpeg_queue_read(&pFFmpeg->queue, pFramesOut, bytesCount, &readBytes);
        *pFramesRead = readBytes / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels;
		ma_mutex_unlock(&pFFmpeg->lock);  // 解锁
        return (*pFramesRead > 0) ? MA_SUCCESS : finalResult;
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
		// 加锁
		ma_mutex_lock(&pFFmpeg->lock);
        pFFmpeg->eofReached = MA_FALSE;
        ma_uint64 length;
        ma_ffmpeg_get_length_in_pcm_frames(pFFmpeg, &length);
        if (!pFFmpeg->formatCtx || !pFFmpeg->codecCtx || !pFFmpeg->stream || length < frameIndex) {
            return MA_INVALID_ARGS;
        }
        AVRational avr = { 1, pFFmpeg->codecCtx->sample_rate };
        int64_t timestamp = av_rescale_q(frameIndex, avr, pFFmpeg->stream->time_base);
        if (av_seek_frame(pFFmpeg->formatCtx, pFFmpeg->stream->index, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
            return MA_ERROR;
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
				return ret;
			}
		} while (pFFmpeg->cursor < frameIndex);
        int popBytes = (frameIndex - (pFFmpeg->cursor - (pFFmpeg->queue.size / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels))) * pFFmpeg->bitsPerSample * pFFmpeg->codecCtx->ch_layout.nb_channels;
        ma_ffmpeg_queue_read(&pFFmpeg->queue, NULL, popBytes, NULL);
		
		// 解锁
		ma_mutex_unlock(&pFFmpeg->lock);
        return MA_SUCCESS;
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
        *pCursor = pFFmpeg->cursor - (pFFmpeg->queue.size / pFFmpeg->bitsPerSample / pFFmpeg->codecCtx->ch_layout.nb_channels);

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
        if (!pFFmpeg->stream) {
            return MA_INVALID_ARGS;
        }

        if (pFFmpeg->stream->nb_frames) {
            *pLength = pFFmpeg->stream->nb_frames;
            return MA_SUCCESS;
        }
        if (pFFmpeg->stream->duration != AV_NOPTS_VALUE && pFFmpeg->stream->codecpar->sample_rate != 0) {
            *pLength =
                (pFFmpeg->stream->duration * pFFmpeg->stream->codecpar->sample_rate * pFFmpeg->stream->time_base.num) /
                pFFmpeg->stream->time_base.den;
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

