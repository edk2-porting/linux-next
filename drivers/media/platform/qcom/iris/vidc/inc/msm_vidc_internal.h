/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_INTERNAL_H_
#define _MSM_VIDC_INTERNAL_H_

#include <linux/bits.h>
#include <linux/spinlock.h>
#include <linux/sync_file.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

struct msm_vidc_core;
struct msm_vidc_inst;

static const char video_banner[] = "Video-Banner: (" __stringify(VIDEO_COMPILE_BY) "@"
	__stringify(VIDEO_COMPILE_HOST) ") (" __stringify(VIDEO_COMPILE_TIME) ")";

#define MAX_NAME_LENGTH   128
#define VENUS_VERSION_LENGTH 128
#define MAX_MATRIX_COEFFS 9
#define MAX_BIAS_COEFFS   3
#define MAX_LIMIT_COEFFS  6
#define MAX_DEBUGFS_NAME  50
#define DEFAULT_HEIGHT    240
#define DEFAULT_WIDTH     320
#define DEFAULT_FPS       30
#define MAXIMUM_VP9_FPS   60
#define RT_DEC_DOWN_PRORITY_OFFSET 1
#define MAX_SUPPORTED_INSTANCES  16
#define DEFAULT_BSE_VPP_DELAY    2
#define MAX_CAP_PARENTS          20
#define MAX_CAP_CHILDREN         20
#define DEFAULT_MAX_HOST_BUF_COUNT  64
#define DEFAULT_MAX_HOST_BURST_BUF_COUNT 256
#define BIT_DEPTH_8 (8 << 16 | 8)
#define BIT_DEPTH_10 (10 << 16 | 10)
#define CODED_FRAMES_PROGRESSIVE 0x0
#define CODED_FRAMES_INTERLACE 0x1
#define MAX_VP9D_INST_COUNT     6
/* TODO: move below macros to waipio.c */
#define MAX_ENH_LAYER_HB        3
#define MAX_HEVC_VBR_ENH_LAYER_SLIDING_WINDOW         5
#define MAX_HEVC_NON_VBR_ENH_LAYER_SLIDING_WINDOW     3
#define MAX_AVC_ENH_LAYER_SLIDING_WINDOW      3
#define MAX_AVC_ENH_LAYER_HYBRID_HP           5
#define INVALID_DEFAULT_MARK_OR_USE_LTR      -1
#define MAX_SLICES_PER_FRAME                 10
#define MAX_SLICES_FRAME_RATE                60
#define MAX_MB_SLICE_WIDTH                 4096
#define MAX_MB_SLICE_HEIGHT                2160
#define MAX_BYTES_SLICE_WIDTH              1920
#define MAX_BYTES_SLICE_HEIGHT             1088
#define MIN_HEVC_SLICE_WIDTH                384
#define MIN_AVC_SLICE_WIDTH                 192
#define MIN_SLICE_HEIGHT                    128
#define MAX_BITRATE_BOOST                    25
#define MAX_SUPPORTED_MIN_QUALITY            70
#define MIN_CHROMA_QP_OFFSET                -12
#define MAX_CHROMA_QP_OFFSET                  0
#define MIN_QP_10BIT                        -11
#define MIN_QP_8BIT                           1
#define INVALID_FD                           -1
#define MAX_ENCODING_REFERNCE_FRAMES          7
#define MAX_LTR_FRAME_COUNT_5                 5
#define MAX_LTR_FRAME_COUNT_2                 2
#define MAX_ENC_RING_BUF_COUNT                5 /* to be tuned */
#define MAX_TRANSCODING_STATS_FRAME_RATE     60
#define MAX_TRANSCODING_STATS_WIDTH        4096
#define MAX_TRANSCODING_STATS_HEIGHT       2304

#define DCVS_WINDOW 16
#define ENC_FPS_WINDOW 3
#define DEC_FPS_WINDOW 10
#define INPUT_TIMER_LIST_SIZE 30

