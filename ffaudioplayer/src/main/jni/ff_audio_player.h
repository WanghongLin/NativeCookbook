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

#ifndef NATIVECOOKBOOK_FF_AUDIO_PLAYER_H
#define NATIVECOOKBOOK_FF_AUDIO_PLAYER_H

#include <jni.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>

#include <android/log.h>
#include <pthread.h>

#define TAG "ff_audio_player"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, TAG, __VA_ARGS__)

typedef enum OutputMode {
    OUTPUT_MODE_OPEN_SL_ES = 0,
    OUTPUT_MODE_AUDIO_TRACK_SINGLE_BUFFER,
    OUTPUT_MODE_AUDIO_TRACK_BYTE_ARRAY
} OutputMode;

typedef enum PlayerEvent {
    PLAYER_EVENT_ON_AUDIO_DATA_AVAILABLE = 0,
    PLAYER_EVENT_INIT,
    PLAYER_EVENT_START,
    PLAYER_EVENT_PAUSE,
    PLAYER_EVENT_STOP,
    PLAYER_EVENT_DESTROY
} PlayerEvent;

typedef struct FFAudioPlayer {

    JavaVM* javaVM;
    JNIEnv* jniEnv;

    jmethodID mid_obtainMessage;
    jmethodID mid_sendToTarget;
    jobject handler;

    const char* dataSource;
    int channels;
    int sampleRate;
    enum AVSampleFormat sampleFormat;

    int outputMode;
    void* output_buffer;
    jlong output_buffer_size;

    int64_t startTime;
    AVFormatContext* pFormatCtx;
    int audioIndex;
    SwrContext* swrContext;
    AVCodecContext* pCodecCtx;

    void (*initPlayer) (struct FFAudioPlayer* audioPlayer, JNIEnv* env);
    void (*destroyPlayer) (struct FFAudioPlayer* audioPlayer);
    void (*setDataSource) (struct FFAudioPlayer* audioPlayer, const char* dataSource);
    void (*sendEvent) (struct FFAudioPlayer* audioPlayer, int what, int arg1, int arg2, jobject obj);
    void (*startPlayer) (struct FFAudioPlayer* audioPlayer);
    void (*pausePlayer) (struct FFAudioPlayer* audioPlayer);
    void (*stopPlayer) (struct FFAudioPlayer* audioPlayer);
    void (*decodeFrame) (struct FFAudioPlayer* audioPlayer, AVPacket* packet);
    void (*playFrame) (struct FFAudioPlayer* audioPlayer, AVFrame* frame);
} FFAudioPlayer;

void init_audio_player(FFAudioPlayer** pAudioPlayer);
void destroy_audio_player(FFAudioPlayer* audioPlayer);

void initPlayer(struct FFAudioPlayer* audioPlayer, JNIEnv* env);
void destroyPlayer(struct FFAudioPlayer* audioPlayer);
void sendEvent(struct FFAudioPlayer* audioPlayer, int what, int arg1, int arg2, jobject obj);
void startPlayer(struct FFAudioPlayer* audioPlayer);
void pausePlayer(struct FFAudioPlayer* audioPlayer);
void stopPlayer(struct FFAudioPlayer* audioPlayer);
void setDataSource(struct FFAudioPlayer* audioPlayer, const char* dataSource);
void* playRoutine(void* data);
void decodeFrame(struct FFAudioPlayer* audioPlayer, AVPacket* packet);
void playFrame(struct FFAudioPlayer* audioPlayer, AVFrame* frame);
#endif //NATIVECOOKBOOK_FF_AUDIO_PLAYER_H
