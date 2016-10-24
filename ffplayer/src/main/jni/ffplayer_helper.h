//
// Created by mutter on 8/18/16.
//

#ifndef NATIVECOOKBOOK_FFPLAYER_HELPER_H
#define NATIVECOOKBOOK_FFPLAYER_HELPER_H


#include <jni.h>
#include "ffplayer.h"
#include <android/log.h>
#include <libavformat/avformat.h>

#define TAG "ffplayer_helper"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL, TAG, __VA_ARGS__)

void send_player_message(JNIEnv* env, FFPlayerMSGType ffPlayerMSGType);
void send_player_message_with_object(JNIEnv* env, FFPlayerMSGType ffPlayerMSGType, jobject object);

typedef void (DecodeCallback)(AVFormatContext* pFormatCtx, int stream_index, AVFrame* frame);

#endif //NATIVECOOKBOOK_FFPLAYER_HELPER_H
