#include "audio_recorder.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if !defined(MA_NO_FFMPEG)
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#endif

#define AUDIO_RECORD_BUFFER_SECONDS 4
#define AUDIO_RECORD_CHUNK_FRAMES 4096
#define AUDIO_RECORD_MIN_BUFFER_FRAMES 8192
#define AUDIO_RECORD_DEFAULT_BITRATE 128000

struct AudioRecorderContext {
    ma_device device;
    ma_mutex buffer_mutex;
    ma_event write_event;
    ma_thread write_thread;
    ma_thread simulate_thread;
    bool device_initialized;
    bool write_thread_started;
    bool simulate_thread_started;
    bool simulate_capture;
    bool stop_requested;
    bool recording;
    bool encoder_initialized;
    bool encoder_finalized;
    ma_result result;
    ma_format format;
    ma_uint32 channels;
    ma_uint32 sample_rate;
    ma_uint32 bytes_per_frame;
    ma_uint8* pcm_buffer;
    ma_uint64 pcm_buffer_capacity_frames;
    ma_uint64 pcm_buffer_read_frame;
    ma_uint64 pcm_buffer_write_frame;
    ma_uint64 pcm_buffer_available_frames;
    ma_uint8* write_scratch;
    ma_uint64 write_scratch_capacity_frames;
    ma_uint8* simulate_scratch;
    ma_uint64 simulate_scratch_capacity_frames;
    ma_uint64 captured_frames;
    ma_uint64 dropped_frames;
    AudioRecorderContainer container;
    AudioRecorderWriteCallback output_write;
    AudioRecorderSeekCallback output_seek;
    void* output_userdata;
    bool output_callback;
    ma_encoder wav_encoder;
    bool wav_encoder_initialized;
#if !defined(MA_NO_FFMPEG)
    AVFormatContext* format_ctx;
    AVIOContext* avio_ctx;
    ma_uint8* avio_buffer;
    bool custom_avio;
    AVCodecContext* codec_ctx;
    AVStream* stream;
    AVPacket* packet;
    SwrContext* swr_ctx;
    AVAudioFifo* fifo;
    int frame_size;
    int64_t next_pts;
#endif
};

static ma_uint64 audio_recorder_buffer_space_locked(AudioRecorderContext* ctx)
{
    return ctx->pcm_buffer_capacity_frames - ctx->pcm_buffer_available_frames;
}

static void audio_recorder_buffer_reset_locked(AudioRecorderContext* ctx)
{
    ctx->pcm_buffer_read_frame = 0;
    ctx->pcm_buffer_write_frame = 0;
    ctx->pcm_buffer_available_frames = 0;
}

