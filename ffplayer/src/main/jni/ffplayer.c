//
// Created by mutter on 8/16/16.
//

#include <jni.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <pthread.h>
#include "ffplayer_helper.h"
#include <android/bitmap.h>
#include <libavutil/imgutils.h>

JavaVM* javaVM;
JNIEnv* jniEnv;

jobject player_handler;
jobject picture_handler;
jobject sound_handler;
jmethodID obtain_message;
jmethodID send_to_target;
jmethodID obtain_message_with_object;

static void* audio_buffer;
static jlong audio_buffer_size;
static jobject picture_bitmap;

static jint video_width;
static jint video_height;
static jint audio_channel;
static jint audio_sample_rate;

static char* data_source;
static pthread_t player_thread;
static int audio_index;
static int video_index;
static int64_t start_time;

static SwrContext* swrContext;
static struct SwsContext* swsContext;
static jboolean verbose = 0;

// thread control
static pthread_mutex_t pthread_mutex;
static FFPlayerState ffPlayerState;
static pthread_rwlock_t pthread_paused_lock;
static int64_t pause_start_time;

void acquire_pause_lock()
{
    LOGD("acquire pause lock");
    pause_start_time = av_gettime();
    pthread_rwlock_init(&pthread_paused_lock, NULL);
    pthread_rwlock_wrlock(&pthread_paused_lock);
}

void release_pause_lock()
{
    LOGD("release pause lock");
    start_time += (av_gettime() - pause_start_time);
    pthread_rwlock_unlock(&pthread_paused_lock);
}

void send_progress_update(jdouble progress)
{
    if (progress > 0 && progress < 1.0) {
        jclass doubleClz = (*jniEnv)->FindClass(jniEnv, "java/lang/Double");
        jmethodID doubleCtr = (*jniEnv)->GetMethodID(jniEnv, doubleClz, "<init>", "(D)V");
        jobject p = (*jniEnv)->NewObject(jniEnv, doubleClz, doubleCtr, progress);

        send_player_message_with_object(jniEnv, FF_PLAYER_MSG_PROGRESS_UPDATE, p);

        (*jniEnv)->DeleteLocalRef(jniEnv, p);
    }
}

void audio_frame_callback(AVFormatContext* pFormatCtx, int stream_index, AVFrame* frame)
{
    int64_t pts = av_frame_get_best_effort_timestamp(frame);
    pts = av_rescale_q(pts, pFormatCtx->streams[stream_index]->time_base, AV_TIME_BASE_Q);
    if (verbose) LOGI("on audio frame %" PRId64, pts);
    while ((start_time + pts) > av_gettime()) {
        ;
    }

    AVFrame* pFrameS16 = av_frame_alloc();
    pFrameS16->nb_samples = frame->nb_samples;
    pFrameS16->channels = audio_channel;
    pFrameS16->sample_rate = audio_sample_rate;

    av_samples_alloc(pFrameS16->data, pFrameS16->linesize, audio_channel, pFrameS16->nb_samples, AV_SAMPLE_FMT_S16P, 0);

    // int buffer_size = av_samples_get_buffer_size(pFrameS16->linesize, audio_channel, pFrameS16->nb_samples,
    //                                              AV_SAMPLE_FMT_S16P, 0);
    // uint8_t* buffer = malloc((size_t) buffer_size);
    // av_samples_fill_arrays(pFrameS16->data, pFrameS16->linesize, buffer, audio_channel, pFrameS16->nb_samples,
    //                        AV_SAMPLE_FMT_S16P, 0);
    int samples = swr_convert(swrContext, pFrameS16->data, pFrameS16->nb_samples, (const uint8_t **) frame->data, frame->nb_samples);

    if (samples > 0) {
        if (pFrameS16->linesize[0] > audio_buffer_size) {
            LOGW("buffer size larger than sink buffer size, should trim to sink buffer size");
        }
        jbyteArray audio_data = (*jniEnv)->NewByteArray(jniEnv, (jsize) pFrameS16->linesize[0]);
        (*jniEnv)->SetByteArrayRegion(jniEnv, audio_data, 0, (jsize) pFrameS16->linesize[0], (const jbyte *) pFrameS16->data[0]);
        send_player_message_with_object(jniEnv, FF_PLAYER_MSG_ON_SOUND_AVAILABLE, audio_data);
        (*jniEnv)->DeleteLocalRef(jniEnv, audio_data);
    }

    av_frame_free(&pFrameS16);
}

