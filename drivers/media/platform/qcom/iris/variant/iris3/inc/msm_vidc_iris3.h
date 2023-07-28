/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_IRIS3_H_
#define _MSM_VIDC_IRIS3_H_

#include "msm_vidc_core.h"

int msm_vidc_init_iris3(struct msm_vidc_core *core);
int msm_vidc_adjust_bitrate_boost_iris3(void *instance, struct v4l2_ctrl *ctrl);

#endif // _MSM_VIDC_IRIS3_H_