#define INPUT_MPLANE V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
#define OUTPUT_MPLANE V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE

#define VIDC_IFACEQ_MAX_PKT_SIZE                1024
#define VIDC_IFACEQ_MED_PKT_SIZE                768
#define VIDC_IFACEQ_MIN_PKT_SIZE                8
#define VIDC_IFACEQ_VAR_SMALL_PKT_SIZE          100
#define VIDC_IFACEQ_VAR_LARGE_PKT_SIZE          512
#define VIDC_IFACEQ_VAR_HUGE_PKT_SIZE          (1024 * 4)

#define NUM_MBS_PER_SEC(__height, __width, __fps) \
	(NUM_MBS_PER_FRAME(__height, __width) * (__fps))

#define NUM_MBS_PER_FRAME(__height, __width) \
	((ALIGN(__height, 16) / 16) * (ALIGN(__width, 16) / 16))

#ifdef V4L2_CTRL_CLASS_CODEC
#define IS_PRIV_CTRL(idx) ( \
	(V4L2_CTRL_ID2WHICH(idx) == V4L2_CTRL_CLASS_CODEC) && \
	V4L2_CTRL_DRIVER_PRIV(idx))
#else
#define IS_PRIV_CTRL(idx) ( \
	(V4L2_CTRL_ID2WHICH(idx) == V4L2_CTRL_CLASS_MPEG) && \
	V4L2_CTRL_DRIVER_PRIV(idx))
#endif

#define BUFFER_ALIGNMENT_SIZE(x) x
#define NUM_MBS_360P (((480 + 15) >> 4) * ((360 + 15) >> 4))
#define NUM_MBS_720P (((1280 + 15) >> 4) * ((720 + 15) >> 4))
#define NUM_MBS_4k (((4096 + 15) >> 4) * ((2304 + 15) >> 4))
#define MB_SIZE_IN_PIXEL (16 * 16)

#define DB_H264_DISABLE_SLICE_BOUNDARY \
		V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY

#define DB_HEVC_DISABLE_SLICE_BOUNDARY \
		V4L2_MPEG_VIDEO_HEVC_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY

/*
 * Convert Q16 number into Integer and Fractional part up to 2 places.
 * Ex : 105752 / 65536 = 1.61; 1.61 in Q16 = 105752;
 * Integer part =  105752 / 65536 = 1;
 * Reminder = 105752 * 0xFFFF = 40216; Last 16 bits.
 * Fractional part = 40216 * 100 / 65536 = 61;
 * Now convert to FP(1, 61, 100).
 */
#define Q16_INT(q) ((q) >> 16)
#define Q16_FRAC(q) ((((q) & 0xFFFF) * 100) >> 16)

/* define timeout values */
#define HW_RESPONSE_TIMEOUT_VALUE     (1000)
#define SW_PC_DELAY_VALUE             (HW_RESPONSE_TIMEOUT_VALUE + 500)
#define FW_UNLOAD_DELAY_VALUE         (SW_PC_DELAY_VALUE + 1500)

#define MAX_DPB_COUNT 32
 /*
  * max dpb count in firmware = 16
  * each dpb: 4 words - <base_address, addr_offset, data_offset>
  * dpb list array size = 16 * 4
  * dpb payload size = 16 * 4 * 4
  */
#define MAX_DPB_LIST_ARRAY_SIZE (16 * 4)
#define MAX_DPB_LIST_PAYLOAD_SIZE (16 * 4 * 4)

enum msm_vidc_domain_type {
	MSM_VIDC_ENCODER           = BIT(0),
	MSM_VIDC_DECODER           = BIT(1),
};

enum msm_vidc_codec_type {
	MSM_VIDC_H264              = BIT(0),
	MSM_VIDC_HEVC              = BIT(1),
	MSM_VIDC_VP9               = BIT(2),
};

