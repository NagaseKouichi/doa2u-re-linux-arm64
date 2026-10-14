/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dsound_mix_model.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL mix line %d\n", __LINE__); return 1; } } while (0)
int main(void)
{
    RecompDsoundMixModel model, prior;
    uint32_t headroom[32] = {0};
    float dry[4], wet[4];
    recomp_dsound_mix_default(&model);
    CHECK(recomp_dsound_mix_matrices(&model, 2, headroom, dry, wet));
    CHECK(dry[0] == 1 && dry[1] == 0 && dry[2] == 0 && dry[3] == 1 && wet[0] == 0);
    const RecompDsoundMixPair speakers[6] = {{0,0},{1,0},{4,-10000},{5,-10000},{3,-10000},{2,-10000}};
    CHECK(recomp_dsound_mix_set(&model, speakers, 6, true));
    CHECK(recomp_dsound_mix_matrices(&model, 2, headroom, dry, wet) && dry[0]==1 && dry[3]==1);
    RecompDsoundMixPair pairs[3] = {{0,-2000},{1,0},{10,0}};
    CHECK(recomp_dsound_mix_set(&model, pairs, 3, true));
    headroom[10] = 1;
    CHECK(recomp_dsound_mix_matrices(&model, 1, headroom, dry, wet));
    CHECK(fabsf(dry[0] - .1f) < .00001f && dry[1] == 1 && wet[0] == .5f && wet[1] == .5f);
    pairs[0] = (RecompDsoundMixPair){30,-10000};
    CHECK(recomp_dsound_mix_set(&model, pairs, 1, false) && model.count == 3 && model.volumes[30] == -10000);
    prior = model;
    pairs[0].volume = 1;
    CHECK(!recomp_dsound_mix_set(&model, pairs, 1, false) && memcmp(&prior,&model,sizeof model)==0);
    pairs[0] = (RecompDsoundMixPair){11,0};
    CHECK(recomp_dsound_mix_set(&model, pairs, 1, true));
    CHECK(!recomp_dsound_mix_matrices(&model, 2, headroom, dry, wet));
    CHECK(recomp_dsound_mix_set(&model, NULL, 0, true));
    CHECK(recomp_dsound_mix_matrices(&model, 2, headroom, dry, wet) && dry[0]==0 && wet[0]==0);
    return 0;
}
