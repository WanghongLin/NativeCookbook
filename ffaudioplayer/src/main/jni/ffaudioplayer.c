//
// Created by mutter on 8/13/16.
//

#ifdef __cplusplus
extern "C" {
#endif

#include <android/log.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include <inttypes.h>

#ifdef __cplusplus
} // extern "C"
#endif

#include <jni.h>

#define TAG "ffaudioplayer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, TAG, __VA_ARGS__)

static int nb_channels;
static int sample_rate;
static void* out_buffer;
static jlong out_buffer_size;

jobject handler;
jmethodID obtainMessageMethodID;
jmethodID sendToTargetMethodID;

static const int EVENT_START = 0;
static const int EVENT_PLAYING = 1;
static const int EVENT_STOP = 2;

static AVCodec* codec;
static AVFormatContext* formatContext;
static AVCodecContext* codecContext;
static AVCodecParameters* codecParameters;
static SwrContext* swrContext;
static int audioIndex = -1;

static void play_frame(AVFrame *pFrame, JNIEnv *env);
static void stop_play(JNIEnv *env) ;

static void sendEvent(JNIEnv *env, int what, int arg1, int arg2)
{
    if (handler != NULL && obtainMessageMethodID != NULL && sendToTargetMethodID != NULL) {
        jobject m = (*env)->CallObjectMethod(env, handler, obtainMessageMethodID, what, arg1, arg2);
        (*env)->CallVoidMethod(env, m, sendToTargetMethodID);
        (*env)->DeleteLocalRef(env, m);
    }
}

JNIEXPORT void JNICALL
Java_com_mutter_ffaudioplayer_FFAudioNative_nativeInitAudioDecoder(JNIEnv *env, jclass type,
                                                                   jint numberOfChannels,
                                                                   jint sampleRate) {
    av_register_all();
    avcodec_register_all();

    nb_channels = numberOfChannels;
    sample_rate = sampleRate;

    jclass clzHandler = (*env)->FindClass(env, "android/os/Handler");
    jclass clzMessage = (*env)->FindClass(env, "android/os/Message");

    obtainMessageMethodID = (*env)->GetMethodID(env, clzHandler, "obtainMessage", "(III)Landroid/os/Message;");
    sendToTargetMethodID = (*env)->GetMethodID(env, clzMessage, "sendToTarget", "()V");
    if (obtainMessageMethodID == NULL || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        LOGE("can not find method obtainMessage");
    }

    if (sendToTargetMethodID == NULL || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        LOGE("can not find method sendToTarget\n");
    }

    LOGI("init audio decoder\n");
}

JNIEXPORT void JNICALL
Java_com_mutter_ffaudioplayer_FFAudioNative_nativeSetAudioSource(JNIEnv *env, jclass type,
                                                                 jstring audioPath_,
                                                                 jobject outBuffer,
                                                                 jobject dataAvailableHandler) {
    const char *audioPath = (*env)->GetStringUTFChars(env, audioPath_, 0);

    out_buffer = (*env)->GetDirectBufferAddress(env, outBuffer);
    out_buffer_size = (*env)->GetDirectBufferCapacity(env, outBuffer);
    handler = (*env)->NewGlobalRef(env, dataAvailableHandler);

    int ret = 0;

    formatContext = avformat_alloc_context();
    if ((ret = avformat_open_input(&formatContext, audioPath, NULL, NULL)) < 0) {
        LOGE("error input input %s\n", av_err2str(ret));
        goto end;
    }

    avformat_find_stream_info(formatContext, NULL);

    ret = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (ret == AVERROR_STREAM_NOT_FOUND) {
        LOGE("no audio stream found %s\n", av_err2str(ret));
        goto end;
    } else {
        jboolean shouldFindDecoder = 0;
        if (ret == AVERROR_DECODER_NOT_FOUND) {
            shouldFindDecoder = 1;
        }
        for (unsigned i = 0; i < formatContext->nb_streams; ++i) {
            if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioIndex = i;

                if (shouldFindDecoder) {
                    codec = avcodec_find_decoder(formatContext->streams[i]->codecpar->codec_id);
                }
                codecParameters = formatContext->streams[i]->codecpar;
                break;
            }
        }
    }

    if (codec != NULL) {
        codecContext = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecContext, codecParameters);
        ret = avcodec_open2(codecContext, codec, NULL);
        if (ret < 0) {
            LOGE("open codec error %s\n", av_err2str(ret));
        }
    } else {
        LOGE("can not find audio codec\n");
    }

    swrContext = swr_alloc_set_opts(NULL,
                                    av_get_default_channel_layout(nb_channels), AV_SAMPLE_FMT_S16, sample_rate,
                                    av_get_default_channel_layout(codecContext->channels), codecContext->sample_fmt, codecContext->sample_rate,
                                    0, NULL);
    if ((ret = swr_init(swrContext)) < 0) {
        LOGE("swr init failed %s\n", av_err2str(ret));
    }

    end:
    (*env)->ReleaseStringUTFChars(env, audioPath_, audioPath);
}

