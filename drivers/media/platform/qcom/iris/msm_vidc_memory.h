/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021,, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_VIDC_MEMORY_H_
#define _MSM_VIDC_MEMORY_H_

#include "msm_vidc_internal.h"

struct msm_vidc_core;
struct msm_vidc_inst;

#define MSM_MEM_POOL_PACKET_SIZE 1024

/*
 * DMA_ATTR_NO_DELAYED_UNMAP: Used by msm specific lazy mapping to indicate
 * that the mapping can be freed on unmap, rather than when the ion_buffer
 * is freed.
 */
#define DMA_ATTR_NO_DELAYED_UNMAP	(1UL << 13)
/*
 * When passed to a DMA map call the DMA_ATTR_FORCE_COHERENT DMA
 * attribute can be used to force a buffer to be mapped as IO coherent.
 */
#define DMA_ATTR_FORCE_COHERENT			(1UL << 15)
/*
 * When passed to a DMA map call the DMA_ATTR_FORCE_NON_COHERENT DMA
 * attribute can be used to force a buffer to not be mapped as IO
 * coherent.
 */
#define DMA_ATTR_FORCE_NON_COHERENT		(1UL << 16)
/*
 * DMA_ATTR_DELAYED_UNMAP: Used by ION, it will ensure that mappings are not
 * removed on unmap but instead are removed when the ion_buffer is freed.
 */
#define DMA_ATTR_DELAYED_UNMAP		(1UL << 17)

/*
 * DMA_ATTR_IOMMU_USE_UPSTREAM_HINT: Normally an smmu will override any bus
 * attributes (i.e cacheablilty) provided by the client device. Some hardware
 * may be designed to use the original attributes instead.
 */
#define DMA_ATTR_IOMMU_USE_UPSTREAM_HINT	(DMA_ATTR_SYS_CACHE_ONLY)

/*
 * DMA_ATTR_IOMMU_USE_LLC_NWA: Overrides the bus attributes to use the System
 * Cache(LLC) with allocation policy as Inner Non-Cacheable, Outer Cacheable:
 * Write-Back, Read-Allocate, No Write-Allocate policy.
 */
#define DMA_ATTR_IOMMU_USE_LLC_NWA	(DMA_ATTR_SYS_CACHE_ONLY_NWA)

struct msm_memory_dmabuf {
	struct list_head       list;
	struct dma_buf        *dmabuf;
	u32                    refcount;
};

enum msm_memory_pool_type {
	MSM_MEM_POOL_BUFFER  = 0,
	MSM_MEM_POOL_MAP,
	MSM_MEM_POOL_ALLOC,
	MSM_MEM_POOL_TIMESTAMP,
	MSM_MEM_POOL_DMABUF,
	MSM_MEM_POOL_PACKET,
	MSM_MEM_POOL_BUF_TIMER,
	MSM_MEM_POOL_BUF_STATS,
	MSM_MEM_POOL_MAX,
};

struct msm_memory_alloc_header {
	struct list_head       list;
	u32                    type;
	bool                   busy;
	void                  *buf;
};

struct msm_memory_pool {
	u32                    size;
	char                  *name;
	struct list_head       free_pool; /* list of struct msm_memory_alloc_header */
	struct list_head       busy_pool; /* list of struct msm_memory_alloc_header */
};

int msm_vidc_memory_alloc(struct msm_vidc_core *core,
	struct msm_vidc_alloc *alloc);
int msm_vidc_memory_free(struct msm_vidc_core *core,
	struct msm_vidc_alloc *alloc);
int msm_vidc_memory_map(struct msm_vidc_core *core,
	struct msm_vidc_map *map);
int msm_vidc_memory_unmap(struct msm_vidc_core *core,
	struct msm_vidc_map *map);
struct dma_buf *msm_vidc_memory_get_dmabuf(struct msm_vidc_inst *inst,
	int fd);
void msm_vidc_memory_put_dmabuf(struct msm_vidc_inst *inst,
	struct dma_buf *dmabuf);
void msm_vidc_memory_put_dmabuf_completely(struct msm_vidc_inst *inst,
	struct msm_memory_dmabuf *buf);
int msm_memory_pools_init(struct msm_vidc_inst *inst);
void msm_memory_pools_deinit(struct msm_vidc_inst *inst);
void *msm_memory_pool_alloc(struct msm_vidc_inst *inst,
	enum msm_memory_pool_type type);
void msm_memory_pool_free(struct msm_vidc_inst *inst, void *vidc_buf);
int msm_vidc_vmem_alloc(unsigned long size, void **mem, const char *msg);
void msm_vidc_vmem_free(void **addr);
#endif // _MSM_VIDC_MEMORY_H_