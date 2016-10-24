//
// Created by mutter on 5/8/16.
//

#include <jni.h>
#include <android/log.h>
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "ffmpeg.h"

#define SAMPLE_RATE 44100
#define NB_CHANNELS 1

AVFormatContext* ofmt_ctx;
AVCodecContext* oc_ctx;
AVCodec* oc;
AVCodecContext* aoc_ctx;
AVCodec* aoc;
struct SwsContext* swsContext;
struct SwrContext* swrContext;

static int video_index = 0;
static int audio_index = 1;

static jboolean isAudioStreamEnd = 0;
static jboolean isVideoStreamEnd = 0;

static uint64_t audio_pts = 0;
static uint64_t video_pts = 0;

static int CAPTURE_WIDTH;
static int CAPTURE_HEIGHT;

#define OUTPUT_WIDTH 568
#define OUTPUT_HEIGHT 320

static void reset_state()
{
    audio_pts = 0;
    video_pts = 0;
    isAudioStreamEnd = 0;
    isVideoStreamEnd = 0;
}

JNIEXPORT void JNICALL
Java_com_mutter_ffrecorder_FFmpegNative_naInitEncoder(JNIEnv *env, jclass type, jint width, jint height,
                                                  jstring outPath_, jint orientationHint) {
    start_logger(TAG);
    av_register_all();
    avcodec_register_all();

    reset_state();
    const char *outPath = (*env)->GetStringUTFChars(env, outPath_, 0);
    CAPTURE_WIDTH = width;
    CAPTURE_HEIGHT = height;
    LOGD("naInitEncoder %s", outPath);

    int ret;
    avformat_alloc_output_context2(&ofmt_ctx,
                                   av_guess_format("mp4", outPath, "video/mp4"), "mp4", outPath);
    oc = avcodec_find_encoder(AV_CODEC_ID_H264);
    aoc = avcodec_find_encoder(AV_CODEC_ID_AAC);
    oc_ctx = avcodec_alloc_context3(oc);
    aoc_ctx = avcodec_alloc_context3(aoc);

    AVStream* stream = avformat_new_stream(ofmt_ctx, oc);
    if (stream != NULL) {
        video_index = 0;
        AVCodecParameters* parameters = stream->codecpar;
        parameters->format = AV_PIX_FMT_YUV420P;
        parameters->codec_id = oc->id;
        parameters->codec_type = AVMEDIA_TYPE_VIDEO;
        parameters->codec_tag = 0;
        parameters->width = OUTPUT_WIDTH;
        parameters->height = OUTPUT_HEIGHT;
        parameters->bit_rate = 480000;
        parameters->sample_rate = SAMPLE_RATE;
        parameters->profile = FF_PROFILE_H264_BASELINE;

        avcodec_parameters_to_context(oc_ctx, parameters);
        oc_ctx->time_base = (AVRational) {1, 24};
        oc_ctx->gop_size = 40;
        oc_ctx->max_b_frames = 3;
        av_opt_set(oc_ctx->priv_data, "preset", "ultrafast", 0);
    }

    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        oc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (orientationHint != 0) {
        char orientation[3] = {0};
        snprintf(orientation, sizeof(orientation), "%d", orientationHint);
        av_dict_set(&stream->metadata, "rotate", orientation, 0);
    }

    AVDictionary* opts = NULL;
    av_dict_set(&opts, "profile", "baseline", 0);
    av_dict_set(&opts, "level", "3.0", 0);
    if ((ret = avcodec_open2(oc_ctx, oc, &opts)) < 0) {
        LOGD("open video codec %s\n", av_err2str(ret));
        goto end;
    }

    stream->codecpar->extradata = av_mallocz(
            (size_t) (oc_ctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE));
    stream->codecpar->extradata_size = oc_ctx->extradata_size;
    memcpy(stream->codecpar->extradata, oc_ctx->extradata, (size_t) oc_ctx->extradata_size);

    swsContext = sws_getContext(CAPTURE_WIDTH, CAPTURE_HEIGHT, AV_PIX_FMT_NV21,
                                OUTPUT_WIDTH, OUTPUT_HEIGHT, AV_PIX_FMT_YUV420P,
                                SWS_FAST_BILINEAR, NULL, NULL, NULL);

    AVStream* astream = avformat_new_stream(ofmt_ctx, aoc);
    if (astream != NULL) {
        audio_index = 1;
        AVCodecParameters* parameters = astream->codecpar;
        parameters->codec_tag = 0;
        parameters->codec_id = aoc->id;
        parameters->codec_type = AVMEDIA_TYPE_AUDIO;
        parameters->sample_rate = SAMPLE_RATE;
        parameters->format = AV_SAMPLE_FMT_FLTP;
        parameters->channel_layout = (uint64_t) av_get_default_channel_layout(NB_CHANNELS);
        parameters->channels = NB_CHANNELS;
        parameters->bit_rate = 64000;
        // parameters->frame_size = 1024;
        parameters->profile = FF_PROFILE_AAC_LOW;

        if ((ret = avcodec_parameters_to_context(aoc_ctx, parameters)) < 0) {
            LOGD("audio parameters to codec context %s\n", av_err2str(ret));
            goto end;
        }
        if (aoc->id == AV_CODEC_ID_AAC) {
            aoc_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
        }
        // av_opt_set(aoc_ctx->priv_data, "vbr", "3", 0);
        // aoc_ctx->time_base = (AVRational) {1, aoc_ctx->sample_rate};
    } else {
        LOGD("new audio stream %s\n", av_err2str(ret));
    }

    if (ofmt_ctx->flags & AVFMT_GLOBALHEADER) {
        aoc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if ((ret = avcodec_open2(aoc_ctx, aoc, NULL)) < 0) {
        LOGD("open audio codec %s\n", av_err2str(ret));
        goto end;
    }

    astream->codecpar->extradata = av_mallocz(
            (size_t) (aoc_ctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE));
    astream->codecpar->extradata_size = aoc_ctx->extradata_size;
    memcpy(astream->codecpar->extradata, aoc_ctx->extradata, (size_t) aoc_ctx->extradata_size);

    swrContext = swr_alloc_set_opts(NULL,
                                    aoc_ctx->channel_layout, aoc_ctx->sample_fmt, aoc_ctx->sample_rate,
                                    AV_CH_LAYOUT_MONO, AV_SAMPLE_FMT_S16, SAMPLE_RATE,
                                    0, NULL);
    if ((ret = swr_init(swrContext)) < 0) {
        LOGD("swr_init failed %s\n", av_err2str(ret));
    }

    if ((ret = avio_open2(&ofmt_ctx->pb, outPath, AVIO_FLAG_READ_WRITE, NULL, NULL)) < 0) {
        LOGD("avio open %s\n", av_err2str(ret));
        goto end;
    }

    if ((ret = avformat_write_header(ofmt_ctx, NULL)) < 0) {
        LOGD("write header %s\n", av_err2str(ret));
        goto end;
    }

    av_dump_format(ofmt_ctx, 0, outPath, 1);
    end:
    LOGD("naInitEncoder END");
    (*env)->ReleaseStringUTFChars(env, outPath_, outPath);
}

JNIEXPORT jint JNICALL
Java_com_mutter_ffrecorder_FFmpegNative_naEncodeVideo(JNIEnv *env, jclass type, jobject data) {

    int ret;

    uint8_t* buffer = (*env)->GetDirectBufferAddress(env, data);
    AVFrame* frame = av_frame_alloc();
    av_image_alloc(frame->data, frame->linesize, CAPTURE_WIDTH, CAPTURE_HEIGHT, AV_PIX_FMT_NV21, 8);
    ret = av_image_fill_arrays(frame->data, frame->linesize, buffer, AV_PIX_FMT_NV21,
                               CAPTURE_WIDTH, CAPTURE_HEIGHT, 8);
    frame->format = AV_PIX_FMT_NV21;
    frame->width = CAPTURE_WIDTH;
    frame->height = CAPTURE_HEIGHT;
    // frame->pts = av_rescale_q(av_gettime(), (AVRational){1, 1000000}, oc_ctx->time_base);
    // frame->pts = av_frame_get_best_effort_timestamp(frame);
    // frame->pts = (int64_t) ((1.0 / 24.0) * 90000 * nextPTS());
    // frame->pts = av_rescale_q(nextPTS(), oc_ctx->time_base, ofmt_ctx->streams[0]->time_base);
    // frame->pts = video_pts++;

    AVFrame* enc_frame = av_frame_alloc();
    av_image_alloc(enc_frame->data, enc_frame->linesize, OUTPUT_WIDTH, OUTPUT_HEIGHT, AV_PIX_FMT_YUV420P, 8);
    sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize, 0, frame->height,
              enc_frame->data, enc_frame->linesize);
    enc_frame->format = AV_PIX_FMT_YUV420P;
    enc_frame->width = OUTPUT_WIDTH;
    enc_frame->height = OUTPUT_HEIGHT;
    enc_frame->pts = video_pts++;

    if (ret >= 0) {
        AVPacket* pkt = av_packet_alloc();
        av_init_packet(pkt);
        pkt->data = NULL;
        pkt->size = 0;

        int got_packet;
        ret = avcodec_encode_video2(oc_ctx, pkt, enc_frame, &got_packet);
        if (ret == 0) {
            if (got_packet) {
//                if (pkt->pts != AV_NOPTS_VALUE) {
//                    pkt->pts = av_rescale_q(pkt->pts, oc_ctx->time_base, ofmt_ctx->streams[0]->time_base);
//                }
//                if (pkt->dts != AV_NOPTS_VALUE) {
//                    pkt->dts = av_rescale_q(pkt->dts, oc_ctx->time_base, ofmt_ctx->streams[0]->time_base);
//                }
                av_packet_rescale_ts(pkt, oc_ctx->time_base, ofmt_ctx->streams[video_index]->time_base);
                pkt->stream_index = 0;

                ret = av_interleaved_write_frame(ofmt_ctx, pkt);
                if (ret < 0) {
                    LOGD("write video frame %s\n", av_err2str(ret));
                }
            }
        } else {
            LOGD("encode video error %s\n", av_err2str(ret));
        }

        av_packet_free(&pkt);
    } else {
        LOGD("fill array error %s\n", av_err2str(ret));
    }

    av_frame_free(&frame);
    LOGD("encode video %llu\n", video_pts);
    return 0;
}

