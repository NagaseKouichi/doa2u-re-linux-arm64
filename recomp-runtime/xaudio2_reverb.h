/* SPDX-License-Identifier: GPL-3.0-or-later */
/* XAudio 2.9 reverb declarations that mingw-w64's xaudio2fx.h omits.
   The I3DL2 conversion is adapted from FAudio's ReverbConvertI3DL2ToNative9
   (zlib license; see NOTICE). Altered: made a template over the parameter
   types and given the Windows SDK's DisableLateField. xaudio2_reverb_test.cpp
   checks the layout and every converted byte against the Windows SDK. */
#ifndef RECOMP_XAUDIO2_REVERB_H
#define RECOMP_XAUDIO2_REVERB_H

#include <windows.h>
#include <xaudio2fx.h>

#include <cmath>
#include <cstdint>

namespace recomp {

#pragma pack(push, 1)
struct ReverbParameters {
    float WetDryMix;
    UINT32 ReflectionsDelay;
    BYTE ReverbDelay, RearDelay, SideDelay, PositionLeft, PositionRight,
        PositionMatrixLeft, PositionMatrixRight, EarlyDiffusion, LateDiffusion,
        LowEQGain, LowEQCutoff, HighEQGain, HighEQCutoff;
    float RoomFilterFreq, RoomFilterMain, RoomFilterHF, ReflectionsGain, ReverbGain,
        DecayTime, Density, RoomSize;
    BOOL DisableLateField;
};

struct ReverbI3dl2Parameters {
    float WetDryMix;
    INT32 Room, RoomHF;
    float RoomRolloffFactor, DecayTime, DecayHFRatio;
    INT32 Reflections;
    float ReflectionsDelay;
    INT32 Reverb;
    float ReverbDelay, Diffusion, Density, HFReference;
};
#pragma pack(pop)

template <typename I3dl2, typename Native>
void reverb_i3dl2_to_native(const I3dl2 *in, Native *out, BOOL seven_dot_one)
{
    out->RearDelay = seven_dot_one ? 20 : 5;
    out->SideDelay = 5;
    out->PositionLeft = out->PositionRight = 6;
    out->PositionMatrixLeft = out->PositionMatrixRight = 27;
    out->RoomSize = 100.0f;
    out->LowEQCutoff = 4;
    out->HighEQCutoff = 6;
    out->RoomFilterMain = (float)in->Room / 100.0f;
    out->RoomFilterHF = (float)in->RoomHF / 100.0f;
    // The SDK promotes the ratio to double; float log10 rounds 0.1 to a different gain.
    if (in->DecayHFRatio >= 1.0f) {
        int32_t index = (int32_t)(-4.0 * std::log10((double)in->DecayHFRatio));
        if (index < -8) index = -8;
        out->LowEQGain = (BYTE)(index < 0 ? index + 8 : 8);
        out->HighEQGain = 8;
        out->DecayTime = in->DecayTime * in->DecayHFRatio;
    } else {
        int32_t index = (int32_t)(4.0 * std::log10((double)in->DecayHFRatio));
        if (index < -8) index = -8;
        out->LowEQGain = 8;
        out->HighEQGain = (BYTE)(index < 0 ? index + 8 : 8);
        out->DecayTime = in->DecayTime;
    }
    float reflections_delay = in->ReflectionsDelay * 1000.0f;
    if (reflections_delay >= 300) reflections_delay = 299.0f;
    else if (reflections_delay <= 1) reflections_delay = 1;
    out->ReflectionsDelay = (UINT32)reflections_delay;
    float reverb_delay = in->ReverbDelay * 1000.0f;
    if (reverb_delay >= 85) reverb_delay = 84.0f;
    out->ReverbDelay = (BYTE)reverb_delay;
    out->ReflectionsGain = in->Reflections / 100.0f;
    out->ReverbGain = in->Reverb / 100.0f;
    out->EarlyDiffusion = (BYTE)(15.0f * in->Diffusion / 100.0f);
    out->LateDiffusion = out->EarlyDiffusion;
    out->Density = in->Density;
    out->RoomFilterFreq = in->HFReference;
    out->WetDryMix = in->WetDryMix;
    out->DisableLateField = FALSE;
}

} // namespace recomp

#if defined(__MINGW32__)
typedef recomp::ReverbParameters XAUDIO2FX_REVERB_PARAMETERS;
typedef recomp::ReverbI3dl2Parameters XAUDIO2FX_REVERB_I3DL2_PARAMETERS;
inline HRESULT XAudio2CreateReverb(IUnknown **apo) { return CreateAudioReverb(apo); }
inline void ReverbConvertI3DL2ToNative(const XAUDIO2FX_REVERB_I3DL2_PARAMETERS *in,
    XAUDIO2FX_REVERB_PARAMETERS *out, BOOL seven_dot_one = FALSE)
{
    recomp::reverb_i3dl2_to_native(in, out, seven_dot_one);
}
#endif

#endif