enum msm_vidc_colorformat_type {
	MSM_VIDC_FMT_NONE          = 0,
	MSM_VIDC_FMT_NV12C         = BIT(0),
	MSM_VIDC_FMT_NV12          = BIT(1),
	MSM_VIDC_FMT_NV21          = BIT(2),
	MSM_VIDC_FMT_TP10C         = BIT(3),
	MSM_VIDC_FMT_P010          = BIT(4),
	MSM_VIDC_FMT_RGBA8888C     = BIT(5),
	MSM_VIDC_FMT_RGBA8888      = BIT(6),
};

enum msm_vidc_buffer_type {
	MSM_VIDC_BUF_NONE,
	MSM_VIDC_BUF_INPUT,
	MSM_VIDC_BUF_OUTPUT,
	MSM_VIDC_BUF_READ_ONLY,
	MSM_VIDC_BUF_INTERFACE_QUEUE,
	MSM_VIDC_BUF_BIN,
	MSM_VIDC_BUF_ARP,
	MSM_VIDC_BUF_COMV,
	MSM_VIDC_BUF_NON_COMV,
	MSM_VIDC_BUF_LINE,
	MSM_VIDC_BUF_DPB,
	MSM_VIDC_BUF_PERSIST,
	MSM_VIDC_BUF_VPSS,
};

/* always match with v4l2 flags V4L2_BUF_FLAG_* */
enum msm_vidc_buffer_flags {
	MSM_VIDC_BUF_FLAG_KEYFRAME         = 0x00000008,
	MSM_VIDC_BUF_FLAG_PFRAME           = 0x00000010,
	MSM_VIDC_BUF_FLAG_BFRAME           = 0x00000020,
	MSM_VIDC_BUF_FLAG_ERROR            = 0x00000040,
	MSM_VIDC_BUF_FLAG_LAST             = 0x00100000,
};

enum msm_vidc_buffer_attributes {
	MSM_VIDC_ATTR_DEFERRED                  = BIT(0),
	MSM_VIDC_ATTR_READ_ONLY                 = BIT(1),
	MSM_VIDC_ATTR_PENDING_RELEASE           = BIT(2),
	MSM_VIDC_ATTR_QUEUED                    = BIT(3),
	MSM_VIDC_ATTR_DEQUEUED                  = BIT(4),
	MSM_VIDC_ATTR_BUFFER_DONE               = BIT(5),
	MSM_VIDC_ATTR_RELEASE_ELIGIBLE          = BIT(6),
};

enum msm_vidc_buffer_region {
	MSM_VIDC_REGION_NONE = 0,
	MSM_VIDC_NON_SECURE,
	MSM_VIDC_NON_SECURE_PIXEL,
	MSM_VIDC_SECURE_PIXEL,
	MSM_VIDC_SECURE_NONPIXEL,
	MSM_VIDC_SECURE_BITSTREAM,
	MSM_VIDC_REGION_MAX,
};

enum msm_vidc_port_type {
	INPUT_PORT = 0,
	OUTPUT_PORT,
	PORT_NONE,
	MAX_PORT,
};

enum msm_vidc_stage_type {
	MSM_VIDC_STAGE_NONE = 0,
	MSM_VIDC_STAGE_1 = 1,
	MSM_VIDC_STAGE_2 = 2,
};

enum msm_vidc_pipe_type {
	MSM_VIDC_PIPE_NONE = 0,
	MSM_VIDC_PIPE_1 = 1,
	MSM_VIDC_PIPE_2 = 2,
	MSM_VIDC_PIPE_4 = 4,
};

enum msm_vidc_quality_mode {
	MSM_VIDC_MAX_QUALITY_MODE = 0x1,
	MSM_VIDC_POWER_SAVE_MODE = 0x2,
};