void video_frame_callback(AVFormatContext* pFormatCtx, int stream_index, AVFrame* frame)
{
    int64_t pts = av_frame_get_best_effort_timestamp(frame);
    pts = av_rescale_q(pts, pFormatCtx->streams[stream_index]->time_base, AV_TIME_BASE_Q);
    if (verbose) LOGI("on video frame %" PRId64, pts);
    while ((start_time + pts) > av_gettime()) {
        ;
    }

    jdouble progress = (jdouble) pts / (jdouble) pFormatCtx->duration;
    send_progress_update(progress);
    if (verbose) LOGD("current progress %f", progress);

    AVFrame* pRGBFrame = av_frame_alloc();
    uint8_t* pBitmap;
    AndroidBitmapInfo bitmapInfo;
    AndroidBitmap_getInfo(jniEnv, picture_bitmap, &bitmapInfo);
    AndroidBitmap_lockPixels(jniEnv, picture_bitmap, (void **) &pBitmap);

    // avpicture_fill((AVPicture *) pRGBFrame, pBitmap, AV_PIX_FMT_RGBA, video_width, video_height);
    av_image_alloc(pRGBFrame->data, pRGBFrame->linesize, video_width, video_height, AV_PIX_FMT_RGBA, 32);
    av_image_fill_linesizes(pRGBFrame->linesize, AV_PIX_FMT_RGBA, video_width);
    av_image_fill_pointers(pRGBFrame->data, AV_PIX_FMT_RGBA, video_height, pBitmap, pRGBFrame->linesize);
    int height = sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize, 0, pFormatCtx->streams[stream_index]->codecpar->height,
              pRGBFrame->data, pRGBFrame->linesize);
    if (height > 0) {
        if (verbose) LOGD("sws scale done %d", height);
        send_player_message(jniEnv, FF_PLAYER_MSG_ON_PICTURE_AVAILABLE);
    }

    AndroidBitmap_unlockPixels(jniEnv, picture_bitmap);
    av_frame_unref(pRGBFrame);
}

void *start_play(void *data);

int prepare_stream_info(AVFormatContext *pContext, AVCodecContext **pCodecContext, AVCodec **pCodec,
                        enum AVMediaType type);

void decode_frame(AVFormatContext *pFormatCtx, int stream_index, AVCodecContext *pCodecCtx,
                  DecodeCallback *callback, AVPacket *packet);

JNIEXPORT void JNICALL
Java_com_mutter_ffplayer_FFPlayerNative_nativeInitPlayer(JNIEnv *env, jclass type,
                                                         jobject playerHandler,
                                                         jobject pictureBitmap,
                                                         jobject pictureHandler, jint width,
                                                         jint height, jobject soundBuffer,
                                                         jobject soundHandler, jint channel,
                                                         jint sampleRate) {
    player_handler = (*env)->NewGlobalRef(env, playerHandler);
    picture_handler = (*env)->NewGlobalRef(env, pictureHandler);
    sound_handler = (*env)->NewGlobalRef(env, soundHandler);

    audio_buffer = (*env)->GetDirectBufferAddress(env, soundBuffer);
    audio_buffer_size = (*env)->GetDirectBufferCapacity(env, soundBuffer);

    picture_bitmap = (*env)->NewGlobalRef(env, pictureBitmap);
    video_width = width;
    video_height = height;
    audio_channel = channel;
    audio_sample_rate = sampleRate;

    obtain_message = (*env)->GetMethodID(env, (*env)->FindClass(env, "android/os/Handler"),
                                         "obtainMessage", "(I)Landroid/os/Message;");
    if (obtain_message == NULL) {
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }
    }

    send_to_target = (*env)->GetMethodID(env, (*env)->FindClass(env, "android/os/Message"),
                                         "sendToTarget", "()V");
    if (send_to_target == NULL) {
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }
        LOGE("native init player error");
    }

    obtain_message_with_object = (*env)->GetMethodID(env, (*env)->FindClass(env, "android/os/Handler"),
                                                     "obtainMessage", "(ILjava/lang/Object;)Landroid/os/Message;");
    if (obtain_message_with_object == NULL) {
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }
        LOGE("native init player error");
    }
    LOGD("init native player done");
}

