/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio_capture_xapo.cpp"
#include "audio_output_xaudio2.cpp"

static bool check(const char *label, bool condition)
{
    if (!condition) std::fprintf(stderr, "FAIL native audio %s\n", label);
    return condition;
}

int main()
{
    if (!check("environment", _putenv_s("RECOMP_AUDIO_GAIN", "1") == 0 &&
            _putenv_s("RECOMP_AUDIO_CAPTURE", "") == 0)) return 1;
    recomp_audio_output_initialize();
    bool passed = check("endpoint", recomp_audio_output_enabled() && recomp_audio_output_ready());
    if (passed) passed = check("master mute", SUCCEEDED(master->SetVolume(0.0f)));
    if (passed) {
        int16_t pcm[800];
        for (uint32_t i = 0; i < 800; ++i) pcm[i] = (i % 40u < 20u) ? 1000 : -1000;
        recomp_audio_output_submit(0, reinterpret_cast<const uint8_t *>(pcm),
            sizeof pcm, 8000, 1, 16, 0);
        passed = check("submission", recomp_audio_output_enabled() && voices[0].source &&
            submitted_buffers == 1 && submitted_bytes == sizeof pcm && nonzero_buffers == 1);
        std::memset(pcm, 0, sizeof pcm);
        XAUDIO2_VOICE_STATE state{};
        const ULONGLONG deadline = GetTickCount64() + 2000;
        while (passed && GetTickCount64() < deadline) {
            recomp_audio_output_initialize();
            passed = check("device during consumption", recomp_audio_output_enabled() != 0);
            if (!passed) break;
            voices[0].source->GetState(&state, 0);
            if (state.BuffersQueued == 0) break;
            Sleep(5);
        }
        passed &= check("consumed PCM", state.BuffersQueued == 0 &&
            state.SamplesPlayed >= 800 + 8000 * kPrerollMs / 1000);
        recomp_audio_output_reset_voice(0);
        passed &= check("reset", voices[0].source == nullptr);
    }
    recomp_audio_output_shutdown();
    recomp_audio_output_shutdown();
    passed &= check("shutdown", !recomp_audio_output_enabled() && !recomp_audio_output_ready() && !master && !com_initialized);
    if (passed) std::puts("PASS real XAudio2 endpoint, copied PCM consumption, reset and shutdown");
    return passed ? 0 : 1;
}
