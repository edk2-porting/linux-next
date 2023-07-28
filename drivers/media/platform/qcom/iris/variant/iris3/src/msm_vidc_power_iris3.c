// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "msm_vidc_core.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_power.h"
#include "msm_vidc_power_iris3.h"
#include "perf_static_model.h"

static int msm_vidc_init_codec_input_freq(struct msm_vidc_inst *inst, u32 data_size,
					  struct api_calculation_input *codec_input)
{
	enum msm_vidc_port_type port;
	u32 color_fmt;

	if (is_encode_session(inst)) {
		codec_input->decoder_or_encoder = CODEC_ENCODER;
	} else if (is_decode_session(inst)) {
		codec_input->decoder_or_encoder = CODEC_DECODER;
	} else {
		d_vpr_e("%s: invalid domain %d\n", __func__, inst->domain);
		return -EINVAL;
	}

	codec_input->chipset_gen = MSM_SM8550;

	if (inst->codec == MSM_VIDC_H264) {
		codec_input->codec    = CODEC_H264;
		codec_input->lcu_size = 16;
		if (inst->capabilities[ENTROPY_MODE].value ==
				V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC)
			codec_input->entropy_coding_mode = CODEC_ENTROPY_CODING_CABAC;
		else
			codec_input->entropy_coding_mode = CODEC_ENTROPY_CODING_CAVLC;
	} else if (inst->codec == MSM_VIDC_HEVC) {
		codec_input->codec    = CODEC_HEVC;
		codec_input->lcu_size = 32;
	} else if (inst->codec == MSM_VIDC_VP9) {
		codec_input->codec    = CODEC_VP9;
		codec_input->lcu_size = 16;
	} else {
		d_vpr_e("%s: invalid codec %d\n", __func__, inst->codec);
		return -EINVAL;
	}

	codec_input->pipe_num = inst->capabilities[PIPE].value;
	codec_input->frame_rate = inst->max_rate;

	port = is_decode_session(inst) ? INPUT_PORT : OUTPUT_PORT;
	codec_input->frame_width = inst->fmts[port].fmt.pix_mp.width;
	codec_input->frame_height = inst->fmts[port].fmt.pix_mp.height;

	if (inst->capabilities[STAGE].value == MSM_VIDC_STAGE_1) {
		codec_input->vsp_vpp_mode = CODEC_VSPVPP_MODE_1S;
	} else if (inst->capabilities[STAGE].value == MSM_VIDC_STAGE_2) {
		codec_input->vsp_vpp_mode = CODEC_VSPVPP_MODE_2S;
	} else {
		d_vpr_e("%s: invalid stage %d\n", __func__,
			inst->capabilities[STAGE].value);
		return -EINVAL;
	}

	if (inst->capabilities[BIT_DEPTH].value == BIT_DEPTH_8)
		codec_input->bitdepth = CODEC_BITDEPTH_8;
	else
		codec_input->bitdepth = CODEC_BITDEPTH_10;

	/*
	 * Used for calculating Encoder GOP Complexity
	 * hierachical_layer= 0..7 used as Array Index
	 * inst->capabilities[B_FRAME].value=[ 0 1 2]
	 * TODO how to map?
	 */

	/* set as IPP */
	codec_input->hierachical_layer = 0;

	if (is_decode_session(inst))
		color_fmt =
			v4l2_colorformat_to_driver(inst,
						   inst->fmts[OUTPUT_PORT].fmt.pix_mp.pixelformat,
						   __func__);
	else
		color_fmt =
			v4l2_colorformat_to_driver(inst,
						   inst->fmts[INPUT_PORT].fmt.pix_mp.pixelformat,
						   __func__);

	codec_input->linear_opb = is_linear_colorformat(color_fmt);
	codec_input->bitrate_mbps =
		(codec_input->frame_rate * data_size * 8) / 1000000;

	/* set as sanity mode */
	codec_input->regression_mode = 1;

	return 0;
}

static int msm_vidc_init_codec_input_bus(struct msm_vidc_inst *inst, struct vidc_bus_vote_data *d,
					 struct api_calculation_input *codec_input)
{
	u32 complexity_factor_int = 0, complexity_factor_frac = 0;
	bool opb_compression_enabled = false;

	if (!d)
		return -EINVAL;

	if (d->domain == MSM_VIDC_ENCODER) {
		codec_input->decoder_or_encoder = CODEC_ENCODER;
	} else if (d->domain == MSM_VIDC_DECODER) {
		codec_input->decoder_or_encoder = CODEC_DECODER;
	} else {
		d_vpr_e("%s: invalid domain %d\n", __func__, d->domain);
		return -EINVAL;
	}

	codec_input->chipset_gen = MSM_SM8550;

