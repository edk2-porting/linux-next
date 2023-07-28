/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_VIDC_CORE_H_
#define _MSM_VIDC_CORE_H_

#include <linux/platform_device.h>

#include "msm_vidc_internal.h"
#include "msm_vidc_state.h"
#include "resources.h"
#include "venus_hfi_queue.h"

#define MAX_EVENTS   30

#define call_iris_op(d, op, ...)			\
	(((d) && (d)->iris_ops && (d)->iris_ops->op) ? \
	((d)->iris_ops->op(__VA_ARGS__)) : 0)

struct msm_vidc_iris_ops {
	int (*boot_firmware)(struct msm_vidc_core *core);
	int (*raise_interrupt)(struct msm_vidc_core *core);
	int (*clear_interrupt)(struct msm_vidc_core *core);
	int (*prepare_pc)(struct msm_vidc_core *core);
	int (*power_on)(struct msm_vidc_core *core);
	int (*power_off)(struct msm_vidc_core *core);
	int (*watchdog)(struct msm_vidc_core *core, u32 intr_status);
};

struct msm_vidc_mem_addr {
	u32 align_device_addr;
	u8 *align_virtual_addr;
	u32 mem_size;
	struct msm_vidc_mem mem;
};

struct msm_vidc_iface_q_info {
	void *q_hdr;
	struct msm_vidc_mem_addr q_array;
};

struct msm_video_device {
	enum msm_vidc_domain_type              type;
	struct video_device                    vdev;
	struct v4l2_m2m_dev                   *m2m_dev;
};

struct msm_vidc_core_power {
	u64 clk_freq;
	u64 bw_ddr;
	u64 bw_llcc;
};

/**
 * struct msm_vidc_core - holds core parameters valid for all instances
 *
 * @pdev: refernce to platform device structure
 * @vdev: a reference to video device structure for encoder & decoder instances
 * @v4l2_dev : a holder for v4l2 device structure
 * @instances: a list_head of all instances
 * @dangling_instances : a list_head of all dangling instances
 * @debugfs_parent: debugfs node for msm_vidc
 * @debugfs_root: debugfs node for core info
 * @fw_version: a holder for fw version
 * @state: a structure of core states
 * @state_handle: a handler for core state change
 * @sub_state: enumeration of core substate
 * @sub_state_name: holder for core substate name
 * @lock: a lock for this strucure
 * @resources: a structure for core resources
 * @platform: a structure for platform data
 * @intr_status: interrupt status
 * @spur_count: counter for spurious interrupt
 * @reg_count: counter for interrupts
 * @enc_codecs_count: encoder codec count
 * @dec_codecs_count: decoder codec count
 * @capabilities: an array for supported core capabilities
 * @inst_caps: a pointer to supported instance capabilities
 * @sfr: SFR register memory
 * @iface_q_table: Interface queue table memory
 * @iface_queues: a array of interface queues info
 * @pm_work: delayed work to handle power collapse
 * @pm_workq: workqueue for power collapse work
 * @batch_workq: workqueue for batching
 * @fw_unload_work: delayed work for fw unload
 * @power: a sturture for core power
 * @skip_pc_count: a counter for skipped power collpase
 * @last_packet_type: holder for last packet type info
 * @packet: pointer to packet from driver to fw
 * @packet_size: size of packet
 * @response_packet: a pointer to response packet from fw to driver
 * @v4l2_file_ops: a pointer to v4l2 file ops
 * @v4l2_ioctl_ops_enc: a pointer to v4l2 ioctl ops for encoder
 * @v4l2_ioctl_ops_dec: a pointer to v4l2 ioclt ops for decoder
 * @v4l2_ctrl_ops: a pointer to v4l2 control ops
 * @vb2_ops: a pointer to vb2 ops
 * @vb2_mem_ops: a pointer to vb2 memory ops
 * @v4l2_m2m_ops: a pointer to v4l2_mem ops
 * @iris_ops: a pointer to iris ops
 * @res_ops: a pointer to resource management ops
 * @session_ops: a pointer to session level ops
 * @mem_ops: a pointer to memory management ops
 * @header_id: id of packet header
 * @packet_id: id of packet
 * @sys_init_id: id of sys init packet
 */

struct msm_vidc_core {
	struct platform_device                *pdev;
	struct msm_video_device                vdev[2];
	struct v4l2_device                     v4l2_dev;
	struct list_head                       instances;
	struct list_head                       dangling_instances;
	struct dentry                         *debugfs_parent;
	struct dentry                         *debugfs_root;
	char                                   fw_version[MAX_NAME_LENGTH];
	enum msm_vidc_core_state               state;
	int                                  (*state_handle)(struct msm_vidc_core *core,
							     enum msm_vidc_core_event_type type,
							     struct msm_vidc_event_data *data);
	enum msm_vidc_core_sub_state           sub_state;
	char                                   sub_state_name[MAX_NAME_LENGTH];
	struct mutex                           lock; /* lock for core structure */
	struct msm_vidc_resource              *resource;
	struct msm_vidc_platform              *platform;
	u32                                    intr_status;
	u32                                    spur_count;
	u32                                    reg_count;
	u32                                    enc_codecs_count;
	u32                                    dec_codecs_count;
	struct msm_vidc_core_capability        capabilities[CORE_CAP_MAX + 1];
	struct msm_vidc_inst_capability       *inst_caps;
	struct msm_vidc_mem_addr               sfr;
	struct msm_vidc_mem_addr               iface_q_table;
	struct msm_vidc_iface_q_info           iface_queues[VIDC_IFACEQ_NUMQ];
	struct delayed_work                    pm_work;
	struct workqueue_struct               *pm_workq;
	struct workqueue_struct               *batch_workq;
	struct delayed_work                    fw_unload_work;
	struct msm_vidc_core_power             power;
	u32                                    skip_pc_count;
	u32                                    last_packet_type;
	u8                                    *packet;
	u32                                    packet_size;
	u8                                    *response_packet;
	struct v4l2_file_operations           *v4l2_file_ops;
	const struct v4l2_ioctl_ops                 *v4l2_ioctl_ops_enc;
	const struct v4l2_ioctl_ops                 *v4l2_ioctl_ops_dec;
	const struct v4l2_ctrl_ops                  *v4l2_ctrl_ops;
	const struct vb2_ops                        *vb2_ops;
	struct vb2_mem_ops                    *vb2_mem_ops;
	struct v4l2_m2m_ops                   *v4l2_m2m_ops;
	struct msm_vidc_iris_ops              *iris_ops;
	const struct msm_vidc_resources_ops   *res_ops;
	struct msm_vidc_session_ops           *session_ops;
	const struct msm_vidc_memory_ops      *mem_ops;
	u32                                    header_id;
	u32                                    packet_id;
	u32                                    sys_init_id;
};

#endif // _MSM_VIDC_CORE_H_
