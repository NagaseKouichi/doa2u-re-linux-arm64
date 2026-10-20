/* Altered from FAudio's src/FAudioFX_reverb.c (FAudio 26.10): only the stereo
 * reverb network and ReverbConvertI3DL2ToNative remain, in C++, without the
 * FAPO wrapper. The original notice follows.
 *
 * FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2024 Ethan Lee, Luigi Auriemma, and the MonoGame Team
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */
#include "audio_reverb.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <new>
#include <vector>

namespace {

float dbGainToFactor(float gain)
{
    return std::pow(10.0f, gain / 20.0f);
}

uint32_t msToSamples(float msec, int32_t sample_rate)
{
    return static_cast<uint32_t>((sample_rate * msec) / 1000.0f);
}

constexpr float kMaxDelayMs = 300.0f;

struct Delay {
    int32_t sample_rate = 0;
    uint32_t capacity = 0, delay = 0, read = 0, write = 0;
    std::vector<float> buffer;

    void initialize(int32_t rate, float delay_ms)
    {
        sample_rate = rate;
        capacity = msToSamples(kMaxDelayMs, rate);
        delay = msToSamples(delay_ms, rate);
        read = 0;
        write = delay;
        buffer.assign(capacity, 0.0f);
    }

    void change(float delay_ms)
    {
        delay = msToSamples(delay_ms, sample_rate);
        read = (write - delay + capacity) % capacity;
    }

    float take()
    {
        const float out = buffer[read];
        read = (read + 1u) % capacity;
        return out;
    }

    void put(float sample)
    {
        buffer[write] = sample;
        write = (write + 1u) % capacity;
    }

    float process(float sample)
    {
        const float out = take();
        put(sample);
        return out;
    }
};

float combFeedbackFromRt60(const Delay &delay, float rt60_ms)
{
    return std::pow(10.0f, (-3.0f * delay.delay * 1000.0f) / (delay.sample_rate * rt60_ms));
}

struct BiQuad {
    int32_t sample_rate = 0;
    float a0 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0, c0 = 0, d0 = 0;
    float delay0 = 0, delay1 = 0;

    void change(bool low_shelving, float frequency, float gain)
    {
        const float two_pi = 6.283185307179586476925286766559005f;
        const float theta_c = (two_pi * frequency) / static_cast<float>(sample_rate);
        const float mu = dbGainToFactor(gain);
        const float beta = low_shelving ? 4.0f / (1 + mu) : (1 + mu) / 4.0f;
        const float delta = beta * std::tan(theta_c * 0.5f);
        const float gamma = (1 - delta) / (1 + delta);
        if (low_shelving) {
            a0 = (1 - gamma) * 0.5f;
            a1 = a0;
        } else {
            a0 = (1 + gamma) * 0.5f;
            a1 = -a0;
        }
        a2 = 0.0f;
        b1 = -gamma;
        b2 = 0.0f;
        c0 = mu - 1.0f;
        d0 = 1.0f;
    }

    void initialize(int32_t rate, bool low_shelving, float frequency, float gain)
    {
        sample_rate = rate;
        delay0 = delay1 = 0.0f;
        change(low_shelving, frequency, gain);
    }

    // Direct Form II Transposed.
    float process(float sample)
    {
        const float result = (a0 * sample) + delay0;
        delay0 = (a1 * sample) - (b1 * result) + delay1;
        delay1 = (a2 * sample) - (b2 * result);
        return (result * c0) + (sample * d0);
    }
};

struct CombShelving {
    Delay delay;
    float feedback_gain = 0;
    BiQuad low, high;

    void initialize(int32_t rate, float delay_ms, float rt60_ms, float low_frequency, float low_gain,
        float high_frequency, float high_gain)
    {
        delay.initialize(rate, delay_ms);
        feedback_gain = combFeedbackFromRt60(delay, rt60_ms);
        low.initialize(rate, true, low_frequency, low_gain);
        high.initialize(rate, false, high_frequency, high_gain);
    }

    float process(float sample)
    {
        const float out = delay.take();
        float feedback = high.process(out);
        feedback = low.process(feedback);
        delay.put(sample + feedback_gain * feedback);
        return out;
    }
};

struct AllPass {
    Delay delay;
    float feedback_gain = 0;

    void initialize(int32_t rate, float delay_ms, float gain)
    {
        delay.initialize(rate, delay_ms);
        feedback_gain = gain;
    }

    void change(float delay_ms, float gain)
    {
        delay.change(delay_ms);
        feedback_gain = gain;
    }

