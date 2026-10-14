/* SPDX-License-Identifier: GPL-3.0-or-later */

int recomp_apu_model_test(void);
int recomp_ohci_model_test(void);
int recomp_fiber_model_test(void);
int recomp_directory_model_test(void);
int recomp_symbolic_link_model_test(void);
int recomp_cri_service_model_test(void);
int recomp_d3d_frame_model_test(void);

int main(void)
{
    int passed = 1;
    passed &= recomp_apu_model_test();
    passed &= recomp_ohci_model_test();
    passed &= recomp_fiber_model_test();
    passed &= recomp_directory_model_test();
    passed &= recomp_symbolic_link_model_test();
    passed &= recomp_cri_service_model_test();
    passed &= recomp_d3d_frame_model_test();
    return passed ? 0 : 1;
}