static ma_uint64 audio_recorder_buffer_write_locked(AudioRecorderContext* ctx, const void* pInput, ma_uint64 frameCount)
{
    if (!ctx->pcm_buffer || ctx->bytes_per_frame == 0 || frameCount == 0) {
        return 0;
    }

    ma_uint64 framesToWrite = frameCount < audio_recorder_buffer_space_locked(ctx)
        ? frameCount
        : audio_recorder_buffer_space_locked(ctx);
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

static ma_uint64 audio_recorder_buffer_read_locked(AudioRecorderContext* ctx, void* pOutput, ma_uint64 frameCount)
{
    if (!ctx->pcm_buffer || ctx->bytes_per_frame == 0 || frameCount == 0) {
        return 0;
    }

    ma_uint64 framesToRead = frameCount < ctx->pcm_buffer_available_frames
        ? frameCount
        : ctx->pcm_buffer_available_frames;
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

static ma_result audio_recorder_buffer_init(AudioRecorderContext* ctx, ma_format format, ma_uint32 channels, ma_uint32 sampleRate)
{
    ctx->bytes_per_frame = ma_get_bytes_per_frame(format, channels);
    if (ctx->bytes_per_frame == 0 || channels == 0 || sampleRate == 0) {
        return MA_INVALID_ARGS;
    }

    ma_uint64 capacityFrames = (ma_uint64)sampleRate * AUDIO_RECORD_BUFFER_SECONDS;
    if (capacityFrames < AUDIO_RECORD_MIN_BUFFER_FRAMES) {
        capacityFrames = AUDIO_RECORD_MIN_BUFFER_FRAMES;
    }

    ctx->pcm_buffer = (ma_uint8*)ma_malloc((size_t)(capacityFrames * ctx->bytes_per_frame), NULL);
    if (!ctx->pcm_buffer) {
        return MA_OUT_OF_MEMORY;
    }

    ctx->write_scratch_capacity_frames = AUDIO_RECORD_CHUNK_FRAMES;
    ctx->write_scratch = (ma_uint8*)ma_malloc((size_t)(ctx->write_scratch_capacity_frames * ctx->bytes_per_frame), NULL);
    if (!ctx->write_scratch) {
        ma_free(ctx->pcm_buffer, NULL);
        ctx->pcm_buffer = NULL;
        return MA_OUT_OF_MEMORY;
    }

    ctx->simulate_scratch_capacity_frames = AUDIO_RECORD_CHUNK_FRAMES;
    ctx->simulate_scratch = (ma_uint8*)ma_malloc((size_t)(ctx->simulate_scratch_capacity_frames * ctx->bytes_per_frame), NULL);
    if (!ctx->simulate_scratch) {
        ma_free(ctx->write_scratch, NULL);
        ctx->write_scratch = NULL;
        ma_free(ctx->pcm_buffer, NULL);
        ctx->pcm_buffer = NULL;
        return MA_OUT_OF_MEMORY;
    }
    memset(ctx->simulate_scratch, 0, (size_t)(ctx->simulate_scratch_capacity_frames * ctx->bytes_per_frame));

    ctx->pcm_buffer_capacity_frames = capacityFrames;
    audio_recorder_buffer_reset_locked(ctx);
    return MA_SUCCESS;
}

static void audio_recorder_buffer_uninit(AudioRecorderContext* ctx)
{
    ma_free(ctx->pcm_buffer, NULL);
    ctx->pcm_buffer = NULL;
    ma_free(ctx->write_scratch, NULL);
    ctx->write_scratch = NULL;
    ma_free(ctx->simulate_scratch, NULL);
    ctx->simulate_scratch = NULL;
    ctx->pcm_buffer_capacity_frames = 0;
    ctx->write_scratch_capacity_frames = 0;
    ctx->simulate_scratch_capacity_frames = 0;
    ctx->bytes_per_frame = 0;
    audio_recorder_buffer_reset_locked(ctx);
}

static ma_result audio_recorder_write_output(AudioRecorderContext* ctx, const void* buffer, size_t bytesToWrite, size_t* bytesWritten)
{
    if (bytesWritten) {
        *bytesWritten = 0;
    }
    if (!ctx || !ctx->output_write || (!buffer && bytesToWrite > 0)) {
        return MA_INVALID_ARGS;
    }

    size_t written = 0;
    ma_result result = ctx->output_write(ctx->output_userdata, buffer, bytesToWrite, &written);
    if (bytesWritten) {
        *bytesWritten = written;
    }
    if (result != MA_SUCCESS) {
        return result;
    }
    if (written != bytesToWrite) {
        return MA_IO_ERROR;
    }

    return MA_SUCCESS;
}

static ma_result audio_recorder_seek_output(AudioRecorderContext* ctx, ma_int64 offset, int origin, ma_int64* cursor)
{
    if (cursor) {
        *cursor = 0;
    }
    if (!ctx || !ctx->output_seek) {
        return MA_NOT_IMPLEMENTED;
    }

    int64_t next = 0;
    ma_result result = ctx->output_seek(ctx->output_userdata, (int64_t)offset, origin, &next);
    if (cursor) {
        *cursor = (ma_int64)next;
    }
    return result;
}

static ma_result audio_recorder_wav_write(ma_encoder* encoder, const void* buffer, size_t bytesToWrite, size_t* bytesWritten)
{
    return audio_recorder_write_output((AudioRecorderContext*)encoder->pUserData, buffer, bytesToWrite, bytesWritten);
}

static ma_result audio_recorder_wav_seek(ma_encoder* encoder, ma_int64 offset, ma_seek_origin origin)
{
    return audio_recorder_seek_output((AudioRecorderContext*)encoder->pUserData, offset, (int)origin, NULL);
}

static ma_result audio_recorder_init_wav_encoder(AudioRecorderContext* ctx, const char* outputPath, ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 bitRate)
{
    (void)bitRate;

    if (channels == 0 || sampleRate == 0) {
        return MA_INVALID_ARGS;
    }

    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, format, channels, sampleRate);
    ma_result result = ctx->output_callback
        ? ma_encoder_init(audio_recorder_wav_write, audio_recorder_wav_seek, ctx, &config, &ctx->wav_encoder)
        : ma_encoder_init_file(outputPath, &config, &ctx->wav_encoder);
    if (result != MA_SUCCESS) {
        return result;
    }

    ctx->container = AUDIO_RECORDER_CONTAINER_WAV;
    ctx->wav_encoder_initialized = true;
    ctx->encoder_initialized = true;
    ctx->encoder_finalized = false;
    return MA_SUCCESS;
}

static ma_result audio_recorder_encode_wav_pcm(AudioRecorderContext* ctx, const void* pInput, ma_uint64 frameCount)
{
    if (!ctx->wav_encoder_initialized || !pInput || frameCount == 0) {
        return MA_SUCCESS;
    }

    ma_uint64 framesWritten = 0;
    ma_result result = ma_encoder_write_pcm_frames(&ctx->wav_encoder, pInput, frameCount, &framesWritten);
    if (result != MA_SUCCESS) {
        return result;
    }
    if (framesWritten != frameCount) {
        return MA_ERROR;
    }

    return MA_SUCCESS;
}

static ma_result audio_recorder_encode_raw_pcm(AudioRecorderContext* ctx, const void* pInput, ma_uint64 frameCount)
{
    if (!ctx->output_callback || !pInput || frameCount == 0) {
        return MA_SUCCESS;
    }

    size_t bytesToWrite = (size_t)(frameCount * ctx->bytes_per_frame);
    size_t bytesWritten = 0;
    return audio_recorder_write_output(ctx, pInput, bytesToWrite, &bytesWritten);
}

static ma_result audio_recorder_finalize_wav_encoder(AudioRecorderContext* ctx)
{
    if (!ctx->wav_encoder_initialized) {
        ctx->encoder_finalized = true;
        return MA_SUCCESS;
    }

    ma_encoder_uninit(&ctx->wav_encoder);
    ctx->wav_encoder_initialized = false;
    ctx->encoder_finalized = true;
    return MA_SUCCESS;
}

static void audio_recorder_uninit_wav_encoder(AudioRecorderContext* ctx)
{
    if (ctx->wav_encoder_initialized) {
        ma_encoder_uninit(&ctx->wav_encoder);
        ctx->wav_encoder_initialized = false;
    }
}

#if !defined(MA_NO_FFMPEG)
static enum AVSampleFormat audio_recorder_map_sample_format(ma_format format)
{
    switch (format) {
    case ma_format_u8:
        return AV_SAMPLE_FMT_U8;
    case ma_format_s16:
        return AV_SAMPLE_FMT_S16;
    case ma_format_s32:
        return AV_SAMPLE_FMT_S32;
    case ma_format_f32:
        return AV_SAMPLE_FMT_FLT;
    default:
        return AV_SAMPLE_FMT_NONE;
    }
}

static ma_result audio_recorder_av_error_to_ma(int error)
{
    if (error == AVERROR(ENOMEM)) {
        return MA_OUT_OF_MEMORY;
    }
    if (error == AVERROR(EINVAL)) {
        return MA_INVALID_ARGS;
    }
    return MA_ERROR;
}

static const char* audio_recorder_get_muxer_name(AudioRecorderContainer container)
{
    switch (container) {
    case AUDIO_RECORDER_CONTAINER_M4A:
        return "ipod";
    case AUDIO_RECORDER_CONTAINER_AAC:
        return "adts";
    default:
        return NULL;
    }
}

static const char* audio_recorder_get_output_name(AudioRecorderContainer container)
{
    switch (container) {
    case AUDIO_RECORDER_CONTAINER_M4A:
        return "recording.m4a";
    case AUDIO_RECORDER_CONTAINER_AAC:
        return "recording.aac";
    default:
        return NULL;
    }
}

static enum AVSampleFormat audio_recorder_select_encoder_sample_format(const AVCodec* codec)
{
    const void* configs = NULL;
    int configCount = 0;
    int result = avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &configs, &configCount);
    if (result >= 0 && configs != NULL && configCount > 0) {
        return ((const enum AVSampleFormat*)configs)[0];
    }

    return AV_SAMPLE_FMT_FLTP;
}