enum msm_vidc_color_primaries {
	MSM_VIDC_PRIMARIES_RESERVED                         = 0,
	MSM_VIDC_PRIMARIES_BT709                            = 1,
	MSM_VIDC_PRIMARIES_UNSPECIFIED                      = 2,
	MSM_VIDC_PRIMARIES_BT470_SYSTEM_M                   = 4,
	MSM_VIDC_PRIMARIES_BT470_SYSTEM_BG                  = 5,
	MSM_VIDC_PRIMARIES_BT601_525                        = 6,
	MSM_VIDC_PRIMARIES_SMPTE_ST240M                     = 7,
	MSM_VIDC_PRIMARIES_GENERIC_FILM                     = 8,
	MSM_VIDC_PRIMARIES_BT2020                           = 9,
	MSM_VIDC_PRIMARIES_SMPTE_ST428_1                    = 10,
	MSM_VIDC_PRIMARIES_SMPTE_RP431_2                    = 11,
	MSM_VIDC_PRIMARIES_SMPTE_EG431_1                    = 12,
	MSM_VIDC_PRIMARIES_SMPTE_EBU_TECH                   = 22,
};

enum msm_vidc_transfer_characteristics {
	MSM_VIDC_TRANSFER_RESERVED                          = 0,
	MSM_VIDC_TRANSFER_BT709                             = 1,
	MSM_VIDC_TRANSFER_UNSPECIFIED                       = 2,
	MSM_VIDC_TRANSFER_BT470_SYSTEM_M                    = 4,
	MSM_VIDC_TRANSFER_BT470_SYSTEM_BG                   = 5,
	MSM_VIDC_TRANSFER_BT601_525_OR_625                  = 6,
	MSM_VIDC_TRANSFER_SMPTE_ST240M                      = 7,
	MSM_VIDC_TRANSFER_LINEAR                            = 8,
	MSM_VIDC_TRANSFER_LOG_100_1                         = 9,
	MSM_VIDC_TRANSFER_LOG_SQRT                          = 10,
	MSM_VIDC_TRANSFER_XVYCC                             = 11,
	MSM_VIDC_TRANSFER_BT1361_0                          = 12,
	MSM_VIDC_TRANSFER_SRGB_SYCC                         = 13,
	MSM_VIDC_TRANSFER_BT2020_14                         = 14,
	MSM_VIDC_TRANSFER_BT2020_15                         = 15,
	MSM_VIDC_TRANSFER_SMPTE_ST2084_PQ                   = 16,
	MSM_VIDC_TRANSFER_SMPTE_ST428_1                     = 17,
	MSM_VIDC_TRANSFER_BT2100_2_HLG                      = 18,
};

enum msm_vidc_matrix_coefficients {
	MSM_VIDC_MATRIX_COEFF_SRGB_SMPTE_ST428_1             = 0,
	MSM_VIDC_MATRIX_COEFF_BT709                          = 1,
	MSM_VIDC_MATRIX_COEFF_UNSPECIFIED                    = 2,
	MSM_VIDC_MATRIX_COEFF_RESERVED                       = 3,
	MSM_VIDC_MATRIX_COEFF_FCC_TITLE_47                   = 4,
	MSM_VIDC_MATRIX_COEFF_BT470_SYS_BG_OR_BT601_625      = 5,
	MSM_VIDC_MATRIX_COEFF_BT601_525_BT1358_525_OR_625    = 6,
	MSM_VIDC_MATRIX_COEFF_SMPTE_ST240                    = 7,
	MSM_VIDC_MATRIX_COEFF_YCGCO                          = 8,
	MSM_VIDC_MATRIX_COEFF_BT2020_NON_CONSTANT            = 9,
	MSM_VIDC_MATRIX_COEFF_BT2020_CONSTANT                = 10,
	MSM_VIDC_MATRIX_COEFF_SMPTE_ST2085                   = 11,
	MSM_VIDC_MATRIX_COEFF_SMPTE_CHROM_DERV_NON_CONSTANT  = 12,
	MSM_VIDC_MATRIX_COEFF_SMPTE_CHROM_DERV_CONSTANT      = 13,
	MSM_VIDC_MATRIX_COEFF_BT2100                         = 14,
};

