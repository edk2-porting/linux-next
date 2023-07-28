// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "msm_media_info.h"
#include "msm_vidc_buffer.h"
#include "msm_vidc_core.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_inst.h"
#include "msm_vidc_internal.h"

/* Generic function for all targets. Not being used for iris2 */
u32 msm_vidc_input_min_count(struct msm_vidc_inst *inst)
{
	u32 input_min_count = 0;
	u32 hb_enh_layer = 0;

	if (is_decode_session(inst)) {
		input_min_count = MIN_DEC_INPUT_BUFFERS;
	} else if (is_encode_session(inst)) {
		input_min_count = MIN_ENC_INPUT_BUFFERS;
		if (is_hierb_type_requested(inst)) {
			hb_enh_layer =
				inst->capabilities[ENH_LAYER_COUNT].value;
			if (inst->codec == MSM_VIDC_H264 &&
			    !inst->capabilities[LAYER_ENABLE].value) {
				hb_enh_layer = 0;
			}
			if (hb_enh_layer)
				input_min_count = (1 << hb_enh_layer) + 2;
		}
	} else {
		i_vpr_e(inst, "%s: invalid domain %d\n",
			__func__, inst->domain);
		return 0;
	}

	return input_min_count;
}

u32 msm_vidc_output_min_count(struct msm_vidc_inst *inst)
{
	u32 output_min_count;

	if (!is_decode_session(inst) && !is_encode_session(inst))
		return 0;

	if (is_encode_session(inst))
		return MIN_ENC_OUTPUT_BUFFERS;

	/* decoder handling below */
	/* fw_min_count > 0 indicates reconfig event has already arrived */
	if (inst->fw_min_count) {
		if (is_split_mode_enabled(inst) &&
		    inst->codec == MSM_VIDC_VP9) {
			/*
			 * return opb min buffer count as min(4, fw_min_count)
			 * fw min count is used for dpb min count
			 */
			return min_t(u32, 4, inst->fw_min_count);
		} else {
			return inst->fw_min_count;
		}
	}

	/* initial handling before reconfig event arrived */
	switch (inst->codec) {
	case MSM_VIDC_H264:
	case MSM_VIDC_HEVC:
		output_min_count = 4;
		break;
	case MSM_VIDC_VP9:
		output_min_count = 9;
		break;
	default:
		output_min_count = 4;
		break;
	}

	return output_min_count;
}

u32 msm_vidc_input_extra_count(struct msm_vidc_inst *inst)
{
	u32 count = 0;
	struct msm_vidc_core *core;

	core = inst->core;

	if (is_decode_session(inst)) {
		/*
		 * if decode batching enabled, ensure minimum batch size
		 * count of input buffers present on input port
		 */
		if (core->capabilities[DECODE_BATCH].value &&
		    inst->decode_batch.enable) {
			if (inst->buffers.input.min_count < inst->decode_batch.size) {
				count = inst->decode_batch.size -
					inst->buffers.input.min_count;
			}
		}
	} else if (is_encode_session(inst)) {
		/* add dcvs buffers, if platform supports dcvs */
		if (core->capabilities[DCVS].value)
			count = DCVS_ENC_EXTRA_INPUT_BUFFERS;
	}

	return count;
}

u32 msm_vidc_output_extra_count(struct msm_vidc_inst *inst)
{
	u32 count = 0;
	struct msm_vidc_core *core;

	core = inst->core;

	if (is_decode_session(inst)) {
		/* add dcvs buffers, if platform supports dcvs */
		if (core->capabilities[DCVS].value)
			count = DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
		/*
		 * if decode batching enabled, ensure minimum batch size
		 * count of extra output buffers added on output port
		 */
		if (core->capabilities[DECODE_BATCH].value &&
		    inst->decode_batch.enable &&
			count < inst->decode_batch.size)
			count = inst->decode_batch.size;
	}

	return count;
}

u32 msm_vidc_internal_buffer_count(struct msm_vidc_inst *inst,
				   enum msm_vidc_buffer_type buffer_type)
{
	u32 count = 0;

	if (is_encode_session(inst))
		return 1;

	if (is_decode_session(inst)) {
		if (buffer_type == MSM_VIDC_BUF_BIN ||
		    buffer_type == MSM_VIDC_BUF_LINE ||
		    buffer_type == MSM_VIDC_BUF_PERSIST) {
			count = 1;
		} else if (buffer_type == MSM_VIDC_BUF_COMV ||
			buffer_type == MSM_VIDC_BUF_NON_COMV) {
			if (inst->codec == MSM_VIDC_H264 ||
			    inst->codec == MSM_VIDC_HEVC)
				count = 1;
			else
				count = 0;
		} else {
			i_vpr_e(inst, "%s: unsupported buffer type %s\n",
				__func__, buf_name(buffer_type));
			count = 0;
		}
	}

