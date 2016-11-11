// Copyright (C) 2016 mutter
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


//
// Created by mutter on 11/8/16.
//

#include "ff_audio_player.h"

void init_audio_player(FFAudioPlayer** pAudioPlayer) {
    (*pAudioPlayer) = malloc(sizeof(FFAudioPlayer));

    (*pAudioPlayer)->initPlayer = &initPlayer;
    (*pAudioPlayer)->destroyPlayer = &destroyPlayer;
    (*pAudioPlayer)->sendEvent = &sendEvent;
    (*pAudioPlayer)->setDataSource = &setDataSource;

    (*pAudioPlayer)->startPlayer = &startPlayer;
    (*pAudioPlayer)->pausePlayer = &pausePlayer;
    (*pAudioPlayer)->stopPlayer = &stopPlayer;
    (*pAudioPlayer)->destroyPlayer = &destroyPlayer;

    (*pAudioPlayer)->decodeFrame = &decodeFrame;
    (*pAudioPlayer)->playFrame = &playFrame;
}

void destroy_audio_player(FFAudioPlayer* audioPlayer) {
    if (audioPlayer != NULL) {

        audioPlayer->initPlayer = NULL;
        audioPlayer->destroyPlayer = NULL;
        audioPlayer->sendEvent = NULL;
        audioPlayer->setDataSource = NULL;

        audioPlayer->startPlayer = NULL;
        audioPlayer->pausePlayer = NULL;
        audioPlayer->stopPlayer = NULL;
        audioPlayer->destroyPlayer = NULL;

        audioPlayer->decodeFrame = NULL;
        audioPlayer->playFrame = NULL;

        free(audioPlayer);
    }
}

void initPlayer(FFAudioPlayer* audioPlayer, JNIEnv* env) {
    jclass clzHandler = (*env)->FindClass(env, "android/os/Handler");
    jclass clzMessage = (*env)->FindClass(env, "android/os/Message");

    audioPlayer->mid_obtainMessage = (*env)->GetMethodID(env, clzHandler, "obtainMessage",
                                                         "(IIILjava/lang/Object;)Landroid/os/Message;");
    audioPlayer->mid_sendToTarget = (*env)->GetMethodID(env, clzMessage, "sendToTarget", "()V");
    if (audioPlayer->mid_obtainMessage == NULL || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        LOGE("can not find method obtainMessage");
    }

    if (audioPlayer->mid_sendToTarget == NULL || (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        LOGE("can not find method sendToTarget\n");
    }

    av_register_all();
    avcodec_register_all();
    LOGI("init audio player\n");
}

void destroyPlayer(FFAudioPlayer* audioPlayer) {
    JNIEnv* env;
    (*audioPlayer->javaVM)->GetEnv(audioPlayer->javaVM, (void **) &env, JNI_VERSION_1_1);
    if (audioPlayer->handler != NULL) {
        (*env)->DeleteGlobalRef(env, audioPlayer->handler);
    }
    if (audioPlayer->dataSource != NULL) {
        free((void *) audioPlayer->dataSource);
    }
    if (audioPlayer->mid_sendToTarget != NULL) {
        (*env)->DeleteLocalRef(env, audioPlayer->mid_sendToTarget);
    }
    if (audioPlayer->mid_obtainMessage != NULL) {
        (*env)->DeleteLocalRef(env, audioPlayer->mid_obtainMessage);
    }
}

void sendEvent(struct FFAudioPlayer* audioPlayer, int what, int arg1, int arg2, jobject obj) {
    if (audioPlayer->handler != NULL) {
        jobject message = (*audioPlayer->jniEnv)->
                CallObjectMethod(audioPlayer->jniEnv, audioPlayer->handler, audioPlayer->mid_obtainMessage,
                                 what, arg1, arg2, obj);
        (*audioPlayer->jniEnv)->CallVoidMethod(audioPlayer->jniEnv, message, audioPlayer->mid_sendToTarget);
        (*audioPlayer->jniEnv)->DeleteLocalRef(audioPlayer->jniEnv, message);
    }
}

void startPlayer(struct FFAudioPlayer* audioPlayer) {
    pthread_t pthread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&pthread, &attr, &playRoutine, audioPlayer);

    pthread_attr_destroy(&attr);
}

void pausePlayer(struct FFAudioPlayer* audioPlayer) {

}

void stopPlayer(struct FFAudioPlayer* audioPlayer) {

}

