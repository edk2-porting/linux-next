/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021,, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_PLATFORM_H_
#define _MSM_VIDC_PLATFORM_H_

#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>

#include "msm_vidc_internal.h"
#include "msm_vidc_core.h"

#define DDR_TYPE_LPDDR4   0x6
#define DDR_TYPE_LPDDR4X  0x7
#define DDR_TYPE_LPDDR5   0x8
#define DDR_TYPE_LPDDR5X  0x9

#define UBWC_CONFIG(mc, ml, hbb, bs1, bs2, bs3, bsp) \
{	                                                 \
	.max_channels = mc,                              \
	.mal_length = ml,                                \
	.highest_bank_bit = hbb,                         \
	.bank_swzl_level = bs1,                          \
	.bank_swz2_level = bs2,                          \
	.bank_swz3_level = bs3,                          \
	.bank_spreading = bsp,                           \
}

struct bw_table {
	const char      *name;
	u32              min_kbps;
	u32              max_kbps;
};

struct pd_table {
	const char      *name;
};

struct regulator_table {
	const char      *name;
	bool             hw_trigger;
};

struct clk_table {
	const char      *name;
	u32              clk_id;
	bool             scaling;
};

struct clk_rst_table {
	const char      *name;
	bool             exclusive_release;
};

struct subcache_table {
	const char      *name;
	u32              llcc_id;
};

struct context_bank_table {
	const char      *name;
	u32              start;
	u32              size;
	bool             secure;
	bool             dma_coherant;
	u32              region;
	u64              dma_mask;
};

struct freq_table {
	unsigned long    freq;
};

struct reg_preset_table {
	u32              reg;
	u32              value;
	u32              mask;
};

struct msm_vidc_ubwc_config_data {
	u32              max_channels;
	u32              mal_length;
	u32              highest_bank_bit;
	u32              bank_swzl_level;
	u32              bank_swz2_level;
	u32              bank_swz3_level;
	u32              bank_spreading;
};

struct codec_info {
	u32 v4l2_codec;
	enum msm_vidc_codec_type vidc_codec;
	const char *pixfmt_name;
};

struct color_format_info {
	u32 v4l2_color_format;
	enum msm_vidc_colorformat_type vidc_color_format;
	const char *pixfmt_name;
};

struct color_primaries_info {
	u32 v4l2_color_primaries;
	enum msm_vidc_color_primaries vidc_color_primaries;
};

struct transfer_char_info {
	u32 v4l2_transfer_char;
	enum msm_vidc_transfer_characteristics vidc_transfer_char;
};

struct matrix_coeff_info {
	u32 v4l2_matrix_coeff;
	enum msm_vidc_matrix_coefficients vidc_matrix_coeff;
};

struct msm_platform_core_capability {
	enum msm_vidc_core_capability_type type;
	u32 value;
};

struct msm_platform_inst_capability {
	enum msm_vidc_inst_capability_type cap_id;
	enum msm_vidc_domain_type domain;
	enum msm_vidc_codec_type codec;
	s32 min;
	s32 max;
	u32 step_or_mask;
	s32 value;
	u32 v4l2_id;
	u32 hfi_id;
	enum msm_vidc_inst_capability_flags flags;
};

struct msm_platform_inst_cap_dependency {
	enum msm_vidc_inst_capability_type cap_id;
	enum msm_vidc_domain_type domain;
	enum msm_vidc_codec_type codec;
	enum msm_vidc_inst_capability_type children[MAX_CAP_CHILDREN];
	int (*adjust)(void *inst,
		      struct v4l2_ctrl *ctrl);
	int (*set)(void *inst,
		   enum msm_vidc_inst_capability_type cap_id);
};

struct msm_vidc_compat_handle {
	const char *compat;
	int (*init_platform)(struct msm_vidc_core *core);
	int (*init_iris)(struct msm_vidc_core *core);
};

struct msm_vidc_format_capability {
	struct codec_info *codec_info;
	u32 codec_info_size;
	struct color_format_info *color_format_info;
	u32 color_format_info_size;
	struct color_primaries_info *color_prim_info;
	u32 color_prim_info_size;
	struct transfer_char_info *transfer_char_info;
	u32 transfer_char_info_size;
	struct matrix_coeff_info *matrix_coeff_info;
	u32 matrix_coeff_info_size;
};