	return count;
}

u32 msm_vidc_decoder_input_size(struct msm_vidc_inst *inst)
{
	u32 frame_size, num_mbs;
	u32 div_factor = 1;
	u32 base_res_mbs = NUM_MBS_4k;
	struct v4l2_format *f;
	u32 bitstream_size_overwrite = 0;
	enum msm_vidc_codec_type codec;

	bitstream_size_overwrite =
		inst->capabilities[BITSTREAM_SIZE_OVERWRITE].value;
	if (bitstream_size_overwrite) {
		frame_size = bitstream_size_overwrite;
		i_vpr_h(inst, "client configured bitstream buffer size %d\n",
			frame_size);
		return frame_size;
	}

	/*
	 * Decoder input size calculation:
	 * For 8k resolution, buffer size is calculated as 8k mbs / 4 and
	 * for 8k cases we expect width/height to be set always.
	 * In all other cases, buffer size is calculated as
	 * 4k mbs for VP8/VP9 and 4k / 2 for remaining codecs.
	 */
	f = &inst->fmts[INPUT_PORT];
	codec = v4l2_codec_to_driver(inst, f->fmt.pix_mp.pixelformat, __func__);
	num_mbs = msm_vidc_get_mbs_per_frame(inst);
	if (num_mbs > NUM_MBS_4k) {
		div_factor = 4;
		base_res_mbs = inst->capabilities[MBPF].value;
	} else {
		base_res_mbs = NUM_MBS_4k;
		if (codec == MSM_VIDC_VP9)
			div_factor = 1;
		else
			div_factor = 2;
	}

	frame_size = base_res_mbs * MB_SIZE_IN_PIXEL * 3 / 2 / div_factor;

	 /* multiply by 10/8 (1.25) to get size for 10 bit case */
	if (codec == MSM_VIDC_VP9 || codec == MSM_VIDC_HEVC)
		frame_size = frame_size + (frame_size >> 2);

	i_vpr_h(inst, "set input buffer size to %d\n", frame_size);

	return ALIGN(frame_size, SZ_4K);
}

u32 msm_vidc_decoder_output_size(struct msm_vidc_inst *inst)
{
	u32 size;
	struct v4l2_format *f;
	enum msm_vidc_colorformat_type colorformat;

	f = &inst->fmts[OUTPUT_PORT];
	colorformat = v4l2_colorformat_to_driver(inst, f->fmt.pix_mp.pixelformat,
						 __func__);
	size = video_buffer_size(colorformat, f->fmt.pix_mp.width,
				 f->fmt.pix_mp.height, true);
	return size;
}

u32 msm_vidc_encoder_input_size(struct msm_vidc_inst *inst)
{
	u32 size;
	struct v4l2_format *f;
	u32 width, height;
	enum msm_vidc_colorformat_type colorformat;

	f = &inst->fmts[INPUT_PORT];
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;
	colorformat = v4l2_colorformat_to_driver(inst, f->fmt.pix_mp.pixelformat,
						 __func__);
	size = video_buffer_size(colorformat, width, height, true);
	return size;
}

u32 msm_vidc_encoder_output_size(struct msm_vidc_inst *inst)
{
	u32 frame_size;
	u32 mbs_per_frame;
	u32 width, height;
	struct v4l2_format *f;
	enum msm_vidc_codec_type codec;

	f = &inst->fmts[OUTPUT_PORT];
	codec = v4l2_codec_to_driver(inst, f->fmt.pix_mp.pixelformat, __func__);
	/*
	 * Encoder output size calculation: 32 Align width/height
	 * For resolution <= 480x360p : YUVsize * 2
	 * For resolution > 360p & <= 4K : YUVsize / 2
	 * For resolution > 4k : YUVsize / 4
	 * Initially frame_size = YUVsize * 2;
	 */

	width = ALIGN(f->fmt.pix_mp.width, BUFFER_ALIGNMENT_SIZE(32));
	height = ALIGN(f->fmt.pix_mp.height, BUFFER_ALIGNMENT_SIZE(32));
	mbs_per_frame = NUM_MBS_PER_FRAME(width, height);
	frame_size = (width * height * 3);

	/* Image session: 2 x yuv size */
	if (inst->capabilities[BITRATE_MODE].value == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		goto skip_calc;

	if (mbs_per_frame <= NUM_MBS_360P)
		(void)frame_size; /* Default frame_size = YUVsize * 2 */
	else if (mbs_per_frame <= NUM_MBS_4k)
		frame_size = frame_size >> 2;
	else
		frame_size = frame_size >> 3;

skip_calc:
	/* multiply by 10/8 (1.25) to get size for 10 bit case */
	if (codec == MSM_VIDC_HEVC)
		frame_size = frame_size + (frame_size >> 2);

	frame_size = ALIGN(frame_size, SZ_4K);

	return frame_size;
}
