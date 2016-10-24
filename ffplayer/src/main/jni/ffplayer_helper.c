//
// Created by mutter on 8/18/16.
//

#include <stddef.h>
#include "ffplayer_helper.h"

extern jobject player_handler;
extern jobject picture_handler;
extern jobject sound_handler;
extern jmethodID obtain_message;
extern jmethodID send_to_target;
extern jmethodID obtain_message_with_object;

static const jboolean verbose;

static void __send_player_message(JNIEnv *env, jobject handler, FFPlayerMSGType ffPlayerMSGType,
                                 jobject object) {
    if (handler == NULL || obtain_message == NULL) {
        LOGE("send player message error");
        return;
    }
    jobject message;
    if (object != NULL) {
        message = (*env)->CallObjectMethod(env, handler, obtain_message_with_object, (int) ffPlayerMSGType, object);
    } else {
        message = (*env)->CallObjectMethod(env, handler, obtain_message, (int) ffPlayerMSGType);
    }

    if (message != NULL) {
        (*env)->CallVoidMethod(env, message, send_to_target);
        if (verbose) LOGI("send player message %d\n", ffPlayerMSGType);
    }

    (*env)->DeleteLocalRef(env, message);
}

void send_player_message(JNIEnv *env, FFPlayerMSGType ffPlayerMSGType) {
    switch (ffPlayerMSGType) {
        case FF_PLAYER_MSG_START:
        case FF_PLAYER_MSG_STOP:
        case FF_PLAYER_MSG_PROGRESS_UPDATE:
            __send_player_message(env, player_handler, ffPlayerMSGType, NULL);
            break;
        case FF_PLAYER_MSG_ON_PICTURE_AVAILABLE:
            __send_player_message(env, picture_handler, ffPlayerMSGType, NULL);
            break;
        case FF_PLAYER_MSG_ON_SOUND_AVAILABLE:
            __send_player_message(env, sound_handler, ffPlayerMSGType, NULL);
            break;
        default:
            break;
    }
}

void send_player_message_with_object(JNIEnv* env, FFPlayerMSGType ffPlayerMSGType, jobject object) {
    switch (ffPlayerMSGType) {
        case FF_PLAYER_MSG_START:
        case FF_PLAYER_MSG_STOP:
        case FF_PLAYER_MSG_PROGRESS_UPDATE:
            __send_player_message(env, player_handler, ffPlayerMSGType, object);
            break;
        case FF_PLAYER_MSG_ON_PICTURE_AVAILABLE:
            __send_player_message(env, picture_handler, ffPlayerMSGType, object);
            break;
        case FF_PLAYER_MSG_ON_SOUND_AVAILABLE:
            __send_player_message(env, sound_handler, ffPlayerMSGType, object);
            break;
        default:
            break;
    }
}
