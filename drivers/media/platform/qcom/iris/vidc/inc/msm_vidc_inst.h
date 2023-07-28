/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_INST_H_
#define _MSM_VIDC_INST_H_

#include "hfi_property.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_memory.h"
#include "msm_vidc_state.h"

#define call_session_op(c, op, ...)			\
	(((c) && (c)->session_ops && (c)->session_ops->op) ? \
	((c)->session_ops->op(__VA_ARGS__)) : 0)

struct msm_vidc_session_ops {
	u64 (*calc_freq)(struct msm_vidc_inst *inst, u32 data_size);
	int (*calc_bw)(struct msm_vidc_inst *inst,
		       struct vidc_bus_vote_data *vote_data);
	int (*decide_work_route)(struct msm_vidc_inst *inst);
	int (*decide_work_mode)(struct msm_vidc_inst *inst);
	int (*decide_quality_mode)(struct msm_vidc_inst *inst);
	int (*buffer_size)(struct msm_vidc_inst *inst, enum msm_vidc_buffer_type type);
	int (*min_count)(struct msm_vidc_inst *inst, enum msm_vidc_buffer_type type);
	int (*extra_count)(struct msm_vidc_inst *inst, enum msm_vidc_buffer_type type);
	int (*ring_buf_count)(struct msm_vidc_inst *inst, u32 data_size);
};

struct msm_vidc_mem_list_info {
	struct msm_vidc_mem_list        bin;
	struct msm_vidc_mem_list        arp;
	struct msm_vidc_mem_list        comv;
	struct msm_vidc_mem_list        non_comv;
	struct msm_vidc_mem_list        line;
	struct msm_vidc_mem_list        dpb;
	struct msm_vidc_mem_list        persist;
	struct msm_vidc_mem_list        vpss;
};

struct msm_vidc_buffers_info {
	struct msm_vidc_buffers        input;
	struct msm_vidc_buffers        output;
	struct msm_vidc_buffers        read_only;
	struct msm_vidc_buffers        bin;
	struct msm_vidc_buffers        arp;
	struct msm_vidc_buffers        comv;
	struct msm_vidc_buffers        non_comv;
	struct msm_vidc_buffers        line;
	struct msm_vidc_buffers        dpb;
	struct msm_vidc_buffers        persist;
	struct msm_vidc_buffers        vpss;
};

struct buf_queue {
	struct vb2_queue *vb2q;
};

/**
 * struct msm_vidc_inst - holds per instance parameters
 *
 * @list: used for attach an instance to the core
 * @lock: lock for this structure
 * @ctx_q_lock:  lock to serialize ioctls calls related to queues
 * @client_lock: lock to serialize ioctls
 * @state: instnace state
 * @event_handle: handler for different v4l2 ioctls
 * @sub_state: substate of instance
 * @sub_state_name: substate name
 * @domain: domain type: encoder or decoder
 * @codec: codec type
 * @core: pointer to core structure
 * @kref: instance reference
 * @session_id: id of current session
 * @debug_str: debug string
 * @packet: HFI packet
 * @packet_size: HFI packet size
 * @fmts: structure of v4l2_format
 * @ctrl_handler: reference of v4l2 ctrl handler
 * @fh: reference of v4l2 file handler
 * @m2m_dev: m2m device handle
 * @m2m_ctx: m2m device context
 * @num_ctrls: supported number of controls
 * @hfi_rc_type: type of HFI rate control
 * @hfi_layer_type: type of HFI layer encoding
 * @bufq: array of vb2 queue
 * @crop: structure of crop info
 * @compose: structure of compose info
 * @power: structure of power info
 * @bus_data: structure of bus data
 * @pool: array of memory pool of buffers
 * @buffers: structure of buffer info
 * @mem_info: structure of memory info
 * @timestamps: structure of timestamp related info
 * @subcr_params: array of subscription params which driver subscribes to fw
 * @hfi_frame_info: structure of frame info
 * @decode_batch: strtucre of decode batch
 * @decode_vpp_delay: structure for vpp delay related info
 * @session_idle: structure of idle session related info
 * @stats_work: delayed work for buffer stats
 * @workq: pointer to workqueue
 * @enc_input_crs: list head of input compression rations
 * @dmabuf_tracker: list head of dambuf tracker
 * @input_timer_list: list head of input timer
 * @caps_list: list head of capability
 * @children_list: list head of children list of caps
 * @firmware_list: list head of fw list of cap which will be set to cap
 * @buffer_stats_list: list head of buffer stats
 * @once_per_session_set: boolean to set once per session property
 * @ipsc_properties_set: boolean to set ipsc properties to fw
 * @opsc_properties_set: boolean to set opsc properties to fw
 * @caps_list_prepared: boolean to prepare capability list
 * @debugfs_root: root of debugfs
 * @debug_count: count for ETBs, EBDs, FTBs and FBDs
 * @stats: structure for bw stats
 * @capabilities: array of supported instance capabilities
 * @completions: structure of signal completions
 * @active: boolean for active instances
 * @last_qbuf_time_ns: time of last qbuf to driver
 * @initial_time_us: start timestamp
 * @max_input_data_size: max size of input data
 * @dpb_list_payload: array of dpb buffers
 * @input_dpb_list_enabled: boolean for input dpb buffer list
 * @output_dpb_list_enabled: boolean for output dpb buffer list
 * @max_rate: max input rate
 * @has_bframe: boolean for B frame
 * @ir_enabled: boolean for intra refresh
 * @iframe: boolean for I frame
 * @fw_min_count: minimnum count of buffers needed by fw
 */