enum msm_vidc_preprocess_type {
	MSM_VIDC_PREPROCESS_NONE = BIT(0),
	MSM_VIDC_PREPROCESS_TYPE0 = BIT(1),
};

enum msm_vidc_core_capability_type {
	CORE_CAP_NONE = 0,
	ENC_CODECS,
	DEC_CODECS,
	MAX_SESSION_COUNT,
	MAX_NUM_720P_SESSIONS,
	MAX_NUM_1080P_SESSIONS,
	MAX_NUM_4K_SESSIONS,
	MAX_NUM_8K_SESSIONS,
	MAX_LOAD,
	MAX_RT_MBPF,
	MAX_MBPF,
	MAX_MBPS,
	MAX_MBPF_HQ,
	MAX_MBPS_HQ,
	MAX_MBPF_B_FRAME,
	MAX_MBPS_B_FRAME,
	MAX_MBPS_ALL_INTRA,
	MAX_ENH_LAYER_COUNT,
	NUM_VPP_PIPE,
	SW_PC,
	SW_PC_DELAY,
	FW_UNLOAD,
	FW_UNLOAD_DELAY,
	HW_RESPONSE_TIMEOUT,
	PREFIX_BUF_COUNT_PIX,
	PREFIX_BUF_SIZE_PIX,
	PREFIX_BUF_COUNT_NON_PIX,
	PREFIX_BUF_SIZE_NON_PIX,
	PAGEFAULT_NON_FATAL,
	PAGETABLE_CACHING,
	DCVS,
	DECODE_BATCH,
	DECODE_BATCH_TIMEOUT,
	STATS_TIMEOUT_MS,
	AV_SYNC_WINDOW_SIZE,
	CLK_FREQ_THRESHOLD,
	NON_FATAL_FAULTS,
	DEVICE_CAPS,
	CORE_CAP_MAX,
};

/**
 * msm_vidc_prepare_dependency_list() api will prepare caps_list by looping over
 * enums(msm_vidc_inst_capability_type) from 0 to INST_CAP_MAX and arranges the
 * node in such a way that parents willbe at the front and dependent children
 * in the back.
 *
 * caps_list preparation may become CPU intensive task, so to save CPU cycles,
 * organize enum in proper order(leaf caps at the beginning and dependent parent caps
 * at back), so that during caps_list preparation num CPU cycles spent will reduce.
 *
 * Note: It will work, if enum kept at different places, but not efficient.
 *
 * - place all leaf(no child) enums before PROFILE cap.
 * - place all intermittent(having both parent and child) enums before FRAME_WIDTH cap.
 * - place all root(no parent) enums before INST_CAP_MAX cap.
 */