static int audio_recorder_avio_write(void* opaque, const uint8_t* buffer, int bufferSize)
{
    if (!opaque || !buffer || bufferSize < 0) {
        return AVERROR(EINVAL);
    }

    size_t bytesWritten = 0;
    ma_result result = audio_recorder_write_output((AudioRecorderContext*)opaque, buffer, (size_t)bufferSize, &bytesWritten);
    if (result != MA_SUCCESS) {
        return AVERROR(EIO);
    }

    return (int)bytesWritten;
}

static int64_t audio_recorder_avio_seek(void* opaque, int64_t offset, int whence)
{
    if (!opaque) {
        return AVERROR(EINVAL);
    }

    ma_int64 cursor = 0;
    ma_result result = audio_recorder_seek_output((AudioRecorderContext*)opaque, (ma_int64)offset, whence, &cursor);
    if (result != MA_SUCCESS) {
        return AVERROR(ENOSYS);
    }

    return (int64_t)cursor;
}

static ma_result audio_recorder_send_frame(AudioRecorderContext* ctx, AVFrame* frame)
{
    int error = avcodec_send_frame(ctx->codec_ctx, frame);
    if (error < 0) {
        return audio_recorder_av_error_to_ma(error);
    }

    for (;;) {
        error = avcodec_receive_packet(ctx->codec_ctx, ctx->packet);
        if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
            return MA_SUCCESS;
        }
        if (error < 0) {
            return audio_recorder_av_error_to_ma(error);
        }

        ctx->packet->stream_index = ctx->stream->index;
        av_packet_rescale_ts(ctx->packet, ctx->codec_ctx->time_base, ctx->stream->time_base);
        error = av_interleaved_write_frame(ctx->format_ctx, ctx->packet);
        av_packet_unref(ctx->packet);
        if (error < 0) {
            return audio_recorder_av_error_to_ma(error);
        }
    }
}

static ma_result audio_recorder_encode_fifo_frame(AudioRecorderContext* ctx, int frameCount)
{
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        return MA_OUT_OF_MEMORY;
    }

    frame->nb_samples = frameCount;
    frame->format = ctx->codec_ctx->sample_fmt;
    frame->sample_rate = ctx->codec_ctx->sample_rate;
    int error = av_channel_layout_copy(&frame->ch_layout, &ctx->codec_ctx->ch_layout);
    if (error < 0) {
        av_frame_free(&frame);
        return audio_recorder_av_error_to_ma(error);
    }

    error = av_frame_get_buffer(frame, 0);
    if (error < 0) {
        av_frame_free(&frame);
        return audio_recorder_av_error_to_ma(error);
    }

    int framesRead = av_audio_fifo_read(ctx->fifo, (void**)frame->extended_data, frameCount);
    if (framesRead < frameCount) {
        av_frame_free(&frame);
        return MA_ERROR;
    }

    frame->pts = ctx->next_pts;
    ctx->next_pts += framesRead;

    ma_result result = audio_recorder_send_frame(ctx, frame);
    av_frame_free(&frame);
    return result;
}

