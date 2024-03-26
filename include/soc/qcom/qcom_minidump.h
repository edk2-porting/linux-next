/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _QCOM_MINIDUMP_H_
#define _QCOM_MINIDUMP_H_

struct rproc;
struct rproc_dump_segment;

#if IS_ENABLED(CONFIG_QCOM_RPROC_MINIDUMP)
void qcom_rproc_minidump(struct rproc *rproc, unsigned int minidump_id,
		   void (*rproc_dumpfn_t)(struct rproc *rproc,
		   struct rproc_dump_segment *segment, void *dest, size_t offset,
		   size_t size));
#else
static inline void qcom_rproc_minidump(struct rproc *rproc, unsigned int minidump_id,
		   void (*rproc_dumpfn_t)(struct rproc *rproc,
		   struct rproc_dump_segment *segment, void *dest, size_t offset,
		   size_t size)) { }
#endif /* CONFIG_QCOM_RPROC_MINIDUMP */
#endif /* _QCOM_MINIDUMP_H_ */