static void play_frame(AVFrame *pFrame, JNIEnv *env) {
    AVFrame* frameS16 = av_frame_alloc();

    int ret;
    frameS16->channels = nb_channels;
    frameS16->nb_samples = pFrame->nb_samples;
    frameS16->sample_rate = sample_rate;

    ret = av_samples_alloc(frameS16->data, frameS16->linesize, nb_channels, frameS16->nb_samples, AV_SAMPLE_FMT_S16, 0);
    if (ret < 0) {
        LOGE("play frame error %s\n", av_err2str(ret));
    }

    ret = swr_convert(swrContext, frameS16->data, frameS16->nb_samples, (const uint8_t **) pFrame->data, pFrame->nb_samples);
    if (ret < 0) {
        LOGE("play frame error %s\n", av_err2str(ret));
    }

    memcpy(out_buffer, frameS16->data[0], (size_t) frameS16->linesize[0]);
    LOGI("convert audio frame, sample %d -> %d, sample rate %d -> %d\n", pFrame->nb_samples, frameS16->nb_samples,
         codecContext->sample_rate, sample_rate);

    sendEvent(env, EVENT_PLAYING, frameS16->linesize[0], 0);

    av_frame_free(&frameS16);
}

JNIEXPORT void JNICALL
Java_com_mutter_ffaudioplayer_FFAudioNative_nativeStartPlay(JNIEnv *env, jclass type) {
    if (codecContext != NULL && formatContext != NULL) {
        AVPacket* packet = av_packet_alloc();
        av_init_packet(packet);
        packet->data = NULL;
        packet->size = 0;

        int64_t startTime = av_gettime();
        sendEvent(env, EVENT_START, 0, 0);
        while (av_read_frame(formatContext, packet) == 0) {
            int packet_ret;
            do {
                packet_ret = avcodec_send_packet(codecContext, packet);
                if (packet_ret == 0) {

                    int frame_ret;
                    do {
                        AVFrame* frame = av_frame_alloc();
                        frame_ret = avcodec_receive_frame(codecContext, frame);
                        if (frame_ret == 0) {
                            int64_t pts = av_frame_get_best_effort_timestamp(frame);
                            pts = av_rescale_q(pts, formatContext->streams[audioIndex]->time_base, AV_TIME_BASE_Q);
                            while (startTime + pts > av_gettime()) {
                                ;
                            }
                            // LOGI("play audio frame %" PRId64 "\n", pts);
                            play_frame(frame, env);
                        }
                        av_frame_unref(frame);
                    } while (frame_ret == 0);
                }
            } while (packet_ret == AVERROR(EAGAIN));
        }

        av_packet_unref(packet);
    } else {
        LOGE("audio data not set\n");
    }

    stop_play(env);
}

static void stop_play(JNIEnv *env) {
    LOGE("stop play\n");
    sendEvent(env, EVENT_STOP, 0, 0);

    if (formatContext != NULL) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
    }
    if (codecContext != NULL) {
        avcodec_close(codecContext);
        avcodec_free_context(&codecContext);
    }
    (*env)->DeleteGlobalRef(env, handler);
}

JNIEXPORT void JNICALL
Java_com_mutter_ffaudioplayer_FFAudioNative_nativeFinalizeAudioDecoder(JNIEnv *env, jclass type) {
    stop_play(env);
}