static ma_result audio_recorder_drain_fifo(AudioRecorderContext* ctx, bool flushPartial)
{
    while (av_audio_fifo_size(ctx->fifo) >= ctx->frame_size) {
        ma_result result = audio_recorder_encode_fifo_frame(ctx, ctx->frame_size);
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    if (flushPartial && av_audio_fifo_size(ctx->fifo) > 0) {
        ma_result result = audio_recorder_encode_fifo_frame(ctx, av_audio_fifo_size(ctx->fifo));
        if (result != MA_SUCCESS) {
            return result;
        }
    }

    return MA_SUCCESS;
}

static ma_result audio_recorder_encode_pcm(AudioRecorderContext* ctx, const void* pInput, ma_uint64 frameCount)
{
    if (ctx->container == AUDIO_RECORDER_CONTAINER_PCM) {
        return audio_recorder_encode_raw_pcm(ctx, pInput, frameCount);
    }

    if (ctx->container == AUDIO_RECORDER_CONTAINER_WAV) {
        return audio_recorder_encode_wav_pcm(ctx, pInput, frameCount);
    }

    if (!ctx->encoder_initialized || !pInput || frameCount == 0) {
        return MA_SUCCESS;
    }

    while (frameCount > 0) {
        int inputFrames = frameCount > (ma_uint64)INT_MAX ? INT_MAX : (int)frameCount;
        const ma_uint8* inputBytes = (const ma_uint8*)pInput;
        int outputCapacity = swr_get_out_samples(ctx->swr_ctx, inputFrames);
        if (outputCapacity <= 0) {
            return MA_ERROR;
        }

        uint8_t** convertedData = NULL;
        int convertedLineSize = 0;
        int error = av_samples_alloc_array_and_samples(
            &convertedData,
            &convertedLineSize,
            ctx->codec_ctx->ch_layout.nb_channels,
            outputCapacity,
            ctx->codec_ctx->sample_fmt,
            0);
        if (error < 0) {
            return audio_recorder_av_error_to_ma(error);
        }

        const uint8_t* inputData[1] = { inputBytes };
        int convertedFrames = swr_convert(ctx->swr_ctx, convertedData, outputCapacity, inputData, inputFrames);
        if (convertedFrames < 0) {
            av_freep(&convertedData[0]);
            av_freep(&convertedData);
            return audio_recorder_av_error_to_ma(convertedFrames);
        }

        error = av_audio_fifo_realloc(ctx->fifo, av_audio_fifo_size(ctx->fifo) + convertedFrames);
        if (error < 0) {
            av_freep(&convertedData[0]);
            av_freep(&convertedData);
            return audio_recorder_av_error_to_ma(error);
        }

        int written = av_audio_fifo_write(ctx->fifo, (void**)convertedData, convertedFrames);
        av_freep(&convertedData[0]);
        av_freep(&convertedData);
        if (written < convertedFrames) {
            return MA_ERROR;
        }

        ma_result result = audio_recorder_drain_fifo(ctx, false);
        if (result != MA_SUCCESS) {
            return result;
        }

        pInput = inputBytes + ((size_t)inputFrames * ctx->bytes_per_frame);
        frameCount -= (ma_uint64)inputFrames;
    }

    return MA_SUCCESS;
}

static ma_result audio_recorder_finalize_encoder(AudioRecorderContext* ctx)
{
    if (ctx->container == AUDIO_RECORDER_CONTAINER_PCM) {
        ctx->encoder_finalized = true;
        return MA_SUCCESS;
    }

    if (ctx->container == AUDIO_RECORDER_CONTAINER_WAV) {
        return audio_recorder_finalize_wav_encoder(ctx);
    }

    if (!ctx->encoder_initialized || ctx->encoder_finalized) {
        return MA_SUCCESS;
    }

    ma_result result = audio_recorder_drain_fifo(ctx, true);
    if (result == MA_SUCCESS) {
        result = audio_recorder_send_frame(ctx, NULL);
    }

    int trailerResult = av_write_trailer(ctx->format_ctx);
    if (result == MA_SUCCESS && trailerResult < 0) {
        result = audio_recorder_av_error_to_ma(trailerResult);
    }

    ctx->encoder_finalized = true;
    return result;
}

static void audio_recorder_uninit_encoder(AudioRecorderContext* ctx)
{
    if (ctx->container == AUDIO_RECORDER_CONTAINER_PCM) {
        ctx->encoder_initialized = false;
        ctx->encoder_finalized = false;
        return;
    }

    if (ctx->container == AUDIO_RECORDER_CONTAINER_WAV) {
        audio_recorder_uninit_wav_encoder(ctx);
        ctx->encoder_initialized = false;
        ctx->encoder_finalized = false;
        return;
    }

    if (ctx->encoder_initialized && !ctx->encoder_finalized) {
        ma_result result = audio_recorder_finalize_encoder(ctx);
        if (ctx->result == MA_SUCCESS && result != MA_SUCCESS) {
            ctx->result = result;
        }
    }

    if (ctx->format_ctx && ctx->format_ctx->pb) {
        if (ctx->custom_avio) {
            av_freep(&ctx->format_ctx->pb->buffer);
            avio_context_free(&ctx->format_ctx->pb);
            ctx->avio_buffer = NULL;
            ctx->avio_ctx = NULL;
            ctx->custom_avio = false;
        } else {
            avio_closep(&ctx->format_ctx->pb);
        }
    }
    avformat_free_context(ctx->format_ctx);
    ctx->format_ctx = NULL;

    avcodec_free_context(&ctx->codec_ctx);
    ctx->stream = NULL;
    av_packet_free(&ctx->packet);
    swr_free(&ctx->swr_ctx);
    if (ctx->fifo) {
        av_audio_fifo_free(ctx->fifo);
        ctx->fifo = NULL;
    }

    ctx->encoder_initialized = false;
    ctx->encoder_finalized = false;
    ctx->frame_size = 0;
    ctx->next_pts = 0;
}

static ma_result audio_recorder_init_encoder(AudioRecorderContext* ctx, const char* outputPath, AudioRecorderContainer container, ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 bitRate)
{
    if (container == AUDIO_RECORDER_CONTAINER_PCM) {
        if (!ctx->output_callback || !ctx->output_write || channels == 0 || sampleRate == 0) {
            return MA_INVALID_ARGS;
        }

        ctx->container = container;
        ctx->encoder_initialized = true;
        ctx->encoder_finalized = false;
        return MA_SUCCESS;
    }

    if (container == AUDIO_RECORDER_CONTAINER_WAV) {
        return audio_recorder_init_wav_encoder(ctx, outputPath, format, channels, sampleRate, bitRate);
    }

    const char* muxerName = audio_recorder_get_muxer_name(container);
    enum AVSampleFormat inputSampleFormat = audio_recorder_map_sample_format(format);
    if ((!outputPath && !ctx->output_callback) || !muxerName || inputSampleFormat == AV_SAMPLE_FMT_NONE || channels == 0 || sampleRate == 0) {
        return MA_INVALID_ARGS;
    }

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        return MA_FORMAT_NOT_SUPPORTED;
    }

    const char* outputName = outputPath ? outputPath : audio_recorder_get_output_name(container);
    int error = avformat_alloc_output_context2(&ctx->format_ctx, NULL, muxerName, outputName);
    if (error < 0 || !ctx->format_ctx) {
        return error < 0 ? audio_recorder_av_error_to_ma(error) : MA_ERROR;
    }

    ctx->stream = avformat_new_stream(ctx->format_ctx, NULL);
    if (!ctx->stream) {
        audio_recorder_uninit_encoder(ctx);
        return MA_OUT_OF_MEMORY;
    }

    ctx->codec_ctx = avcodec_alloc_context3(codec);
    if (!ctx->codec_ctx) {
        audio_recorder_uninit_encoder(ctx);
        return MA_OUT_OF_MEMORY;
    }

    av_channel_layout_default(&ctx->codec_ctx->ch_layout, (int)channels);
    ctx->codec_ctx->sample_rate = (int)sampleRate;
    ctx->codec_ctx->sample_fmt = audio_recorder_select_encoder_sample_format(codec);
    ctx->codec_ctx->bit_rate = bitRate > 0 ? bitRate : AUDIO_RECORD_DEFAULT_BITRATE;
    ctx->codec_ctx->time_base = (AVRational){ 1, (int)sampleRate };
    ctx->stream->time_base = ctx->codec_ctx->time_base;

    if (ctx->format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        ctx->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    error = avcodec_open2(ctx->codec_ctx, codec, NULL);
    if (error < 0) {
        audio_recorder_uninit_encoder(ctx);
        return audio_recorder_av_error_to_ma(error);
    }

    error = avcodec_parameters_from_context(ctx->stream->codecpar, ctx->codec_ctx);
    if (error < 0) {
        audio_recorder_uninit_encoder(ctx);
        return audio_recorder_av_error_to_ma(error);
    }

    if (ctx->output_callback) {
        ctx->avio_buffer = av_malloc(32768);
        if (!ctx->avio_buffer) {
            audio_recorder_uninit_encoder(ctx);
            return MA_OUT_OF_MEMORY;
        }

        ctx->avio_ctx = avio_alloc_context(
            ctx->avio_buffer,
            32768,
            1,
            ctx,
            NULL,
            audio_recorder_avio_write,
            ctx->output_seek ? audio_recorder_avio_seek : NULL);
        if (!ctx->avio_ctx) {
            av_freep(&ctx->avio_buffer);
            audio_recorder_uninit_encoder(ctx);
            return MA_OUT_OF_MEMORY;
        }

        if (ctx->output_seek) {
            ctx->avio_ctx->seekable = AVIO_SEEKABLE_NORMAL;
        }
        ctx->format_ctx->pb = ctx->avio_ctx;
        ctx->format_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
        ctx->custom_avio = true;
    } else if (!(ctx->format_ctx->oformat->flags & AVFMT_NOFILE)) {
        error = avio_open(&ctx->format_ctx->pb, outputPath, AVIO_FLAG_WRITE);
        if (error < 0) {
            audio_recorder_uninit_encoder(ctx);
            return audio_recorder_av_error_to_ma(error);
        }
    }

    error = avformat_write_header(ctx->format_ctx, NULL);
    if (error < 0) {
        audio_recorder_uninit_encoder(ctx);
        return audio_recorder_av_error_to_ma(error);
    }

    ctx->packet = av_packet_alloc();
    if (!ctx->packet) {
        audio_recorder_uninit_encoder(ctx);
        return MA_OUT_OF_MEMORY;
    }

    AVChannelLayout inputLayout;
    av_channel_layout_default(&inputLayout, (int)channels);
    error = swr_alloc_set_opts2(
        &ctx->swr_ctx,
        &ctx->codec_ctx->ch_layout,
        ctx->codec_ctx->sample_fmt,
        ctx->codec_ctx->sample_rate,
        &inputLayout,
        inputSampleFormat,
        (int)sampleRate,
        0,
        NULL);
    av_channel_layout_uninit(&inputLayout);
    if (error < 0) {
        audio_recorder_uninit_encoder(ctx);
        return audio_recorder_av_error_to_ma(error);
    }

    error = swr_init(ctx->swr_ctx);
    if (error < 0) {
        audio_recorder_uninit_encoder(ctx);
        return audio_recorder_av_error_to_ma(error);
    }

    ctx->fifo = av_audio_fifo_alloc(ctx->codec_ctx->sample_fmt, ctx->codec_ctx->ch_layout.nb_channels, 1);
    if (!ctx->fifo) {
        audio_recorder_uninit_encoder(ctx);
        return MA_OUT_OF_MEMORY;
    }

    ctx->frame_size = ctx->codec_ctx->frame_size > 0 ? ctx->codec_ctx->frame_size : 1024;
    ctx->next_pts = 0;
    ctx->container = container;
    ctx->encoder_initialized = true;
    ctx->encoder_finalized = false;
    return MA_SUCCESS;
}
#else
static ma_result audio_recorder_init_encoder(AudioRecorderContext* ctx, const char* outputPath, AudioRecorderContainer container, ma_format format, ma_uint32 channels, ma_uint32 sampleRate, ma_uint32 bitRate)
{
    if (container == AUDIO_RECORDER_CONTAINER_PCM) {
        if (!ctx->output_callback || !ctx->output_write || channels == 0 || sampleRate == 0) {
            return MA_INVALID_ARGS;
        }

        ctx->container = container;
        ctx->encoder_initialized = true;
        ctx->encoder_finalized = false;
        return MA_SUCCESS;
    }

    if (container == AUDIO_RECORDER_CONTAINER_WAV) {
        return audio_recorder_init_wav_encoder(ctx, outputPath, format, channels, sampleRate, bitRate);
    }

    return MA_NOT_IMPLEMENTED;
}

static ma_result audio_recorder_encode_pcm(AudioRecorderContext* ctx, const void* pInput, ma_uint64 frameCount)
{
    if (ctx->container == AUDIO_RECORDER_CONTAINER_PCM) {
        return audio_recorder_encode_raw_pcm(ctx, pInput, frameCount);
    }

    if (ctx->container == AUDIO_RECORDER_CONTAINER_WAV) {
        return audio_recorder_encode_wav_pcm(ctx, pInput, frameCount);
    }

    return MA_NOT_IMPLEMENTED;
}

static ma_result audio_recorder_finalize_encoder(AudioRecorderContext* ctx)
{
    if (ctx->container == AUDIO_RECORDER_CONTAINER_PCM) {
        ctx->encoder_finalized = true;
        return MA_SUCCESS;
    }

    if (ctx->container == AUDIO_RECORDER_CONTAINER_WAV) {
        return audio_recorder_finalize_wav_encoder(ctx);
    }

    return MA_NOT_IMPLEMENTED;
}

static void audio_recorder_uninit_encoder(AudioRecorderContext* ctx)
{
    if (ctx->container == AUDIO_RECORDER_CONTAINER_PCM) {
        ctx->encoder_initialized = false;
        ctx->encoder_finalized = false;
        return;
    }

    if (ctx->container == AUDIO_RECORDER_CONTAINER_WAV) {
        audio_recorder_uninit_wav_encoder(ctx);
        ctx->encoder_initialized = false;
        ctx->encoder_finalized = false;
    }
}
#endif

static void audio_recorder_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    AudioRecorderContext* ctx = (AudioRecorderContext*)pDevice->pUserData;
    (void)pOutput;

    if (!ctx || !pInput || frameCount == 0) {
        return;
    }

    ma_mutex_lock(&ctx->buffer_mutex);
    ma_uint64 framesWritten = audio_recorder_buffer_write_locked(ctx, pInput, frameCount);
    ctx->captured_frames += framesWritten;
    if (framesWritten < frameCount) {
        ctx->dropped_frames += (ma_uint64)frameCount - framesWritten;
    }
    ma_mutex_unlock(&ctx->buffer_mutex);

    ma_event_signal(&ctx->write_event);
}

