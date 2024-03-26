// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2016 Linaro Ltd
 * Copyright (C) 2015 Sony Mobile Communications Inc
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/remoteproc.h>
#include <linux/soc/qcom/smem.h>
#include <linux/string.h>
#include <soc/qcom/qcom_minidump.h>

#include "qcom_minidump_internal.h"

static void qcom_minidump_cleanup(struct rproc *rproc)
{
	struct rproc_dump_segment *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, &rproc->dump_segments, node) {
		list_del(&entry->node);
		kfree(entry->priv);
		kfree(entry);
	}
}

static int qcom_add_minidump_segments(struct rproc *rproc, struct minidump_subsystem *subsystem,
			void (*rproc_dumpfn_t)(struct rproc *rproc, struct rproc_dump_segment *segment,
				void *dest, size_t offset, size_t size))
{
	struct minidump_region __iomem *ptr;
	struct minidump_region region;
	int seg_cnt, i;
	dma_addr_t da;
	size_t size;
	char *name;

	if (WARN_ON(!list_empty(&rproc->dump_segments))) {
		dev_err(&rproc->dev, "dump segment list already populated\n");
		return -EUCLEAN;
	}

	seg_cnt = le32_to_cpu(subsystem->region_count);
	ptr = ioremap((unsigned long)le64_to_cpu(subsystem->regions_baseptr),
		      seg_cnt * sizeof(struct minidump_region));
	if (!ptr)
		return -EFAULT;

	for (i = 0; i < seg_cnt; i++) {
		memcpy_fromio(&region, ptr + i, sizeof(region));
		if (le32_to_cpu(region.valid) == MINIDUMP_REGION_VALID) {
			name = kstrndup(region.name, MAX_REGION_NAME_LENGTH - 1, GFP_KERNEL);
			if (!name) {
				iounmap(ptr);
				return -ENOMEM;
			}
			da = le64_to_cpu(region.address);
			size = le64_to_cpu(region.size);
			rproc_coredump_add_custom_segment(rproc, da, size, rproc_dumpfn_t, name);
		}
	}

	iounmap(ptr);
	return 0;
}

void qcom_rproc_minidump(struct rproc *rproc, unsigned int minidump_id,
		void (*rproc_dumpfn_t)(struct rproc *rproc,
		struct rproc_dump_segment *segment, void *dest, size_t offset,
		size_t size))
{
	int ret;
	struct minidump_subsystem *subsystem;
	struct minidump_global_toc *toc;

	/* Get Global minidump ToC*/
	toc = qcom_smem_get(QCOM_SMEM_HOST_ANY, SBL_MINIDUMP_SMEM_ID, NULL);

	/* check if global table pointer exists and init is set */
	if (IS_ERR(toc) || !toc->status) {
		dev_err(&rproc->dev, "Minidump TOC not found in SMEM\n");
		return;
	}

	/* Get subsystem table of contents using the minidump id */
	subsystem = &toc->subsystems[minidump_id];

	/**
	 * Collect minidump if SS ToC is valid and segment table
	 * is initialized in memory and encryption status is set.
	 */
	if (subsystem->regions_baseptr == 0 ||
	    le32_to_cpu(subsystem->status) != 1 ||
	    le32_to_cpu(subsystem->enabled) != MINIDUMP_SS_ENABLED ||
	    le32_to_cpu(subsystem->encryption_status) != MINIDUMP_SS_ENCR_DONE) {
		dev_err(&rproc->dev, "Minidump not ready, skipping\n");
		return;
	}

	ret = qcom_add_minidump_segments(rproc, subsystem, rproc_dumpfn_t);
	if (ret) {
		dev_err(&rproc->dev, "Failed with error: %d while adding minidump entries\n", ret);
		goto clean_minidump;
	}
	rproc_coredump_using_sections(rproc);
clean_minidump:
	qcom_minidump_cleanup(rproc);
}
EXPORT_SYMBOL_GPL(qcom_rproc_minidump);

MODULE_DESCRIPTION("Qualcomm remoteproc minidump(smem) helper module");
MODULE_LICENSE("GPL");