void *start_play(void *data) {

    (*javaVM)->AttachCurrentThread(javaVM, &jniEnv, NULL);

    const char* path = *((char**) data);
    int ret;

    LOGI("start play in thread id %" PRId64 " path is %s\n", pthread_self(), path);

    av_register_all();
    avcodec_register_all();

    AVFormatContext* fmt_ctx;
    AVCodecContext* c_ctx_audio = NULL;
    AVCodecContext* c_ctx_video = NULL;
    AVCodec* c_audio;
    AVCodec* c_video;

    fmt_ctx = avformat_alloc_context();
    ret = avformat_open_input(&fmt_ctx, path, NULL, NULL);
    if (ret < 0) {
        LOGD("open input error %s\n", av_err2str(ret));
        goto end;
    }
    avformat_find_stream_info(fmt_ctx, NULL);

    if ((audio_index = prepare_stream_info(fmt_ctx, &c_ctx_audio, &c_audio, AVMEDIA_TYPE_AUDIO)) < 0) {
        LOGD("prepare audio stream info error %s\n", av_err2str(audio_index));
        goto end;
    }

    if ((video_index = prepare_stream_info(fmt_ctx, &c_ctx_video, &c_video, AVMEDIA_TYPE_VIDEO)) < 0) {
        LOGD("prepare video stream info error %s\n", av_err2str(video_index));
        goto end;
    }

    AVPacket* packet = av_packet_alloc();
    av_init_packet(packet);
    packet->data = NULL;
    packet->size = 0;

    pthread_mutex_lock(&pthread_mutex);
    start_time = av_gettime();
    pthread_mutex_unlock(&pthread_mutex);

    send_player_message(jniEnv, FF_PLAYER_MSG_START);

    while (av_read_frame(fmt_ctx, packet) == 0) {
        if (packet->stream_index == audio_index) {
            decode_frame(fmt_ctx, audio_index, c_ctx_audio, &audio_frame_callback, packet);
        } else if (packet->stream_index == video_index) {
            decode_frame(fmt_ctx, video_index, c_ctx_video, &video_frame_callback, packet);
        } else {
            LOGW("unknown stream type\n");
        }

        if (ffPlayerState == FF_PLAYER_STATE_STOPPED) {
            break;
        } else if (ffPlayerState == FF_PLAYER_STATE_PAUSED) {
            pthread_rwlock_rdlock(&pthread_paused_lock);
        } else if (ffPlayerState == FF_PLAYER_STATE_PLAYING) {
            // do nothing
        }
    }
    av_packet_unref(packet);

    send_player_message(jniEnv, FF_PLAYER_MSG_STOP);

    end:
    avformat_close_input(&fmt_ctx);
    avformat_free_context(fmt_ctx);
    if (c_ctx_audio != NULL) {
        avcodec_close(c_ctx_audio);
        avcodec_free_context(&c_ctx_audio);
    }
    if (c_ctx_video != NULL) {
        avcodec_close(c_ctx_video);
        avcodec_free_context(&c_ctx_video);
    }

    (*javaVM)->DetachCurrentThread(javaVM);
    return NULL;
}

void decode_frame(AVFormatContext *pFormatCtx, int stream_index, AVCodecContext *pCodecCtx,
                  DecodeCallback *callback, AVPacket *packet) {
    int packet_ret;
    do {
        packet_ret = avcodec_send_packet(pCodecCtx, packet);
        if (packet_ret == 0) {
            int frame_ret;
            do {
                AVFrame* frame = av_frame_alloc();
                frame_ret = avcodec_receive_frame(pCodecCtx, frame);
                if (callback != NULL) {
                    callback(pFormatCtx, stream_index, frame);
                }
                av_frame_free(&frame);
            } while (frame_ret == 0);
        }
    } while (packet_ret == AVERROR(EAGAIN));
}