JNIEXPORT void JNICALL
Java_com_mutter_ffrecorder_FFmpegNative_naFinalizeEncoder(JNIEnv *env, jclass type, jboolean isAudioEnd, jboolean isVideoEnd) {
    if (isAudioEnd) {
        isAudioStreamEnd = 1;
    }

    if (isVideoEnd) {
        isVideoStreamEnd = 1;
    }

    if (isAudioStreamEnd && isVideoStreamEnd) {
        av_write_trailer(ofmt_ctx);
        avio_closep(&ofmt_ctx->pb);

        avcodec_close(oc_ctx);
        avcodec_close(aoc_ctx);
        avcodec_free_context(&oc_ctx);
        avcodec_free_context(&aoc_ctx);
        avformat_free_context(ofmt_ctx);
        LOGD("finalize encoder");
    }
}

JNIEXPORT jint JNICALL
Java_com_mutter_ffrecorder_FFmpegNative_naEncodeAudio(JNIEnv *env, jclass type, jshortArray  data) {

    int ret = 0;

    jshort* tmp_data = (*env)->GetShortArrayElements(env, data, 0);

    size_t buffer_size = (size_t) ((*env)->GetArrayLength(env, data) * 2);
    uint8_t* buffer = av_malloc(buffer_size);
    memcpy(buffer, tmp_data, buffer_size);

    // uint8_t* buffer = (*env)->GetDirectBufferAddress(env, data);
    // jlong  buffer_size = (*env)->GetDirectBufferCapacity(env, data);
    int nb_samples = (int) (buffer_size / 2);

    AVFrame* frame = av_frame_alloc();
    av_samples_alloc(frame->data, frame->linesize, 1, nb_samples, AV_SAMPLE_FMT_S16, 0);
    ret = av_samples_fill_arrays(frame->data, frame->linesize, buffer, 1, nb_samples, AV_SAMPLE_FMT_S16, 0);
    if (ret < 0) {
        LOGD("fill audio arrays error %s\n", av_err2str(ret));
        goto end;
    }

    frame->nb_samples = nb_samples;
    frame->sample_rate = SAMPLE_RATE;
    frame->format = AV_SAMPLE_FMT_S16;
    frame->channel_layout = AV_CH_LAYOUT_MONO;
    frame->channels = NB_CHANNELS;

    ret = avcodec_fill_audio_frame(frame, 1, AV_SAMPLE_FMT_S16, buffer, (int) buffer_size, 0);
    if (ret < 0) {
        LOGD("fill audio frame error %s\n", av_err2str(ret));
        goto end;
    }

    AVFrame* enc_frame = av_frame_alloc();
    av_samples_alloc(enc_frame->data, enc_frame->linesize, 1,
                     aoc_ctx->frame_size, aoc_ctx->sample_fmt, 0);
    // ret = swr_convert_frame(swrContext, enc_frame, frame);
    ret = swr_convert(swrContext, enc_frame->data, aoc_ctx->frame_size,
                      (const uint8_t **) frame->data, frame->nb_samples);

    enc_frame->nb_samples = aoc_ctx->frame_size;
    enc_frame->sample_rate = aoc_ctx->sample_rate;
    enc_frame->channel_layout = aoc_ctx->channel_layout;
    enc_frame->format = aoc_ctx->sample_fmt;
    enc_frame->channels = NB_CHANNELS;
    // enc_frame->quality = aoc_ctx->global_quality;
    // enc_frame->pts = ++audio_frame_count;
    enc_frame->pts = audio_pts;
    audio_pts += enc_frame->nb_samples;

    if (ret >= 0) {

        AVPacket* pkt = av_packet_alloc();
        av_init_packet(pkt);
        pkt->data = NULL;
        pkt->size = 0;
        int got_packet;
        if ((ret = avcodec_encode_audio2(aoc_ctx, pkt, enc_frame, &got_packet)) == 0) {
            if (got_packet) {
//                if (pkt->pts != AV_NOPTS_VALUE) {
//                    pkt->pts = av_rescale_q(pkt->pts, aoc_ctx->time_base,
//                                            ofmt_ctx->streams[1]->time_base);
//                }
//                if (pkt->dts != AV_NOPTS_VALUE) {
//                    pkt->dts = av_rescale_q(pkt->dts, aoc_ctx->time_base,
//                                            ofmt_ctx->streams[1]->time_base);
//                }
                av_packet_rescale_ts(pkt, aoc_ctx->time_base, ofmt_ctx->streams[audio_index]->time_base);
//                pkt->pts = AV_NOPTS_VALUE;
//                pkt->dts = av_rescale_q(av_gettime(), (AVRational){1, 1000000},
//                                        ofmt_ctx->streams[1]->time_base);
                pkt->stream_index = 1;
                if ((ret = av_interleaved_write_frame(ofmt_ctx, pkt)) != 0) {
                    LOGD("write audio frame error %s\n", av_err2str(ret));
                }
            }
        } else {
            LOGD("encode audio error %s\n", av_err2str(ret));
        }

        av_packet_free(&pkt);
    } else {
        LOGD("convert audio frame error %s\n", av_err2str(ret));
    }

    av_frame_unref(frame);
    av_frame_unref(enc_frame);
    av_free(buffer);
    LOGD("encode audio");
    end:

    (*env)->ReleaseShortArrayElements(env, data, tmp_data, JNI_ABORT);
    return ret;
}