    float process(float sample)
    {
        const float out = delay.take();
        const float to_buffer = sample + feedback_gain * out;
        delay.put(to_buffer);
        return out - feedback_gain * to_buffer;
    }
};

/* Reverb network - loosely based on the reverberator from "Designing Audio
   Effect Plug-Ins in C++" by Will Pirkle and the classic Schroeder-Moorer
   reverberator, with modifications to fit the XAudio2FX parameters. */
constexpr int kCombCount = 8, kApfInCount = 1, kApfOutCount = 4;
constexpr float kCombDelays[kCombCount] = {25.31f, 26.94f, 28.96f, 30.75f, 32.24f, 33.80f, 35.31f, 36.67f};
constexpr float kApfInDelays[kApfInCount] = {13.28f};
constexpr float kApfOutDelays[kApfOutCount] = {5.10f, 12.61f, 10.0f, 7.73f};
// Stereo spread: the right channel's taps are 0.5216 ms longer.
constexpr float kSpreadMs[2] = {0.0f, 0.5216f};

struct NativeParameters {
    float wet_dry_mix;
    uint32_t reflections_delay;
    uint8_t reverb_delay, rear_delay, position_left, position_right;
    uint8_t position_matrix_left, position_matrix_right;
    uint8_t early_diffusion, late_diffusion, low_eq_gain, low_eq_cutoff, high_eq_gain, high_eq_cutoff;
    float room_filter_freq, room_filter_main, room_filter_hf, reflections_gain, reverb_gain;
    float decay_time, density, room_size;
};

/* XAudio2's ReverbConvertI3DL2ToNative, as FAudio implements it. */
NativeParameters convertI3dl2(const RecompAudioReverb &i3dl2)
{
    NativeParameters native{};
    native.rear_delay = 5;
    native.position_left = native.position_right = 6;
    native.position_matrix_left = native.position_matrix_right = 27;
    native.room_size = 100.0f;
    native.low_eq_cutoff = 4;
    native.high_eq_cutoff = 6;
    native.room_filter_main = static_cast<float>(i3dl2.room) / 100.0f;
    native.room_filter_hf = static_cast<float>(i3dl2.room_hf) / 100.0f;
    if (i3dl2.decay_hf_ratio >= 1.0f) {
        int32_t index = static_cast<int32_t>(-4.0 * std::log10(i3dl2.decay_hf_ratio));
        if (index < -8) index = -8;
        native.low_eq_gain = static_cast<uint8_t>(index < 0 ? index + 8 : 8);
        native.high_eq_gain = 8;
        native.decay_time = i3dl2.decay_time * i3dl2.decay_hf_ratio;
    } else {
        int32_t index = static_cast<int32_t>(4.0 * std::log10(i3dl2.decay_hf_ratio));
        if (index < -8) index = -8;
        native.low_eq_gain = 8;
        native.high_eq_gain = static_cast<uint8_t>(index < 0 ? index + 8 : 8);
        native.decay_time = i3dl2.decay_time;
    }
    float reflections_delay = i3dl2.reflections_delay * 1000.0f;
    if (reflections_delay >= 300.0f) reflections_delay = 299.0f;
    else if (reflections_delay <= 1.0f) reflections_delay = 1.0f;
    native.reflections_delay = static_cast<uint32_t>(reflections_delay);
    float reverb_delay = i3dl2.reverb_delay * 1000.0f;
    if (reverb_delay >= 85.0f) reverb_delay = 84.0f;
    native.reverb_delay = static_cast<uint8_t>(reverb_delay);
    native.reflections_gain = i3dl2.reflections / 100.0f;
    native.reverb_gain = i3dl2.reverb / 100.0f;
    native.early_diffusion = static_cast<uint8_t>(15.0f * i3dl2.diffusion / 100.0f);
    native.late_diffusion = native.early_diffusion;
    native.density = i3dl2.density;
    native.room_filter_freq = i3dl2.hf_reference;
    // audio_output_xaudio2.cpp passes WetDryMix 100.
    native.wet_dry_mix = 100.0f;
    return native;
}

struct Channel {
    Delay reverb_delay;
    CombShelving comb[kCombCount];
    AllPass apf_out[kApfOutCount];
    BiQuad room_high_shelf;
    float early_gain = 1.0f;
    float gain = 1.0f;
};

} // namespace

struct RecompAudioReverbState {
    Delay early_delay;
    AllPass apf_in[kApfInCount];
    Channel channel[2];
    float early_gain = 1.0f, reverb_gain = 1.0f, room_gain = 1.0f;
    float wet_ratio = 1.0f, dry_ratio = 0.0f;