static ma_result audio_recorder_device_init(AudioRecorderContext* ctx, const ma_device_config* deviceConfig)
{
    const char* backend = getenv("SIMPLE_AUDIO_PLAYER_RECORDING_BACKEND");
    if (backend != NULL && strcmp(backend, "null") == 0) {
        ma_backend backends[] = { ma_backend_null };
        return ma_device_init_ex(backends, 1, NULL, deviceConfig, &ctx->device);
    }

    return ma_device_init(NULL, deviceConfig, &ctx->device);
}

static bool audio_recorder_should_simulate_capture(void)
{
    const char* backend = getenv("SIMPLE_AUDIO_PLAYER_RECORDING_BACKEND");
    return backend != NULL && strcmp(backend, "simulated") == 0;
}

static ma_thread_result MA_THREADCALL audio_recorder_simulate_thread_proc(void* pData)
{
    AudioRecorderContext* ctx = (AudioRecorderContext*)pData;
    if (!ctx) {
        return (ma_thread_result)0;
    }

    for (;;) {
        ma_uint64 framesWritten = 0;

        ma_mutex_lock(&ctx->buffer_mutex);
        if (ctx->stop_requested || ctx->result != MA_SUCCESS) {
            ma_mutex_unlock(&ctx->buffer_mutex);
            break;
        }

        ma_uint64 framesToWrite = audio_recorder_buffer_space_locked(ctx);
        if (framesToWrite > ctx->simulate_scratch_capacity_frames) {
            framesToWrite = ctx->simulate_scratch_capacity_frames;
        }

        if (framesToWrite > 0) {
            framesWritten = audio_recorder_buffer_write_locked(ctx, ctx->simulate_scratch, framesToWrite);
            ctx->captured_frames += framesWritten;
        }
        ma_mutex_unlock(&ctx->buffer_mutex);

        if (framesWritten > 0) {
            ma_event_signal(&ctx->write_event);
            ma_uint32 sleepMs = (ma_uint32)((framesWritten * 1000) / (ctx->sample_rate > 0 ? ctx->sample_rate : 44100));
            ma_sleep(sleepMs > 0 ? sleepMs : 1);
        } else {
            ma_sleep(5);
        }
    }

    return (ma_thread_result)0;
}

