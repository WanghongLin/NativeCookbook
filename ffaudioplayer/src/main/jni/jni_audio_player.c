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

#include "jni_audio_player.h"
#include "ff_audio_player.h"

static FFAudioPlayer* audioPlayer;

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativeInit(JNIEnv * env, jclass clz, jint channels, jint sampleRate) {
    init_audio_player(&audioPlayer);

    (*env)->GetJavaVM(env, &audioPlayer->javaVM);
    audioPlayer->channels = channels;
    audioPlayer->sampleRate = sampleRate;
    audioPlayer->sampleFormat = AV_SAMPLE_FMT_S16P;
    audioPlayer->outputMode = OUTPUT_MODE_OPEN_SL_ES;
    audioPlayer->initPlayer(audioPlayer, env);
}

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativeSetDataSource(JNIEnv * env, jclass clz, jstring dataSource) {
    if (audioPlayer != NULL) {
        jboolean isCopy = 1;
        const char* path = (*env)->GetStringUTFChars(env, dataSource, &isCopy);
        audioPlayer->setDataSource(audioPlayer, path);
        (*env)->ReleaseStringUTFChars(env, dataSource, path);
    }
}

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativeSetOutputSink(JNIEnv * env, jclass clz, jobject outBuffer) {
    audioPlayer->output_buffer = (*env)->GetDirectBufferAddress(env, outBuffer);
    audioPlayer->output_buffer_size = (*env)->GetDirectBufferCapacity(env, outBuffer);
}

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativeSetOutputMode(JNIEnv * env, jclass clz, jint outputMode) {
    audioPlayer->outputMode = outputMode;
}

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativeSetHandler(JNIEnv * env, jclass clz, jobject handler) {
    audioPlayer->handler = (*env)->NewGlobalRef(env, handler);
}

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativeStart(JNIEnv * env, jclass clz) {
    if (audioPlayer != NULL) {
        audioPlayer->startPlayer(audioPlayer);
    }
}

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativePause(JNIEnv * env, jclass clz) {
    if (audioPlayer != NULL) {
        audioPlayer->pausePlayer(audioPlayer);
    }
}

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativeStop(JNIEnv * env, jclass clz) {
    if (audioPlayer != NULL) {
        audioPlayer->stopPlayer(audioPlayer);
    }
}

void Java_com_mutter_ffaudioplayer_FFAudioPlayer_nativeDestroy(JNIEnv * env, jclass clz) {
    if (audioPlayer != NULL) {
        audioPlayer->destroyPlayer(audioPlayer);
        destroy_audio_player(audioPlayer);
    }
}

