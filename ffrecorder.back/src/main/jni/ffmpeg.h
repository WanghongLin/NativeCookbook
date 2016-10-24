//
// Created by mutter on 5/12/16.
//

#ifndef NATIVECOOKBOOK_FFMPEG_H
#define NATIVECOOKBOOK_FFMPEG_H

#include <android/log.h>
#include "libavformat/avformat.h"

#define TAG "FFMPEG"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

int start_logger(const char* app_name);
int init_video_filter(AVCodecContext *ic_ctx);

#endif //NATIVECOOKBOOK_FFMPEG_H
