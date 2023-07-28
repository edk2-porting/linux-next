/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_STATE_H_
#define _MSM_VIDC_STATE_H_

#include "msm_vidc_internal.h"

enum msm_vidc_core_state {
	MSM_VIDC_CORE_DEINIT,
	MSM_VIDC_CORE_INIT_WAIT,
	MSM_VIDC_CORE_INIT,
	MSM_VIDC_CORE_ERROR,
};

enum msm_vidc_core_sub_state {
	CORE_SUBSTATE_NONE                   = 0x0,
	CORE_SUBSTATE_POWER_ENABLE           = BIT(0),
	CORE_SUBSTATE_GDSC_HANDOFF           = BIT(1),
	CORE_SUBSTATE_PM_SUSPEND             = BIT(2),
	CORE_SUBSTATE_FW_PWR_CTRL            = BIT(3),
	CORE_SUBSTATE_PAGE_FAULT             = BIT(4),
	CORE_SUBSTATE_CPU_WATCHDOG           = BIT(5),
	CORE_SUBSTATE_VIDEO_UNRESPONSIVE     = BIT(6),
	CORE_SUBSTATE_MAX                    = BIT(7),
};

enum msm_vidc_core_event_type {
	CORE_EVENT_NONE                      = BIT(0),
	CORE_EVENT_UPDATE_SUB_STATE          = BIT(1),
};

enum msm_vidc_state {
	MSM_VIDC_OPEN,
	MSM_VIDC_INPUT_STREAMING,
	MSM_VIDC_OUTPUT_STREAMING,
	MSM_VIDC_STREAMING,
	MSM_VIDC_CLOSE,
	MSM_VIDC_ERROR,
};

#define MSM_VIDC_SUB_STATE_NONE          0
#define MSM_VIDC_MAX_SUB_STATES          6
/*
 * max value of inst->sub_state if all
 * the 6 valid bits are set i.e 111111==>63
 */
#define MSM_VIDC_MAX_SUB_STATE_VALUE     ((1 << MSM_VIDC_MAX_SUB_STATES) - 1)

enum msm_vidc_sub_state {
	MSM_VIDC_DRAIN                     = BIT(0),
	MSM_VIDC_DRC                       = BIT(1),
	MSM_VIDC_DRAIN_LAST_BUFFER         = BIT(2),
	MSM_VIDC_DRC_LAST_BUFFER           = BIT(3),
	MSM_VIDC_INPUT_PAUSE               = BIT(4),
	MSM_VIDC_OUTPUT_PAUSE              = BIT(5),
};

enum msm_vidc_event {
	MSM_VIDC_TRY_FMT,
	MSM_VIDC_S_FMT,
	MSM_VIDC_REQBUFS,
	MSM_VIDC_S_CTRL,
	MSM_VIDC_STREAMON,
	MSM_VIDC_STREAMOFF,
	MSM_VIDC_CMD_START,
	MSM_VIDC_CMD_STOP,
	MSM_VIDC_BUF_QUEUE,
};

/* core statemachine functions */
enum msm_vidc_allow msm_vidc_allow_core_state_change(struct msm_vidc_core *core,
						     enum msm_vidc_core_state req_state);
int msm_vidc_update_core_state(struct msm_vidc_core *core,
			       enum msm_vidc_core_state request_state, const char *func);
bool core_in_valid_state(struct msm_vidc_core *core);
bool is_core_state(struct msm_vidc_core *core, enum msm_vidc_core_state state);
bool is_core_sub_state(struct msm_vidc_core *core, enum msm_vidc_core_sub_state sub_state);
const char *core_state_name(enum msm_vidc_core_state state);
const char *core_sub_state_name(enum msm_vidc_core_sub_state sub_state);

/* inst statemachine functions */
bool is_drc_pending(struct msm_vidc_inst *inst);
bool is_drain_pending(struct msm_vidc_inst *inst);
int msm_vidc_update_state(struct msm_vidc_inst *inst,
			  enum msm_vidc_state request_state, const char *func);
int msm_vidc_change_state(struct msm_vidc_inst *inst,
			  enum msm_vidc_state request_state, const char *func);
int msm_vidc_change_sub_state(struct msm_vidc_inst *inst,
			      enum msm_vidc_sub_state clear_sub_state,
			      enum msm_vidc_sub_state set_sub_state,
			      const char *func);
const char *state_name(enum msm_vidc_state state);
const char *sub_state_name(enum msm_vidc_sub_state sub_state);
bool is_state(struct msm_vidc_inst *inst, enum msm_vidc_state state);
bool is_sub_state(struct msm_vidc_inst *inst,
		  enum msm_vidc_sub_state sub_state);

#endif // _MSM_VIDC_STATE_H_