enum msm_vidc_inst_capability_type {
	INST_CAP_NONE = 0,
	MIN_FRAME_QP,
	MAX_FRAME_QP,
	I_FRAME_QP,
	P_FRAME_QP,
	B_FRAME_QP,
	TIME_DELTA_BASED_RC,
	CONSTANT_QUALITY,
	VBV_DELAY,
	PEAK_BITRATE,
	ENTROPY_MODE,
	TRANSFORM_8X8,
	STAGE,
	LTR_COUNT,
	IR_PERIOD,
	BITRATE_BOOST,
	OUTPUT_ORDER,
	INPUT_BUF_HOST_MAX_COUNT,
	OUTPUT_BUF_HOST_MAX_COUNT,
	VUI_TIMING_INFO,
	SLICE_DECODE,
	PROFILE,
	ENH_LAYER_COUNT,
	BIT_RATE,
	GOP_SIZE,
	B_FRAME,
	ALL_INTRA,
	MIN_QUALITY,
	SLICE_MODE,
	FRAME_WIDTH,
	LOSSLESS_FRAME_WIDTH,
	FRAME_HEIGHT,
	LOSSLESS_FRAME_HEIGHT,
	PIX_FMTS,
	MIN_BUFFERS_INPUT,
	MIN_BUFFERS_OUTPUT,
	MBPF,
	BATCH_MBPF,
	BATCH_FPS,
	LOSSLESS_MBPF,
	FRAME_RATE,
	OPERATING_RATE,
	INPUT_RATE,
	TIMESTAMP_RATE,
	SCALE_FACTOR,
	MB_CYCLES_VSP,
	MB_CYCLES_VPP,
	MB_CYCLES_LP,
	MB_CYCLES_FW,
	MB_CYCLES_FW_VPP,
	ENC_RING_BUFFER_COUNT,
	HFLIP,
	VFLIP,
	ROTATION,
	HEADER_MODE,
	PREPEND_SPSPPS_TO_IDR,
	WITHOUT_STARTCODE,
	NAL_LENGTH_FIELD,
	REQUEST_I_FRAME,
	BITRATE_MODE,
	LOSSLESS,
	FRAME_SKIP_MODE,
	FRAME_RC_ENABLE,
	GOP_CLOSURE,
	USE_LTR,
	MARK_LTR,
	BASELAYER_PRIORITY,
	IR_TYPE,
	AU_DELIMITER,
	GRID_ENABLE,
	GRID_SIZE,
	I_FRAME_MIN_QP,
	P_FRAME_MIN_QP,
	B_FRAME_MIN_QP,
	I_FRAME_MAX_QP,
	P_FRAME_MAX_QP,
	B_FRAME_MAX_QP,
	LAYER_TYPE,
	LAYER_ENABLE,
	L0_BR,
	L1_BR,
	L2_BR,
	L3_BR,
	L4_BR,
	L5_BR,
	LEVEL,
	HEVC_TIER,
	DISPLAY_DELAY_ENABLE,
	DISPLAY_DELAY,
	CONCEAL_COLOR_8BIT,
	CONCEAL_COLOR_10BIT,
	LF_MODE,
	LF_ALPHA,
	LF_BETA,
	SLICE_MAX_BYTES,
	SLICE_MAX_MB,
	MB_RC,
	CHROMA_QP_INDEX_OFFSET,
	PIPE,
	POC,
	CODED_FRAMES,
	BIT_DEPTH,
	BITSTREAM_SIZE_OVERWRITE,
	DEFAULT_HEADER,
	RAP_FRAME,
	SEQ_CHANGE_AT_SYNC_FRAME,
	QUALITY_MODE,
	CABAC_MAX_BITRATE,
	CAVLC_MAX_BITRATE,
	ALLINTRA_MAX_BITRATE,
	NUM_COMV,
	SIGNAL_COLOR_INFO,
	INST_CAP_MAX,
};

enum msm_vidc_inst_capability_flags {
	CAP_FLAG_NONE                    = 0,
	CAP_FLAG_DYNAMIC_ALLOWED         = BIT(0),
	CAP_FLAG_MENU                    = BIT(1),
	CAP_FLAG_INPUT_PORT              = BIT(2),
	CAP_FLAG_OUTPUT_PORT             = BIT(3),
	CAP_FLAG_CLIENT_SET              = BIT(4),
	CAP_FLAG_BITMASK                 = BIT(5),
	CAP_FLAG_VOLATILE                = BIT(6),
};

struct msm_vidc_inst_cap {
	enum msm_vidc_inst_capability_type cap_id;
	s32 min;
	s32 max;
	u32 step_or_mask;
	s32 value;
	u32 v4l2_id;
	u32 hfi_id;
	enum msm_vidc_inst_capability_flags flags;
	enum msm_vidc_inst_capability_type children[MAX_CAP_CHILDREN];
	int (*adjust)(void *inst,
		      struct v4l2_ctrl *ctrl);
	int (*set)(void *inst,
		   enum msm_vidc_inst_capability_type cap_id);
};