    explicit RecompAudioReverbState(int32_t rate)
    {
        early_delay.initialize(rate, 10.0f);
        for (int i = 0; i < kApfInCount; ++i) apf_in[i].initialize(rate, kApfInDelays[i], 0.5f);
        for (int c = 0; c < 2; ++c) {
            channel[c].reverb_delay.initialize(rate, 10.0f);
            for (int i = 0; i < kCombCount; ++i)
                channel[c].comb[i].initialize(rate, kCombDelays[i] + kSpreadMs[c], 500, 500, -6, 5000, -6);
            for (int i = 0; i < kApfOutCount; ++i)
                channel[c].apf_out[i].initialize(rate, kApfOutDelays[i] + kSpreadMs[c], 0.5f);
            channel[c].room_high_shelf.initialize(rate, false, 5000, -10);
        }
    }

    void set(const NativeParameters &p)
    {
        early_delay.change(static_cast<float>(p.reflections_delay));
        const float early_diffusion = 0.6f - ((p.early_diffusion / 15.0f) * 0.2f);
        for (int i = 0; i < kApfInCount; ++i) apf_in[i].change(kApfInDelays[i], early_diffusion);
        for (int c = 0; c < 2; ++c) {
            channel[c].reverb_delay.change(static_cast<float>(p.reverb_delay));
            for (int i = 0; i < kCombCount; ++i) {
                CombShelving &comb = channel[c].comb[i];
                comb.delay.change(kCombDelays[i] + kSpreadMs[c]);
                comb.feedback_gain = combFeedbackFromRt60(comb.delay, std::max(p.decay_time, 0.1f) * 1000.0f);
                comb.low.change(true, 50.0f + p.low_eq_cutoff * 50.0f, p.low_eq_gain - 8.0f);
                comb.high.change(false, 1000 + p.high_eq_cutoff * 500.0f, p.high_eq_gain - 8.0f);
            }
        }
        early_gain = dbGainToFactor(p.reflections_gain);
        reverb_gain = dbGainToFactor(p.reverb_gain);
        room_gain = dbGainToFactor(p.room_filter_main);
        const float late_diffusion = 0.6f - ((p.late_diffusion / 15.0f) * 0.2f);
        for (int c = 0; c < 2; ++c) {
            for (int i = 0; i < kApfOutCount; ++i)
                channel[c].apf_out[i].change(kApfOutDelays[i] + kSpreadMs[c], late_diffusion);
            channel[c].room_high_shelf.change(false, p.room_filter_freq, p.room_filter_main + p.room_filter_hf);
            const float matrix = c == 0 ? p.position_matrix_left : p.position_matrix_right;
            channel[c].gain = 1.5f - (matrix / 27.0f) * 0.5f;
            const float position = c == 0 ? p.position_left : p.position_right;
            channel[c].early_gain = (1.2f - (position / 6.0f) * 0.2f) * early_gain;
        }
        wet_ratio = p.wet_dry_mix / 100.0f;
        dry_ratio = 1.0f - wet_ratio;
    }

    float processEarly(float sample)
    {
        float early = early_delay.process(sample);
        for (int i = 0; i < kApfInCount; ++i) early = apf_in[i].process(early);
        return early;
    }

    float processChannel(Channel &ch, float sample)
    {
        const float delayed = ch.reverb_delay.process(sample);
        float out = 0.0f;
        for (int i = 0; i < kCombCount; ++i) out += ch.comb[i].process(delayed);
        out /= static_cast<float>(kCombCount);
        for (int i = 0; i < kApfOutCount; ++i) out = ch.apf_out[i].process(out);
        const float early_late = (sample * ch.early_gain) + (out * reverb_gain);
        return ch.room_high_shelf.process(early_late * room_gain) * ch.gain;
    }
};

RecompAudioReverbState *recomp_audio_reverb_create(int sample_rate)
{
    return new (std::nothrow) RecompAudioReverbState(sample_rate);
}

void recomp_audio_reverb_destroy(RecompAudioReverbState *reverb)
{
    delete reverb;
}

void recomp_audio_reverb_set_i3dl2(RecompAudioReverbState *reverb, const RecompAudioReverb *parameters)
{
    if (reverb != nullptr && parameters != nullptr) reverb->set(convertI3dl2(*parameters));
}

void recomp_audio_reverb_process(RecompAudioReverbState *reverb, float *frames, int frame_count)
{
    for (int i = 0; i < frame_count; ++i) {
        float *frame = frames + i * 2;
        // Combine two channels into one for the shared early reflections.
        const float early = reverb->processEarly((frame[0] + frame[1]) / 2.0f);
        const float left = reverb->processChannel(reverb->channel[0], early) * reverb->wet_ratio +
            frame[0] * reverb->dry_ratio;
        const float right = reverb->processChannel(reverb->channel[1], early) * reverb->wet_ratio +
            frame[1] * reverb->dry_ratio;
        frame[0] = left;
        frame[1] = right;
    }
}