void setDataSource(struct FFAudioPlayer* audioPlayer, const char* dataSource) {
    audioPlayer->dataSource = malloc(strlen(dataSource)+1);
    memset((void *) audioPlayer->dataSource, 0, strlen(dataSource)+1);
    memcpy((void *) audioPlayer->dataSource, dataSource, strlen(dataSource));
    LOGD("setDataSource %s", audioPlayer->dataSource);

    int ret;
    AVCodec* codec;
    audioPlayer->pFormatCtx = avformat_alloc_context();
    if ((ret = avformat_open_input(&audioPlayer->pFormatCtx, audioPlayer->dataSource, NULL, NULL)) < 0) {
        LOGE("error input input %s\n", av_err2str(ret));
        goto end;
    }

    avformat_find_stream_info(audioPlayer->pFormatCtx, NULL);

    ret = av_find_best_stream(audioPlayer->pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (ret != AVERROR_STREAM_NOT_FOUND) {
        jboolean find_decoder = 0;
        if (ret == AVERROR_DECODER_NOT_FOUND) {
            find_decoder = 1;
        }
        for (unsigned i = 0; i < audioPlayer->pFormatCtx->nb_streams; ++i) {
            if (audioPlayer->pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioPlayer->audioIndex = i;
                if (find_decoder) {
                    codec = avcodec_find_decoder(audioPlayer->pFormatCtx->streams[i]->codecpar->codec_id);
                }
                break;
            }
        }
    } else {
        LOGE("no audio stream found %s\n", av_err2str(ret));
        goto end;
    }

    if (codec != NULL) {
        audioPlayer->pCodecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(audioPlayer->pCodecCtx,
                                      audioPlayer->pFormatCtx->streams[audioPlayer->audioIndex]->codecpar);
        ret = avcodec_open2(audioPlayer->pCodecCtx, codec, NULL);
        if (ret < 0) {
            LOGE("open codec error %s\n", av_err2str(ret));
        }
    } else {
        LOGE("can not find audio codec\n");
    }

    audioPlayer->swrContext =
            swr_alloc_set_opts(NULL,
                               av_get_default_channel_layout(audioPlayer->channels), audioPlayer->sampleFormat, audioPlayer->sampleRate,
                               av_get_default_channel_layout(audioPlayer->pCodecCtx->channels), audioPlayer->pCodecCtx->sample_fmt,
                               audioPlayer->pCodecCtx->sample_rate,
                               0, NULL);
    if ((ret = swr_init(audioPlayer->swrContext)) < 0) {
        LOGE("swr init failed %s\n", av_err2str(ret));
    }

    end:
    return;
}

void *playRoutine(void* data) {
    FFAudioPlayer* audioPlayer = data;
    (*audioPlayer->javaVM)->AttachCurrentThread(audioPlayer->javaVM, &audioPlayer->jniEnv, NULL);

    (*audioPlayer->javaVM)->GetEnv(audioPlayer->javaVM, (void **) &audioPlayer->jniEnv, JNI_VERSION_1_1);

    AVPacket* packet = av_packet_alloc();
    av_init_packet(packet);
    packet->data = NULL;
    packet->size = 0;

    audioPlayer->startTime = av_gettime();
    audioPlayer->sendEvent(audioPlayer, PLAYER_EVENT_START, 0, 0, NULL);
    while (av_read_frame(audioPlayer->pFormatCtx, packet) == 0) {
        if (packet->stream_index == audioPlayer->audioIndex) {
            audioPlayer->decodeFrame(audioPlayer, packet);
        }
    }
    audioPlayer->sendEvent(audioPlayer, PLAYER_EVENT_STOP, 0, 0, NULL);
    av_packet_unref(packet);

    swr_free(&audioPlayer->swrContext);
    avcodec_close(audioPlayer->pCodecCtx);
    avformat_close_input(&audioPlayer->pFormatCtx);
    avcodec_free_context(&audioPlayer->pCodecCtx);
    avformat_free_context(audioPlayer->pFormatCtx);

    (*audioPlayer->javaVM)->DetachCurrentThread(audioPlayer->javaVM);

    return NULL;
}

void decodeFrame(struct FFAudioPlayer* audioPlayer, AVPacket* packet) {
    int packet_ret;
    do {
        packet_ret = avcodec_send_packet(audioPlayer->pCodecCtx, packet);
        if (packet_ret == 0) {

            int frame_ret;
            do {
                AVFrame* frame = av_frame_alloc();
                frame_ret = avcodec_receive_frame(audioPlayer->pCodecCtx, frame);
                if (frame_ret == 0) {
                    audioPlayer->playFrame(audioPlayer, frame);
                }
                av_frame_unref(frame);
            } while (frame_ret == 0);
        }
    } while (packet_ret == AVERROR(EAGAIN));
}

void playFrame(struct FFAudioPlayer* audioPlayer, AVFrame* frame) {
    int64_t pts = av_frame_get_best_effort_timestamp(frame);
    pts = av_rescale_q(pts, audioPlayer->pFormatCtx->streams[audioPlayer->audioIndex]->time_base, AV_TIME_BASE_Q);
    while (audioPlayer->startTime + pts > av_gettime()) {
        ;
    }
    // LOGI("play audio frame %" PRId64 "\n", pts);
    AVFrame* frameS16 = av_frame_alloc();

    int ret;
    frameS16->channels = audioPlayer->channels;
    frameS16->nb_samples = frame->nb_samples;
    frameS16->sample_rate = audioPlayer->sampleRate;

    ret = av_samples_alloc(frameS16->data, frameS16->linesize, frameS16->channels, frameS16->nb_samples, audioPlayer->sampleFormat, 0);
    if (ret < 0) {
        LOGE("play frame error %s\n", av_err2str(ret));
    }

    ret = swr_convert(audioPlayer->swrContext, frameS16->data, frameS16->nb_samples, (const uint8_t **) frame->data, frame->nb_samples);
    if (ret < 0) {
        LOGE("play frame error %s\n", av_err2str(ret));
    }

    memcpy(audioPlayer->output_buffer, frameS16->data[0], (size_t) frameS16->linesize[0]);
//    LOGI("convert audio frame, sample %d -> %d, sample rate %d -> %d\n", frame->nb_samples, frameS16->nb_samples,
//         audioPlayer->pCodecCtx->sample_rate, sample_rate);

    audioPlayer->sendEvent(audioPlayer, PLAYER_EVENT_ON_AUDIO_DATA_AVAILABLE, frameS16->linesize[0], 0, NULL);

    av_frame_free(&frameS16);
}
