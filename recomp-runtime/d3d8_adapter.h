/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOA2U_RECOMP_D3D8_ADAPTER_H
#define DOA2U_RECOMP_D3D8_ADAPTER_H

#include "runtime.h"
#include "d3d_presenter.h"

#ifdef __cplusplus
extern "C" {
#endif

RecompFunction recomp_d3d8_lookup_manual(uint32_t address);
uint32_t recomp_d3d8_require_device(void);
void recomp_d3d8_submit(const RecompD3dPresenterCommand *command);
void recomp_d3d8_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