static ma_thread_result MA_THREADCALL audio_recorder_write_thread_proc(void* pData)
{
    AudioRecorderContext* ctx = (AudioRecorderContext*)pData;

    for (;;) {
        ma_uint64 framesRead = 0;
        bool shouldStop = false;

        ma_mutex_lock(&ctx->buffer_mutex);
        shouldStop = ctx->stop_requested && ctx->pcm_buffer_available_frames == 0;
        if (!shouldStop && ctx->pcm_buffer_available_frames > 0) {
            ma_uint64 framesToRead = ctx->pcm_buffer_available_frames < ctx->write_scratch_capacity_frames
                ? ctx->pcm_buffer_available_frames
                : ctx->write_scratch_capacity_frames;
            framesRead = audio_recorder_buffer_read_locked(ctx, ctx->write_scratch, framesToRead);
        }
        ma_mutex_unlock(&ctx->buffer_mutex);

        if (shouldStop) {
            break;
        }

        if (framesRead == 0) {
            ma_event_wait(&ctx->write_event);
            continue;
        }

        ma_result result = audio_recorder_encode_pcm(ctx, ctx->write_scratch, framesRead);
        if (result != MA_SUCCESS) {
            ma_mutex_lock(&ctx->buffer_mutex);
            ctx->result = result;
            ctx->stop_requested = true;
            audio_recorder_buffer_reset_locked(ctx);
            ma_mutex_unlock(&ctx->buffer_mutex);
            if (ctx->device_initialized) {
                ma_device_stop(&ctx->device);
            }
            break;
        }
    }

    ma_result result = audio_recorder_finalize_encoder(ctx);
    ma_mutex_lock(&ctx->buffer_mutex);
    if (ctx->result == MA_SUCCESS && result != MA_SUCCESS) {
        ctx->result = result;
    }
    ctx->recording = false;
    ma_mutex_unlock(&ctx->buffer_mutex);

    return (ma_thread_result)0;
}

