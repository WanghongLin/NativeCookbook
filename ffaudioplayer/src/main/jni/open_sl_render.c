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
// Created by mutter on 11/12/16.
//

#include <malloc.h>
#include <memory.h>
#include "open_sl_render.h"

SLRender *createSLRender(int sampleRate, int bufferSize) {
    SLRender* slRender = malloc(sizeof(SLRender));

    slRender->writePosition = 0;
    slRender->readPosition = 0;
    slRender->sampleHeader = NULL;
    slRender->sampleEnd = NULL;
    slRender->writeSamples = &writeSamples;
    slRender->stop = &_stop;
    slRender->bqPlayerSampleRate = sampleRate * 1000;
    slRender->bqPlayerBufferSize = bufferSize;
    pthread_rwlock_init(&slRender->sampleLock, NULL);

    _createEngine(slRender);
    _createBufferQueueAudioPlayer(slRender);

    return slRender;
}

void _createEngine(SLRender *slRender) {

    SLresult result;

    // create engine
    result = slCreateEngine(&slRender->slEngineObj, 0, NULL, 0, NULL, NULL);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // realize the engine
    result = (*slRender->slEngineObj)->Realize(slRender->slEngineObj, SL_BOOLEAN_FALSE);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // get the engine interface, which is needed in order to create other objects
    result = (*slRender->slEngineObj)->GetInterface(slRender->slEngineObj, SL_IID_ENGINE, &slRender->slEngineItf);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*slRender->slEngineItf)->CreateOutputMix(slRender->slEngineItf, &slRender->slOutputMixObj, 1, ids, req);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // realize the output mix
    result = (*slRender->slOutputMixObj)->Realize(slRender->slOutputMixObj, SL_BOOLEAN_FALSE);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // get the environmental reverb interface
    // this could fail if the environmental reverb effect is not available,
    // either because the feature is not present, excessive CPU load, or
    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
    result = (*slRender->slOutputMixObj)->GetInterface(slRender->slOutputMixObj, SL_IID_ENVIRONMENTALREVERB,
                                              &slRender->slEnvironmentalReverbItf);
    SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_ROOM;
    if (SL_RESULT_SUCCESS == result) {
        result = (*slRender->slEnvironmentalReverbItf)->SetEnvironmentalReverbProperties(
                slRender->slEnvironmentalReverbItf, &reverbSettings);
        _checkOpenSLESResult(__FILE__, __LINE__, result);
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "crete engine");
}

void _createBufferQueueAudioPlayer(SLRender *slRender) {
    SLresult result;
    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_8,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
    /*
     * Enable Fast Audio when possible:  once we set the same rate to be the native, fast audio path
     * will be triggered
     */
    if (slRender->bqPlayerSampleRate) {
        format_pcm.samplesPerSec = (SLuint32) slRender->bqPlayerSampleRate; // sample rate in mili second
    }
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, slRender->slOutputMixObj};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    /*
     * create audio player:
     *     fast audio does not support when SL_IID_EFFECTSEND is required, skip it
     *     for fast audio case
     */
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/ };

    result = (*slRender->slEngineItf)->CreateAudioPlayer(slRender->slEngineItf, &slRender->slPlayerObj, &audioSrc, &audioSnk,
                                                slRender->bqPlayerSampleRate ? 2 : 3, ids, req);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // realize the player
    result = (*slRender->slPlayerObj)->Realize(slRender->slPlayerObj, SL_BOOLEAN_FALSE);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // get the play interface
    result = (*slRender->slPlayerObj)->GetInterface(slRender->slPlayerObj, SL_IID_PLAY, &slRender->slPlayItf);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // get the buffer queue interface
    result = (*slRender->slPlayerObj)->GetInterface(slRender->slPlayerObj, SL_IID_BUFFERQUEUE,
                                             &slRender->bufferQueueItf);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // register callback on the buffer queue
    result = (*slRender->bufferQueueItf)->RegisterCallback(slRender->bufferQueueItf, &_bufferQueueCallback, slRender);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // get the effect send interface
    slRender->slEffectSendItf = NULL;
    if( 0 == slRender->bqPlayerSampleRate) {
        result = (*slRender->slPlayerObj)->GetInterface(slRender->slPlayerObj, SL_IID_EFFECTSEND,
                                                 &slRender->slEffectSendItf);
        _checkOpenSLESResult(__FILE__, __LINE__, result);
    }