struct msm_vidc_inst {
	struct list_head                   list;
	struct mutex                       lock; /* instance lock */
	/* lock to serialize IOCTL calls related to queues */
	struct mutex                       ctx_q_lock;
	struct mutex                       client_lock; /* lock to serialize IOCTLs */
	enum msm_vidc_state                state;
	int                              (*event_handle)(struct msm_vidc_inst *inst,
							 enum msm_vidc_event event, void *data);
	enum msm_vidc_sub_state            sub_state;
	char                               sub_state_name[MAX_NAME_LENGTH];
	enum msm_vidc_domain_type          domain;
	enum msm_vidc_codec_type           codec;
	void                              *core;
	struct kref                        kref;
	u32                                session_id;
	u8                                 debug_str[24];
	void                              *packet;
	u32                                packet_size;
	struct v4l2_format                 fmts[MAX_PORT];
	struct v4l2_ctrl_handler           ctrl_handler;
	struct v4l2_fh                     fh;
	struct v4l2_m2m_dev               *m2m_dev;
	struct v4l2_m2m_ctx               *m2m_ctx;
	u32                                num_ctrls;
	enum hfi_rate_control              hfi_rc_type;
	enum hfi_layer_encoding_type       hfi_layer_type;
	struct buf_queue                   bufq[MAX_PORT];
	struct msm_vidc_rectangle          crop;
	struct msm_vidc_rectangle          compose;
	struct msm_vidc_power              power;
	struct vidc_bus_vote_data          bus_data;
	struct msm_memory_pool             pool[MSM_MEM_POOL_MAX];
	struct msm_vidc_buffers_info       buffers;
	struct msm_vidc_mem_list_info      mem_info;
	struct msm_vidc_timestamps         timestamps;
	struct msm_vidc_subscription_params       subcr_params[MAX_PORT];
	struct msm_vidc_hfi_frame_info     hfi_frame_info;
	struct msm_vidc_decode_batch       decode_batch;
	struct msm_vidc_decode_vpp_delay   decode_vpp_delay;
	struct msm_vidc_session_idle       session_idle;
	struct delayed_work                stats_work;
	struct workqueue_struct           *workq;
	struct list_head                   enc_input_crs;
	struct list_head                   dmabuf_tracker; /* struct msm_memory_dmabuf */
	struct list_head                   input_timer_list; /* struct msm_vidc_input_timer */
	struct list_head                   caps_list;
	struct list_head                   children_list; /* struct msm_vidc_inst_cap_entry */
	struct list_head                   firmware_list; /* struct msm_vidc_inst_cap_entry */
	struct list_head                   buffer_stats_list; /* struct msm_vidc_buffer_stats */
	bool                               once_per_session_set;
	bool                               ipsc_properties_set;
	bool                               opsc_properties_set;
	bool                               caps_list_prepared;
	struct dentry                     *debugfs_root;
	struct debug_buf_count             debug_count;
	struct msm_vidc_statistics         stats;
	struct msm_vidc_inst_cap           capabilities[INST_CAP_MAX + 1];
	struct completion                  completions[MAX_SIGNAL];
	bool                               active;
	u64                                last_qbuf_time_ns;
	u64                                initial_time_us;
	u32                                max_input_data_size;
	u32                                dpb_list_payload[MAX_DPB_LIST_ARRAY_SIZE];
	bool                               input_dpb_list_enabled;
	bool                               output_dpb_list_enabled;
	u32                                max_rate;
	bool                               has_bframe;
	bool                               ir_enabled;
	bool                               iframe;
	u32                                fw_min_count;
};

#endif // _MSM_VIDC_INST_H_