AUDIO_PLAYER_API AudioRecorderContext* audio_recorder_context_create(void)
{
    AudioRecorderContext* ctx = (AudioRecorderContext*)ma_malloc(sizeof(AudioRecorderContext), NULL);
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(AudioRecorderContext));
    ctx->result = MA_SUCCESS;

    if (ma_mutex_init(&ctx->buffer_mutex) != MA_SUCCESS) {
        ma_free(ctx, NULL);
        return NULL;
    }

    if (ma_event_init(&ctx->write_event) != MA_SUCCESS) {
        ma_mutex_uninit(&ctx->buffer_mutex);
        ma_free(ctx, NULL);
        return NULL;
    }

    return ctx;
}

static ma_result audio_recorder_init_output(AudioRecorderContext* ctx, const char* outputPath, AudioRecorderContainer container, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate, const ma_uint32 bitRate)
{
    if (!ctx || channels == 0 || sampleRate == 0) {
        return MA_INVALID_ARGS;
    }
    if (ctx->device_initialized || ctx->encoder_initialized) {
        return MA_INVALID_OPERATION;
    }

    ctx->simulate_capture = audio_recorder_should_simulate_capture();
    ma_result result = MA_SUCCESS;

    if (ctx->simulate_capture) {
        ctx->format = format;
        ctx->channels = channels;
        ctx->sample_rate = sampleRate;
    } else {
        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
        deviceConfig.capture.format = format;
        deviceConfig.capture.channels = channels;
        deviceConfig.sampleRate = sampleRate;
        deviceConfig.dataCallback = audio_recorder_data_callback;
        deviceConfig.pUserData = ctx;

        result = audio_recorder_device_init(ctx, &deviceConfig);
        if (result != MA_SUCCESS) {
            return result;
        }

        ctx->format = ctx->device.capture.format;
        ctx->channels = ctx->device.capture.channels;
        ctx->sample_rate = ctx->device.sampleRate;
        ctx->device_initialized = true;
    }

    result = audio_recorder_buffer_init(ctx, ctx->format, ctx->channels, ctx->sample_rate);
    if (result != MA_SUCCESS) {
        if (ctx->device_initialized) {
            ma_device_uninit(&ctx->device);
            ctx->device_initialized = false;
        }
        return result;
    }

    result = audio_recorder_init_encoder(ctx, outputPath, container, ctx->format, ctx->channels, ctx->sample_rate, bitRate);
    if (result != MA_SUCCESS) {
        audio_recorder_buffer_uninit(ctx);
        if (ctx->device_initialized) {
            ma_device_uninit(&ctx->device);
            ctx->device_initialized = false;
        }
        return result;
    }

    ctx->result = MA_SUCCESS;
    ctx->captured_frames = 0;
    ctx->dropped_frames = 0;
    return MA_SUCCESS;
}

AUDIO_PLAYER_API ma_result audio_recorder_init_file(AudioRecorderContext* ctx, const char* outputPath, AudioRecorderContainer container, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate, const ma_uint32 bitRate)
{
    if (!ctx || !outputPath || container == AUDIO_RECORDER_CONTAINER_PCM) {
        return MA_INVALID_ARGS;
    }

    ctx->output_write = NULL;
    ctx->output_seek = NULL;
    ctx->output_userdata = NULL;
    ctx->output_callback = false;
    return audio_recorder_init_output(ctx, outputPath, container, format, channels, sampleRate, bitRate);
}