#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
    // get the mute/solo interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_MUTESOLO, &bqPlayerMuteSolo);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;
#endif

    // get the volume interface
    result = (*slRender->slPlayerObj)->GetInterface(slRender->slPlayerObj, SL_IID_VOLUME, &slRender->slVolumeItf);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    // set the player's state to playing
    result = (*slRender->slPlayItf)->SetPlayState(slRender->slPlayItf, SL_PLAYSTATE_PLAYING);
    _checkOpenSLESResult(__FILE__, __LINE__, result);

    __android_log_print(ANDROID_LOG_INFO, TAG, "create buffer queue audio player");
}

void _checkOpenSLESResult(const char *FILE, int line, SLresult result) {
    if (result != SL_RESULT_SUCCESS) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "%s %d %d", FILE, line, result);
    }
}

void _bufferQueueCallback(SLAndroidSimpleBufferQueueItf bufferQueueItf, void *context) {
    SLRender* slRender = context;
    __android_log_print(ANDROID_LOG_INFO, TAG, "buffer queue callback #1 %d %d", slRender->readPosition, slRender->writePosition);
    pthread_rwlock_rdlock(&slRender->sampleLock);
    if (slRender->readPosition < slRender->writePosition) {
        uint32_t i = 0;
        Sample* sample = slRender->sampleHeader;
        while (sample->next != NULL) {
            sample = sample->next;
            i++;
            if (slRender->readPosition == i) {
                __android_log_print(ANDROID_LOG_INFO, TAG, "buffer queue callback #2");
                break;
            }
        }

        if (sample != NULL) {
            slRender->readPosition++;
            __android_log_print(ANDROID_LOG_INFO, TAG, "buffer queue callback #3");
            (*bufferQueueItf)->Enqueue(bufferQueueItf, sample->sample, (SLuint32) sample->size);
        }
    } else {
        // FIXME: feed with silence data or the last available chunk?
        Sample* sample = slRender->sampleHeader;
        while (sample->next != NULL) {
            sample = sample->next;
        }
        if (sample != NULL) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "buffer queue callback waiting #4");
            (*bufferQueueItf)->Enqueue(bufferQueueItf, sample->sample, (SLuint32) sample->size);
        }
    }
    pthread_rwlock_unlock(&slRender->sampleLock);
}

void writeSamples(struct SLRender *slRender, void *samples, size_t size) {
    if (size > 0) {
        Sample *sample = malloc(sizeof(struct Sample));
        sample->sample = malloc(size);
        sample->size = size;
        sample->next = NULL;
        memcpy(sample->sample, samples, size);

        if (slRender->sampleHeader == NULL) {
            slRender->sampleHeader = sample;
            slRender->sampleEnd = sample;
            (*slRender->bufferQueueItf)->Enqueue(slRender->bufferQueueItf, sample->sample,
                                                 (SLuint32) sample->size);
        } else {
            slRender->sampleEnd->next = sample;
            slRender->sampleEnd = sample;
        }

        pthread_rwlock_wrlock(&slRender->sampleLock);
        slRender->writePosition++;
        pthread_rwlock_unlock(&slRender->sampleLock);
    }
}

void _stop(struct SLRender *slRender) {
    (*slRender->slPlayItf)->SetPlayState(slRender->slPlayItf, SL_PLAYSTATE_STOPPED);
}

void destroySLRender(SLRender *slRender) {
    pthread_rwlock_destroy(&slRender->sampleLock);
    if (slRender->sampleHeader != NULL) {
        Sample* sample = slRender->sampleHeader;
        while (sample->next != NULL) {
            Sample* thisSample = sample;
            sample = sample->next;
            if (thisSample->sample != NULL) {
                free(thisSample->sample);
            }
            free(thisSample);
        }
        if (sample->sample != NULL) {
            free(sample->sample);
        }
        free(sample);
    }
    free(slRender);
}
