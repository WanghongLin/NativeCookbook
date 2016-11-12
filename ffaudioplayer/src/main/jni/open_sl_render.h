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

#ifndef NATIVECOOKBOOK_OPEN_SL_RENDER_H
#define NATIVECOOKBOOK_OPEN_SL_RENDER_H

#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdint.h>
#include <pthread.h>

#define TAG "SLRender"

typedef struct Sample {
    void *sample;
    size_t size;
    struct Sample* next;
} Sample;

typedef struct SLRender {
    int bqPlayerSampleRate;

    /**
     * Not used currently
     */
    int bqPlayerBufferSize;

    SLObjectItf slEngineObj;
    SLEngineItf slEngineItf;

    SLObjectItf slOutputMixObj;
    SLEnvironmentalReverbItf slEnvironmentalReverbItf;

    SLObjectItf slPlayerObj;
    SLPlayItf slPlayItf;
    SLAndroidSimpleBufferQueueItf bufferQueueItf;
    SLEffectSendItf slEffectSendItf;
    SLVolumeItf slVolumeItf;

    struct Sample* sampleHeader;
    struct Sample* sampleEnd;
    uint32_t writePosition;
    uint32_t readPosition;
    pthread_rwlock_t sampleLock;
    void (*writeSamples) (struct SLRender* slRender, void* samples, size_t size);
    void (*stop) (struct SLRender* slRender);
} SLRender;

SLRender *createSLRender(int sampleRate, int bufferSize);
void destroySLRender(SLRender* slRender);
void writeSamples(struct SLRender* slRender, void* samples, size_t size);
void _bufferQueueCallback(SLAndroidSimpleBufferQueueItf bufferQueueItf, void* context);
void _createEngine(SLRender* slRender);
void _createBufferQueueAudioPlayer(SLRender* slRender);
void _checkOpenSLESResult(const char* FILE, int line, SLresult result);
void _stop(struct SLRender* slRender);

#endif //NATIVECOOKBOOK_OPEN_SL_RENDER_H