	if (d->codec == MSM_VIDC_H264) {
		codec_input->codec = CODEC_H264;
	} else if (d->codec == MSM_VIDC_HEVC) {
		codec_input->codec = CODEC_HEVC;
	} else if (d->codec == MSM_VIDC_VP9) {
		codec_input->codec = CODEC_VP9;
	} else {
		d_vpr_e("%s: invalid codec %d\n", __func__, d->codec);
		return -EINVAL;
	}

	codec_input->lcu_size = d->lcu_size;
	codec_input->pipe_num = d->num_vpp_pipes;
	codec_input->frame_rate = d->fps;
	codec_input->frame_width = d->input_width;
	codec_input->frame_height = d->input_height;

	if (d->work_mode == MSM_VIDC_STAGE_1) {
		codec_input->vsp_vpp_mode = CODEC_VSPVPP_MODE_1S;
	} else if (d->work_mode == MSM_VIDC_STAGE_2) {
		codec_input->vsp_vpp_mode = CODEC_VSPVPP_MODE_2S;
	} else {
		d_vpr_e("%s: invalid stage %d\n", __func__, d->work_mode);
		return -EINVAL;
	}

	if (inst->capabilities[ENTROPY_MODE].value ==
			V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC) {
		codec_input->entropy_coding_mode = CODEC_ENTROPY_CODING_CABAC;
	} else if (inst->capabilities[ENTROPY_MODE].value ==
			V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC) {
		codec_input->entropy_coding_mode = CODEC_ENTROPY_CODING_CAVLC;
	} else {
		d_vpr_e("%s: invalid entropy %d\n", __func__,
			inst->capabilities[ENTROPY_MODE].value);
		return -EINVAL;
	}

	/*
	 * Used for calculating Encoder GOP Complexity
	 * hierachical_layer= 0..7 used as Array Index
	 * TODO how to map?
	 */
	codec_input->hierachical_layer = 0; /* set as IPP */

	/*
	 * If the calculated motion_vector_complexity is > 2 then set the
	 * complexity_setting and refframe_complexity to be pwc(performance worst case)
	 * values. If the motion_vector_complexity is < 2 then set the complexity_setting
	 * and refframe_complexity to be average case values.
	 */

	complexity_factor_int = Q16_INT(d->complexity_factor);
	complexity_factor_frac = Q16_FRAC(d->complexity_factor);

	if (complexity_factor_int < COMPLEXITY_THRESHOLD ||
	    (complexity_factor_int == COMPLEXITY_THRESHOLD &&
	     complexity_factor_frac == 0)) {
		/* set as average case values */
		codec_input->complexity_setting = COMPLEXITY_SETTING_AVG;
		codec_input->refframe_complexity = REFFRAME_COMPLEXITY_AVG;
	} else {
		/* set as pwc */
		codec_input->complexity_setting = COMPLEXITY_SETTING_PWC;
		codec_input->refframe_complexity = REFFRAME_COMPLEXITY_PWC;
	}

	codec_input->status_llc_onoff = d->use_sys_cache;

	if (__bpp(d->color_formats[0]) == 8)
		codec_input->bitdepth = CODEC_BITDEPTH_8;
	else
		codec_input->bitdepth = CODEC_BITDEPTH_10;

	if (d->num_formats == 1) {
		codec_input->split_opb = 0;
		codec_input->linear_opb = !__ubwc(d->color_formats[0]);
	} else if (d->num_formats == 2) {
		codec_input->split_opb = 1;
		codec_input->linear_opb = !__ubwc(d->color_formats[1]);
	} else {
		d_vpr_e("%s: invalid num_formats %d\n",
			__func__, d->num_formats);
		return -EINVAL;
	}

	codec_input->linear_ipb = 0;   /* set as ubwc ipb */

	/* TODO Confirm if we always LOSSLESS mode ie lossy_ipb = 0*/
	codec_input->lossy_ipb = 0;   /* set as lossless ipb */

	/* TODO Confirm if no multiref */
	codec_input->encoder_multiref = 0;	/* set as no multiref */
	codec_input->bitrate_mbps = (d->bitrate / 1000000); /* bps 10;	set as 10mbps */

	opb_compression_enabled = d->num_formats >= 2 && __ubwc(d->color_formats[1]);

	/* ANDROID CR is in Q16 format, StaticModel CR in x100 format */
	codec_input->cr_dpb = ((Q16_INT(d->compression_ratio) * 100) +
			       Q16_FRAC(d->compression_ratio));

	codec_input->cr_opb = opb_compression_enabled ?
		codec_input->cr_dpb : 65536;

	codec_input->cr_ipb = ((Q16_INT(d->input_cr) * 100) + Q16_FRAC(d->input_cr));
	codec_input->cr_rpb = codec_input->cr_dpb;  /* cr_rpb only for encoder */

	/* disable by default, only enable for aurora depth map session */
	codec_input->lumaonly_decode = 0;

	/* set as custom regression mode, as are using cr,cf values from FW */
	codec_input->regression_mode = REGRESSION_MODE_CUSTOM;