struct msm_vidc_inst_capability {
	enum msm_vidc_domain_type domain;
	enum msm_vidc_codec_type codec;
	struct msm_vidc_inst_cap cap[INST_CAP_MAX + 1];
};

struct msm_vidc_core_capability {
	enum msm_vidc_core_capability_type type;
	u32 value;
};

struct msm_vidc_inst_cap_entry {
	/* list of struct msm_vidc_inst_cap_entry */
	struct list_head list;
	enum msm_vidc_inst_capability_type cap_id;
};

struct msm_vidc_event_data {
	union {
		bool                         bval;
		u32                          uval;
		u64                          uval64;
		s32                          val;
		s64                          val64;
		void                        *ptr;
	} edata;
};

struct debug_buf_count {
	u64 etb;
	u64 ftb;
	u64 fbd;
	u64 ebd;
};

struct msm_vidc_statistics {
	struct debug_buf_count             count;
	u64                                data_size;
	u64                                time_ms;
	u32                                avg_bw_llcc;
	u32                                avg_bw_ddr;
};

enum msm_vidc_cache_op {
	MSM_VIDC_CACHE_CLEAN,
	MSM_VIDC_CACHE_INVALIDATE,
	MSM_VIDC_CACHE_CLEAN_INVALIDATE,
};

enum msm_vidc_dcvs_flags {
	MSM_VIDC_DCVS_INCR               = BIT(0),
	MSM_VIDC_DCVS_DECR               = BIT(1),
};

enum msm_vidc_clock_properties {
	CLOCK_PROP_HAS_SCALING           = BIT(0),
	CLOCK_PROP_HAS_MEM_RETENTION     = BIT(1),
};

enum signal_session_response {
	SIGNAL_CMD_STOP_INPUT = 0,
	SIGNAL_CMD_STOP_OUTPUT,
	SIGNAL_CMD_CLOSE,
	MAX_SIGNAL,
};

struct msm_vidc_input_cr_data {
	struct list_head       list;
	u32                    index;
	u32                    input_cr;
};

struct msm_vidc_session_idle {
	bool                   idle;
	u64                    last_activity_time_ns;
};

struct msm_vidc_color_info {
	u32 colorspace;
	u32 ycbcr_enc;
	u32 xfer_func;
	u32 quantization;
};

struct msm_vidc_rectangle {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};

struct msm_vidc_subscription_params {
	u32                    bitstream_resolution;
	u32                    crop_offsets[2];
	u32                    bit_depth;
	u32                    coded_frames;
	u32                    fw_min_count;
	u32                    pic_order_cnt;
	u32                    color_info;
	u32                    profile;
	u32                    level;
	u32                    tier;
};

struct msm_vidc_hfi_frame_info {
	u32                    picture_type;
	u32                    no_output;
	u32                    subframe_input;
	u32                    cr;
	u32                    cf;
	u32                    data_corrupt;
	u32                    overflow;
};

struct msm_vidc_decode_vpp_delay {
	bool                   enable;
	u32                    size;
};

struct msm_vidc_decode_batch {
	bool                   enable;
	u32                    size;
	struct delayed_work    work;
};

enum msm_vidc_power_mode {
	VIDC_POWER_NORMAL = 0,
	VIDC_POWER_LOW,
	VIDC_POWER_TURBO,
};

struct vidc_bus_vote_data {
	enum msm_vidc_domain_type domain;
	enum msm_vidc_codec_type codec;
	enum msm_vidc_power_mode power_mode;
	u32 color_formats[2];
	int num_formats; /* 1 = DPB-OPB unified; 2 = split */
	int input_height, input_width, bitrate;
	int output_height, output_width;
	int rotation;
	int compression_ratio;
	int complexity_factor;
	int input_cr;
	u32 lcu_size;
	u32 fps;
	u32 work_mode;
	bool use_sys_cache;
	bool b_frames_enabled;
	u64 calc_bw_ddr;
	u64 calc_bw_llcc;
	u32 num_vpp_pipes;
};