AUDIO_PLAYER_API ma_result audio_recorder_init_stream(AudioRecorderContext* ctx, AudioRecorderContainer container, AudioRecorderWriteCallback onWrite, AudioRecorderSeekCallback onSeek, void* userdata, const ma_format format, const ma_uint32 channels, const ma_uint32 sampleRate, const ma_uint32 bitRate)
{
    if (!ctx || !onWrite) {
        return MA_INVALID_ARGS;
    }
    if ((container == AUDIO_RECORDER_CONTAINER_WAV || container == AUDIO_RECORDER_CONTAINER_M4A) && !onSeek) {
        return MA_INVALID_ARGS;
    }

    ctx->output_write = onWrite;
    ctx->output_seek = onSeek;
    ctx->output_userdata = userdata;
    ctx->output_callback = true;
    return audio_recorder_init_output(ctx, NULL, container, format, channels, sampleRate, bitRate);
}

AUDIO_PLAYER_API ma_result audio_recorder_start(AudioRecorderContext* ctx)
{
    if (!ctx) {
        return MA_INVALID_ARGS;
    }
    if ((!ctx->device_initialized && !ctx->simulate_capture) || !ctx->encoder_initialized) {
        return MA_DEVICE_NOT_INITIALIZED;
    }
    if (ctx->encoder_finalized) {
        return MA_INVALID_OPERATION;
    }
    if (ctx->recording) {
        return MA_SUCCESS;
    }

    ctx->stop_requested = false;
    ctx->result = MA_SUCCESS;
    ma_result result = ma_thread_create(&ctx->write_thread, ma_thread_priority_normal, 0, audio_recorder_write_thread_proc, ctx, NULL);
    if (result != MA_SUCCESS) {
        return result;
    }
    ctx->write_thread_started = true;

    if (ctx->simulate_capture) {
        result = ma_thread_create(&ctx->simulate_thread, ma_thread_priority_normal, 0, audio_recorder_simulate_thread_proc, ctx, NULL);
        if (result != MA_SUCCESS) {
            ma_mutex_lock(&ctx->buffer_mutex);
            ctx->stop_requested = true;
            ma_mutex_unlock(&ctx->buffer_mutex);
            ma_event_signal(&ctx->write_event);
            ma_thread_wait(&ctx->write_thread);
            ctx->write_thread_started = false;
            return result;
        }
        ctx->simulate_thread_started = true;
    } else {
        result = ma_device_start(&ctx->device);
        if (result != MA_SUCCESS) {
            ma_mutex_lock(&ctx->buffer_mutex);
            ctx->stop_requested = true;
            ma_mutex_unlock(&ctx->buffer_mutex);
            ma_event_signal(&ctx->write_event);
            ma_thread_wait(&ctx->write_thread);
            ctx->write_thread_started = false;
            return result;
        }
    }

    ctx->recording = true;
    return MA_SUCCESS;
}

AUDIO_PLAYER_API ma_result audio_recorder_stop(AudioRecorderContext* ctx)
{
    if (!ctx) {
        return MA_INVALID_ARGS;
    }

    if (ctx->device_initialized) {
        ma_device_stop(&ctx->device);
    }

    ma_mutex_lock(&ctx->buffer_mutex);
    ctx->stop_requested = true;
    ma_mutex_unlock(&ctx->buffer_mutex);
    ma_event_signal(&ctx->write_event);

    if (ctx->simulate_thread_started) {
        ma_thread_wait(&ctx->simulate_thread);
        ctx->simulate_thread_started = false;
    }

    if (ctx->write_thread_started) {
        ma_thread_wait(&ctx->write_thread);
        ctx->write_thread_started = false;
    } else if (ctx->encoder_initialized && !ctx->encoder_finalized) {
        ma_result result = audio_recorder_finalize_encoder(ctx);
        if (ctx->result == MA_SUCCESS && result != MA_SUCCESS) {
            ctx->result = result;
        }
    }

    return ctx->result;
}

AUDIO_PLAYER_API void audio_recorder_cleanup(AudioRecorderContext* ctx)
{
    if (!ctx) {
        return;
    }

    audio_recorder_stop(ctx);

    if (ctx->device_initialized) {
        ma_device_uninit(&ctx->device);
        ctx->device_initialized = false;
    }
    ctx->simulate_capture = false;

    audio_recorder_uninit_encoder(ctx);
    audio_recorder_buffer_uninit(ctx);
    ma_event_uninit(&ctx->write_event);
    ma_mutex_uninit(&ctx->buffer_mutex);
    ma_free(ctx, NULL);
}

AUDIO_PLAYER_API ma_uint64 audio_recorder_get_captured_frames(AudioRecorderContext* ctx)
{
    if (!ctx) {
        return 0;
    }

    ma_mutex_lock(&ctx->buffer_mutex);
    ma_uint64 frames = ctx->captured_frames;
    ma_mutex_unlock(&ctx->buffer_mutex);
    return frames;
}

AUDIO_PLAYER_API ma_uint64 audio_recorder_get_dropped_frames(AudioRecorderContext* ctx)
{
    if (!ctx) {
        return 0;
    }

    ma_mutex_lock(&ctx->buffer_mutex);
    ma_uint64 frames = ctx->dropped_frames;
    ma_mutex_unlock(&ctx->buffer_mutex);
    return frames;
}

AUDIO_PLAYER_API ma_result audio_recorder_get_result(AudioRecorderContext* ctx)
{
    if (!ctx) {
        return MA_INVALID_ARGS;
    }

    ma_mutex_lock(&ctx->buffer_mutex);
    ma_result result = ctx->result;
    ma_mutex_unlock(&ctx->buffer_mutex);
    return result;
}