	/* Dump all the variables for easier debugging */
	if (msm_vidc_debug & VIDC_BUS) {
		struct dump dump[] = {
		{"complexity_factor_int", "%d", complexity_factor_int},
		{"complexity_factor_frac", "%d", complexity_factor_frac},
		{"refframe_complexity", "%d", codec_input->refframe_complexity},
		{"complexity_setting", "%d", codec_input->complexity_setting},
		{"cr_dpb", "%d", codec_input->cr_dpb},
		{"cr_opb", "%d", codec_input->cr_opb},
		{"cr_ipb", "%d", codec_input->cr_ipb},
		{"cr_rpb", "%d", codec_input->cr_rpb},
		{"lcu size", "%d", codec_input->lcu_size},
		{"pipe number", "%d", codec_input->pipe_num},
		{"frame_rate", "%d", codec_input->frame_rate},
		{"frame_width", "%d", codec_input->frame_width},
		{"frame_height", "%d", codec_input->frame_height},
		{"work_mode", "%d", d->work_mode},
		{"encoder_or_decode", "%d", inst->domain},
		{"chipset_gen", "%d", codec_input->chipset_gen},
		{"codec_input", "%d", codec_input->codec},
		{"entropy_coding_mode", "%d", codec_input->entropy_coding_mode},
		{"hierachical_layer", "%d", codec_input->hierachical_layer},
		{"status_llc_onoff", "%d", codec_input->status_llc_onoff},
		{"bit_depth", "%d", codec_input->bitdepth},
		{"split_opb", "%d", codec_input->split_opb},
		{"linear_opb", "%d", codec_input->linear_opb},
		{"linear_ipb", "%d", codec_input->linear_ipb},
		{"lossy_ipb", "%d", codec_input->lossy_ipb},
		{"encoder_multiref", "%d", codec_input->encoder_multiref},
		{"bitrate_mbps", "%d", codec_input->bitrate_mbps},
		{"lumaonly_decode", "%d", codec_input->lumaonly_decode},
		{"regression_mode", "%d", codec_input->regression_mode},
		};
		__dump(dump, ARRAY_SIZE(dump));
	}

	return 0;
}

u64 msm_vidc_calc_freq_iris3(struct msm_vidc_inst *inst, u32 data_size)
{
	u64 freq = 0;
	struct msm_vidc_core *core;
	int ret = 0;
	struct api_calculation_input codec_input;
	struct api_calculation_freq_output codec_output;
	u32 fps, mbpf;

	core = inst->core;

	mbpf = msm_vidc_get_mbs_per_frame(inst);
	fps = inst->max_rate;

	memset(&codec_input, 0, sizeof(struct api_calculation_input));
	memset(&codec_output, 0, sizeof(struct api_calculation_freq_output));
	ret = msm_vidc_init_codec_input_freq(inst, data_size, &codec_input);
	if (ret)
		return freq;
	ret = msm_vidc_calculate_frequency(codec_input, &codec_output);
	if (ret)
		return freq;
	freq = codec_output.hw_min_freq * 1000000; /* Convert to Hz */

	i_vpr_p(inst, "%s: filled len %d, required freq %llu, fps %u, mbpf %u\n",
		__func__, data_size, freq, fps, mbpf);

	if (inst->iframe && is_hevc_10bit_decode_session(inst)) {
		/*
		 * for HEVC 10bit and iframe case only allow TURBO and
		 * limit to NOM for all other cases
		 */
	} else {
		/* limit to NOM, index 0 is TURBO, index 1 is NOM clock rate */
		if (core->resource->freq_set.count >= 2 &&
		    freq > core->resource->freq_set.freq_tbl[1].freq)
			freq = core->resource->freq_set.freq_tbl[1].freq;
	}

	return freq;
}

int msm_vidc_calc_bw_iris3(struct msm_vidc_inst *inst,
			   struct vidc_bus_vote_data *vidc_data)
{
	int ret = 0;
	struct api_calculation_input codec_input;
	struct api_calculation_bw_output codec_output;

	if (!vidc_data)
		return ret;

	memset(&codec_input, 0, sizeof(struct api_calculation_input));
	memset(&codec_output, 0, sizeof(struct api_calculation_bw_output));

	ret = msm_vidc_init_codec_input_bus(inst, vidc_data, &codec_input);
	if (ret)
		return ret;
	ret = msm_vidc_calculate_bandwidth(codec_input, &codec_output);
	if (ret)
		return ret;

	vidc_data->calc_bw_ddr = kbps(codec_output.ddr_bw_rd + codec_output.ddr_bw_wr);
	vidc_data->calc_bw_llcc = kbps(codec_output.noc_bw_rd + codec_output.noc_bw_wr);

	i_vpr_l(inst, "%s: calc_bw_ddr %llu calc_bw_llcc %llu",
		__func__, vidc_data->calc_bw_ddr, vidc_data->calc_bw_llcc);

	return ret;
}