struct msm_vidc_power {
	enum msm_vidc_power_mode power_mode;
	u32                    buffer_counter;
	u32                    min_threshold;
	u32                    nom_threshold;
	u32                    max_threshold;
	bool                   dcvs_mode;
	u32                    dcvs_window;
	u64                    min_freq;
	u64                    curr_freq;
	u32                    ddr_bw;
	u32                    sys_cache_bw;
	u32                    dcvs_flags;
	u32                    fw_cr;
	u32                    fw_cf;
};

struct msm_vidc_mem {
	struct list_head            list;
	enum msm_vidc_buffer_type   type;
	enum msm_vidc_buffer_region region;
	u32                         size;
	u8                          secure:1;
	u8                          map_kernel:1;
	struct dma_buf             *dmabuf;
	struct iosys_map            dmabuf_map;
	void                       *kvaddr;
	dma_addr_t                  device_addr;
	unsigned long               attrs;
	u32                         refcount;
	struct sg_table            *table;
	struct dma_buf_attachment  *attach;
	enum dma_data_direction     direction;
};

struct msm_vidc_mem_list {
	struct list_head            list; // list of "struct msm_vidc_mem"
};

struct msm_vidc_buffer {
	struct list_head                   list;
	struct msm_vidc_inst              *inst;
	enum msm_vidc_buffer_type          type;
	enum msm_vidc_buffer_region        region;
	u32                                index;
	int                                fd;
	u32                                buffer_size;
	u32                                data_offset;
	u32                                data_size;
	u64                                device_addr;
	u32                                flags;
	u64                                timestamp;
	enum msm_vidc_buffer_attributes    attr;
	void                              *dmabuf;
	struct sg_table                   *sg_table;
	struct dma_buf_attachment         *attach;
	u32                                dbuf_get:1;
	u32                                start_time_ms;
	u32                                end_time_ms;
};

struct msm_vidc_buffers {
	struct list_head       list; // list of "struct msm_vidc_buffer"
	u32                    min_count;
	u32                    extra_count;
	u32                    actual_count;
	u32                    size;
	bool                   reuse;
};

struct msm_vidc_buffer_stats {
	struct list_head                   list;
	u32                                frame_num;
	u64                                timestamp;
	u32                                etb_time_ms;
	u32                                ebd_time_ms;
	u32                                ftb_time_ms;
	u32                                fbd_time_ms;
	u32                                data_size;
	u32                                flags;
	u32                                ts_offset;
};

enum msm_vidc_buffer_stats_flag {
	MSM_VIDC_STATS_FLAG_CORRUPT        = BIT(0),
	MSM_VIDC_STATS_FLAG_OVERFLOW       = BIT(1),
	MSM_VIDC_STATS_FLAG_NO_OUTPUT      = BIT(2),
	MSM_VIDC_STATS_FLAG_SUBFRAME_INPUT = BIT(3),
};

struct msm_vidc_sort {
	struct list_head       list;
	s64                    val;
};

struct msm_vidc_timestamp {
	struct msm_vidc_sort   sort;
	u64                    rank;
};

struct msm_vidc_timestamps {
	struct list_head       list;
	u32                    count;
	u64                    rank;
};

struct msm_vidc_input_timer {
	struct list_head       list;
	u64                    time_us;
};

enum msm_vidc_allow {
	MSM_VIDC_DISALLOW,
	MSM_VIDC_ALLOW,
	MSM_VIDC_DEFER,
	MSM_VIDC_DISCARD,
	MSM_VIDC_IGNORE,
};

struct msm_vidc_sfr {
	u32 bufsize;
	u8 rg_data[];
};

struct msm_vidc_ctrl_data {
	bool skip_s_ctrl;
};

#endif // _MSM_VIDC_INTERNAL_H_