int prepare_stream_info(AVFormatContext *pContext, AVCodecContext **pCodecContext, AVCodec **pCodec,
                        enum AVMediaType type) {
    int ret;
    int index = 0;

    ret = av_find_best_stream(pContext, type, -1, -1, pCodec, 0);
    if (ret == AVERROR_STREAM_NOT_FOUND) {
        LOGE("no stream found for media type %d\n", type);
    } else {
        jboolean find_decoder = 0;
        if (ret == AVERROR_DECODER_NOT_FOUND) {
            find_decoder = 1;
        }
        for (unsigned i = 0; i < pContext->nb_streams; ++i) {
            if (pContext->streams[i]->codecpar->codec_type == type) {
                index = i;
                if (find_decoder) {
                    *pCodec = avcodec_find_decoder(pContext->streams[i]->codecpar->codec_id);
                }

                if (*pCodec != NULL) {
                    *pCodecContext = avcodec_alloc_context3(*pCodec);
                    ret = avcodec_parameters_to_context(*pCodecContext, pContext->streams[i]->codecpar);
                } else {
                    LOGE("no decoder found for stream %d\n", i);
                    return AVERROR_DECODER_NOT_FOUND;
                }
            }
        }
    }

    if (*pCodecContext != NULL && *pCodec != NULL) {
        ret = avcodec_open2(*pCodecContext, *pCodec, NULL);
        if (ret < 0) {
            LOGE("could not open decoder for media type %d\n", type);
            return AVERROR_UNKNOWN;
        }
    } else {
        LOGE("fill codec context error %s\n", av_err2str(ret));
        return ret;
    }

    if (type == AVMEDIA_TYPE_AUDIO) {
        swrContext = swr_alloc_set_opts(NULL,
                                        av_get_default_channel_layout(audio_channel), AV_SAMPLE_FMT_S16P, audio_sample_rate,
                                        av_get_default_channel_layout((*pCodecContext)->channels),
                                        (*pCodecContext)->sample_fmt, (*pCodecContext)->sample_rate, 0, NULL);
        if (swr_init(swrContext) < 0) {
            LOGE("swr init failed");
        }
    } else if (type == AVMEDIA_TYPE_VIDEO) {
        swsContext = sws_getContext((*pCodecContext)->width, (*pCodecContext)->height, (*pCodecContext)->pix_fmt,
                                    video_width, video_height, AV_PIX_FMT_RGBA,
                                    SWS_BICUBLIN, NULL, NULL, NULL);
        if (swsContext == NULL) {
            LOGE("sws init failed");
        }
    }
    return index;
}

JNIEXPORT void JNICALL
Java_com_mutter_ffplayer_FFPlayerNative_nativeStartPlayer(JNIEnv *env, jclass type,
                                                          jstring dataSource_) {
    const char *dataSource = (*env)->GetStringUTFChars(env, dataSource_, 0);

    data_source = malloc(strlen(dataSource) + 1);
    snprintf(data_source, strlen(dataSource) + 1, dataSource);

    LOGD("start player, will play %s\n", data_source);

    (*env)->GetJavaVM(env, &javaVM);

    pthread_mutex_init(&pthread_mutex, NULL);
    pthread_rwlock_init(&pthread_paused_lock, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&player_thread, &attr, &start_play, &data_source);
    pthread_attr_destroy(&attr);

    (*env)->ReleaseStringUTFChars(env, dataSource_, dataSource);
}

JNIEXPORT void JNICALL
Java_com_mutter_ffplayer_FFPlayerNative_nativePausePlayer(JNIEnv *env, jclass type) {
    acquire_pause_lock();
    ffPlayerState = FF_PLAYER_STATE_PAUSED;
}

JNIEXPORT void JNICALL
Java_com_mutter_ffplayer_FFPlayerNative_nativeResumePlayer(JNIEnv *env, jclass type) {
    ffPlayerState = FF_PLAYER_STATE_PLAYING;
    release_pause_lock();
}

JNIEXPORT void JNICALL
Java_com_mutter_ffplayer_FFPlayerNative_nativeStopPlayer(JNIEnv *env, jclass type) {
    ffPlayerState = FF_PLAYER_STATE_STOPPED;
}

JNIEXPORT void JNICALL
Java_com_mutter_ffplayer_FFPlayerNative_nativeReleasePlayer(JNIEnv *env, jclass type) {
    LOGD("native release player");
    swr_free(&swrContext);
    sws_freeContext(swsContext);
    pthread_mutex_destroy(&pthread_mutex);
    pthread_rwlock_destroy(&pthread_paused_lock);
    (*env)->DeleteGlobalRef(env, player_handler);
    (*env)->DeleteGlobalRef(env, picture_handler);
    (*env)->DeleteGlobalRef(env, sound_handler);
    (*env)->DeleteGlobalRef(env, picture_bitmap);
}
