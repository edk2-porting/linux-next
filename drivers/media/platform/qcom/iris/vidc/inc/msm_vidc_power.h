/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_POWER_H_
#define _MSM_VIDC_POWER_H_

#include "msm_vidc_debug.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_internal.h"

#define COMPRESSION_RATIO_MAX 5

/* TODO: Move to dtsi OR use source clock instead of branch clock.*/
#define MSM_VIDC_CLOCK_SOURCE_SCALING_RATIO 3

enum vidc_bus_type {
	PERF,
	DDR,
	LLCC,
};

/*
 * Minimum dimensions for which to calculate bandwidth.
 * This means that anything bandwidth(0, 0) ==
 * bandwidth(BASELINE_DIMENSIONS.width, BASELINE_DIMENSIONS.height)
 */
static const struct {
	int height, width;
} BASELINE_DIMENSIONS = {
	.width = 1280,
	.height = 720,
};

/* converts Mbps to bps (the "b" part can be bits or bytes based on context) */
#define kbps(__mbps) ((__mbps) * 1000)
#define bps(__mbps) (kbps(__mbps) * 1000)

static inline u32 get_type_frm_name(const char *name)
{
	if (!strcmp(name, "venus-llcc"))
		return LLCC;
	else if (!strcmp(name, "venus-ddr"))
		return DDR;
	else
		return PERF;
}

#define DUMP_HEADER_MAGIC 0xdeadbeef
#define DUMP_FP_FMT "%FP" /* special format for fp_t */

struct dump {
	char *key;
	char *format;
	size_t val;
};

void __dump(struct dump dump[], int len);

static inline bool __ubwc(enum msm_vidc_colorformat_type f)
{
	switch (f) {
	case MSM_VIDC_FMT_NV12C:
	case MSM_VIDC_FMT_TP10C:
		return true;
	default:
		return false;
	}
}

static inline int __bpp(enum msm_vidc_colorformat_type f)
{
	switch (f) {
	case MSM_VIDC_FMT_NV12:
	case MSM_VIDC_FMT_NV21:
	case MSM_VIDC_FMT_NV12C:
	case MSM_VIDC_FMT_RGBA8888C:
		return 8;
	case MSM_VIDC_FMT_P010:
	case MSM_VIDC_FMT_TP10C:
		return 10;
	default:
		d_vpr_e("Unsupported colorformat (%x)", f);
		return INT_MAX;
	}
}

u64 msm_vidc_max_freq(struct msm_vidc_inst *inst);
int msm_vidc_scale_power(struct msm_vidc_inst *inst, bool scale_buses);
void msm_vidc_power_data_reset(struct msm_vidc_inst *inst);

#endif
