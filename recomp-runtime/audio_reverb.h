// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DOA2U_RECOMP_AUDIO_REVERB_H
#define DOA2U_RECOMP_AUDIO_REVERB_H

#include "audio_output.h"

/* The stereo I3DL2 reverb behind the SDL audio backend: XAudio2's reverb
   submix as FAudio reimplements it (audio_reverb.cpp). XAudio2 converts I3DL2
   to its native parameters with ReverbConvertI3DL2ToNative; this does the
   same, with the effect fully wet, as audio_output_xaudio2.cpp sets it. */
typedef struct RecompAudioReverbState RecompAudioReverbState;

RecompAudioReverbState *recomp_audio_reverb_create(int sample_rate);
void recomp_audio_reverb_destroy(RecompAudioReverbState *reverb);
void recomp_audio_reverb_set_i3dl2(RecompAudioReverbState *reverb, const RecompAudioReverb *parameters);
/* Replaces interleaved stereo float frames with the reverb's output. */
void recomp_audio_reverb_process(RecompAudioReverbState *reverb, float *frames, int frame_count);

#endif