struct msm_vidc_platform_data {
	const struct bw_table *bw_tbl;
	unsigned int bw_tbl_size;
	const struct regulator_table *regulator_tbl;
	unsigned int regulator_tbl_size;
	const struct pd_table *pd_tbl;
	unsigned int pd_tbl_size;
	const char * const *opp_tbl;
	unsigned int opp_tbl_size;
	const struct clk_table *clk_tbl;
	unsigned int clk_tbl_size;
	const struct clk_rst_table *clk_rst_tbl;
	unsigned int clk_rst_tbl_size;
	const struct subcache_table *subcache_tbl;
	unsigned int subcache_tbl_size;
	const struct context_bank_table *context_bank_tbl;
	unsigned int context_bank_tbl_size;
	struct freq_table *freq_tbl;
	unsigned int freq_tbl_size;
	const struct reg_preset_table *reg_prst_tbl;
	unsigned int reg_prst_tbl_size;
	struct msm_vidc_ubwc_config_data *ubwc_config;
	const char *fwname;
	u32 pas_id;
	struct msm_platform_core_capability *core_data;
	u32 core_data_size;
	struct msm_platform_inst_capability *inst_cap_data;
	u32 inst_cap_data_size;
	struct msm_platform_inst_cap_dependency *inst_cap_dependency_data;
	u32 inst_cap_dependency_data_size;
	struct msm_vidc_format_capability *format_data;
	const u32 *psc_avc_tbl;
	unsigned int psc_avc_tbl_size;
	const u32 *psc_hevc_tbl;
	unsigned int psc_hevc_tbl_size;
	const u32 *psc_vp9_tbl;
	unsigned int psc_vp9_tbl_size;
	const u32 *dec_input_prop_avc;
	unsigned int dec_input_prop_size_avc;
	const u32 *dec_input_prop_hevc;
	unsigned int dec_input_prop_size_hevc;
	const u32 *dec_input_prop_vp9;
	unsigned int dec_input_prop_size_vp9;
	const u32 *dec_output_prop_avc;
	unsigned int dec_output_prop_size_avc;
	const u32 *dec_output_prop_hevc;
	unsigned int dec_output_prop_size_hevc;
	const u32 *dec_output_prop_vp9;
	unsigned int dec_output_prop_size_vp9;
};

struct msm_vidc_platform {
	struct msm_vidc_platform_data data;
};

static inline bool is_sys_cache_present(struct msm_vidc_core *core)
{
	return !!core->platform->data.subcache_tbl_size;
}

int msm_vidc_init_platform(struct msm_vidc_core *core);

/* control framework support functions */

enum msm_vidc_inst_capability_type msm_vidc_get_cap_id(struct msm_vidc_inst *inst, u32 id);
int msm_vidc_update_cap_value(struct msm_vidc_inst *inst, u32 cap,
			      s32 adjusted_val, const char *func);
bool is_parent_available(struct msm_vidc_inst *inst, u32 cap_id,
			 u32 check_parent, const char *func);
int msm_vidc_get_parent_value(struct msm_vidc_inst *inst, u32 cap, u32 parent,
			      s32 *value, const char *func);
u32 msm_vidc_get_port_info(struct msm_vidc_inst *inst,
			   enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_v4l2_menu_to_hfi(struct msm_vidc_inst *inst,
			      enum msm_vidc_inst_capability_type cap_id, u32 *value);
int msm_vidc_v4l2_to_hfi_enum(struct msm_vidc_inst *inst,
			      enum msm_vidc_inst_capability_type cap_id, u32 *value);
int msm_vidc_packetize_control(struct msm_vidc_inst *inst,
			       enum msm_vidc_inst_capability_type cap_id, u32 payload_type,
			       void *hfi_val, u32 payload_size, const char *func);
int msm_vidc_adjust_bitrate(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_layer_bitrate(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_bitrate_mode(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_entropy_mode(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_profile(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_ltr_count(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_use_ltr(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_mark_ltr(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_delta_based_rc(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_output_order(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_input_buf_host_max_count(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_output_buf_host_max_count(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_transform_8x8(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_chroma_qp_index_offset(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_slice_count(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_layer_count(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_gop_size(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_b_frame(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_peak_bitrate(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_hevc_min_qp(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_hevc_max_qp(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_hevc_i_frame_qp(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_hevc_p_frame_qp(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_hevc_b_frame_qp(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_bitrate_boost(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_min_quality(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_all_intra(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_dec_slice_mode(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_adjust_ir_period(void *instance, struct v4l2_ctrl *ctrl);
int msm_vidc_set_header_mode(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_deblock_mode(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_min_qp(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_max_qp(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_frame_qp(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_req_sync_frame(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_chroma_qp_index_offset(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_slice_count(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_layer_count_and_type(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_gop_size(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_bitrate(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_layer_bitrate(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_u32(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_u32_packed(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_u32_enum(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_constant_quality(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_cbr_related_properties(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_use_and_mark_ltr(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_nal_length(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_flip(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_rotation(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_ir_period(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_stage(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_pipe(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_level(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_q16(void *instance, enum msm_vidc_inst_capability_type cap_id);
int msm_vidc_set_vui_timing_info(void *instance, enum msm_vidc_inst_capability_type cap_id);

#endif // _MSM_VIDC_PLATFORM_H_
