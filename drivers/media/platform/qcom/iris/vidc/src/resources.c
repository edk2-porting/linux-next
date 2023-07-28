// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/interconnect.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/sort.h>

#include "msm_vidc_core.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_power.h"
#include "venus_hfi.h"

/* Less than 50MBps is treated as trivial BW change */
#define TRIVIAL_BW_THRESHOLD 50000
#define TRIVIAL_BW_CHANGE(a, b) \
	((a) > (b) ? (a) - (b) < TRIVIAL_BW_THRESHOLD : \
		(b) - (a) < TRIVIAL_BW_THRESHOLD)

enum reset_state {
	INIT = 1,
	ASSERT,
	DEASSERT,
};

/* A comparator to compare loads (needed later on) */
static inline int cmp(const void *a, const void *b)
{
	/* want to sort in reverse so flip the comparison */
	return ((struct freq_table *)b)->freq -
		((struct freq_table *)a)->freq;
}

static void __fatal_error(bool fatal)
{
	WARN_ON(fatal);
}

static void devm_llcc_release(void *res)
{
	llcc_slice_putd((struct llcc_slice_desc *)res);
}

static struct llcc_slice_desc *devm_llcc_get(struct device *dev, u32 id)
{
	struct llcc_slice_desc *llcc = NULL;
	int rc = 0;

	llcc = llcc_slice_getd(id);
	if (!llcc)
		return NULL;

	/**
	 * register release callback with devm, so that when device goes
	 * out of scope(during remove sequence), devm will take care of
	 * de-register part by invoking release callback.
	 */
	rc = devm_add_action_or_reset(dev, devm_llcc_release, (void *)llcc);
	if (rc)
		return NULL;

	return llcc;
}

static void devm_pd_release(void *res)
{
	struct device *pd = (struct device *)res;

	d_vpr_h("%s(): %s\n", __func__, dev_name(pd));
	dev_pm_domain_detach(pd, true);
}

static struct device *devm_pd_get(struct device *dev, const char *name)
{
	struct device *pd = NULL;
	int rc = 0;

	pd = dev_pm_domain_attach_by_name(dev, name);
	if (!pd) {
		d_vpr_e("%s: pm domain attach failed %s\n", __func__, name);
		return NULL;
	}

	rc = devm_add_action_or_reset(dev, devm_pd_release, (void *)pd);
	if (rc) {
		d_vpr_e("%s: add action or reset failed %s\n", __func__, name);
		return NULL;
	}

	return pd;
}

static void devm_opp_dl_release(void *res)
{
	struct device_link *link = (struct device_link *)res;

	d_vpr_h("%s(): %s\n", __func__, dev_name(&link->link_dev));
	device_link_del(link);
}

static int devm_opp_dl_get(struct device *dev, struct device *supplier)
{
	u32 flag = DL_FLAG_RPM_ACTIVE | DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS;
	struct device_link *link = NULL;
	int rc = 0;

	link = device_link_add(dev, supplier, flag);
	if (!link) {
		d_vpr_e("%s: device link add failed\n", __func__);
		return -EINVAL;
	}

	rc = devm_add_action_or_reset(dev, devm_opp_dl_release, (void *)link);
	if (rc) {
		d_vpr_e("%s: add action or reset failed\n", __func__);
		return rc;
	}

	return rc;
}

static void devm_pm_runtime_put_sync(void *res)
{
	struct device *dev = (struct device *)res;

	d_vpr_h("%s(): %s\n", __func__, dev_name(dev));
	pm_runtime_put_sync(dev);
}

static int devm_pm_runtime_get_sync(struct device *dev)
{
	int rc = 0;

	rc = pm_runtime_get_sync(dev);
	if (rc < 0) {
		d_vpr_e("%s: pm domain get sync failed\n", __func__);
		return rc;
	}

	rc = devm_add_action_or_reset(dev, devm_pm_runtime_put_sync, (void *)dev);
	if (rc) {
		d_vpr_e("%s: add action or reset failed\n", __func__);
		return rc;
	}

	return rc;
}

static int __opp_set_rate(struct msm_vidc_core *core, u64 freq)
{
	unsigned long opp_freq = 0;
	struct dev_pm_opp *opp;
	int rc = 0;

	opp_freq = freq;

	/* find max(ceil) freq from opp table */
	opp = dev_pm_opp_find_freq_ceil(&core->pdev->dev, &opp_freq);
	if (IS_ERR(opp)) {
		opp = dev_pm_opp_find_freq_floor(&core->pdev->dev, &opp_freq);
		if (IS_ERR(opp)) {
			d_vpr_e("%s: unable to find freq %lld in opp table\n", __func__, freq);
			return -EINVAL;
		}
	}
	dev_pm_opp_put(opp);

	/* print freq value */
	d_vpr_h("%s: set rate %lu (requested %llu)\n",
		__func__, opp_freq, freq);

	/* scale freq to power up mxc & mmcx */
	rc = dev_pm_opp_set_rate(&core->pdev->dev, opp_freq);
	if (rc) {
		d_vpr_e("%s: failed to set rate\n", __func__);
		return rc;
	}

	return rc;
}

static int __init_register_base(struct msm_vidc_core *core)
{
	struct msm_vidc_resource *res;

	res = core->resource;

	res->register_base_addr = devm_platform_ioremap_resource(core->pdev, 0);
	if (IS_ERR(res->register_base_addr)) {
		d_vpr_e("%s: map reg addr failed %ld\n",
			__func__, PTR_ERR(res->register_base_addr));
		return -EINVAL;
	}
	d_vpr_h("%s: reg_base %p\n", __func__, res->register_base_addr);

	return 0;
}

static int __init_irq(struct msm_vidc_core *core)
{
	struct msm_vidc_resource *res;
	int rc = 0;

	res = core->resource;

	res->irq = platform_get_irq(core->pdev, 0);

	if (res->irq < 0)
		d_vpr_e("%s: get irq failed, %d\n", __func__, res->irq);

	d_vpr_h("%s: irq %d\n", __func__, res->irq);

	rc = devm_request_threaded_irq(&core->pdev->dev, res->irq, venus_hfi_isr,
				       venus_hfi_isr_handler, IRQF_TRIGGER_HIGH, "msm-vidc", core);
	if (rc) {
		d_vpr_e("%s: Failed to allocate venus IRQ\n", __func__);
		return rc;
	}
	disable_irq_nosync(res->irq);

	return rc;
}

static int __init_bus(struct msm_vidc_core *core)
{
	const struct bw_table *bus_tbl;
	struct bus_set *interconnects;
	struct bus_info *binfo = NULL;
	u32 bus_count = 0, cnt = 0;
	int rc = 0;

	interconnects = &core->resource->bus_set;

	bus_tbl = core->platform->data.bw_tbl;
	bus_count = core->platform->data.bw_tbl_size;

	if (!bus_tbl || !bus_count) {
		d_vpr_e("%s: invalid bus tbl %p or count %d\n",
			__func__, bus_tbl, bus_count);
		return -EINVAL;
	}

	/* allocate bus_set */
	interconnects->bus_tbl = devm_kzalloc(&core->pdev->dev,
					      sizeof(*interconnects->bus_tbl) * bus_count,
					      GFP_KERNEL);
	if (!interconnects->bus_tbl) {
		d_vpr_e("%s: failed to alloc memory for bus table\n", __func__);
		return -ENOMEM;
	}
	interconnects->count = bus_count;

	/* populate bus field from platform data */
	for (cnt = 0; cnt < interconnects->count; cnt++) {
		interconnects->bus_tbl[cnt].name = bus_tbl[cnt].name;
		interconnects->bus_tbl[cnt].min_kbps = bus_tbl[cnt].min_kbps;
		interconnects->bus_tbl[cnt].max_kbps = bus_tbl[cnt].max_kbps;
	}

	/* print bus fields */
	venus_hfi_for_each_bus(core, binfo) {
		d_vpr_h("%s: name %s min_kbps %u max_kbps %u\n",
			__func__, binfo->name, binfo->min_kbps, binfo->max_kbps);
	}

	/* get interconnect handle */
	venus_hfi_for_each_bus(core, binfo) {
		binfo->icc = devm_of_icc_get(&core->pdev->dev, binfo->name);
		if (IS_ERR_OR_NULL(binfo->icc)) {
			d_vpr_e("%s: failed to get bus: %s\n", __func__, binfo->name);
			rc = PTR_ERR_OR_ZERO(binfo->icc) ?
				PTR_ERR_OR_ZERO(binfo->icc) : -EBADHANDLE;
			binfo->icc = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_power_domains(struct msm_vidc_core *core)
{
	struct power_domain_info *pdinfo = NULL;
	const struct pd_table *pd_tbl;
	struct power_domain_set *pds;
	struct device **opp_vdevs = NULL;
	const char * const *opp_tbl;
	u32 pd_count = 0, opp_count = 0, cnt = 0;
	int rc = 0;

	pds = &core->resource->power_domain_set;

	pd_tbl = core->platform->data.pd_tbl;
	pd_count = core->platform->data.pd_tbl_size;

	/* skip init if power domain not supported */
	if (!pd_count) {
		d_vpr_h("%s: power domain entries not available in db\n", __func__);
		return 0;
	}

	/* sanitize power domain table */
	if (!pd_tbl) {
		d_vpr_e("%s: invalid power domain tbl\n", __func__);
		return -EINVAL;
	}

	/* allocate power_domain_set */
	pds->power_domain_tbl = devm_kzalloc(&core->pdev->dev,
					     sizeof(*pds->power_domain_tbl) * pd_count,
					     GFP_KERNEL);
	if (!pds->power_domain_tbl) {
		d_vpr_e("%s: failed to alloc memory for pd table\n", __func__);
		return -ENOMEM;
	}
	pds->count = pd_count;

	/* populate power domain fields */
	for (cnt = 0; cnt < pds->count; cnt++)
		pds->power_domain_tbl[cnt].name = pd_tbl[cnt].name;

	/* print power domain fields */
	venus_hfi_for_each_power_domain(core, pdinfo)
		d_vpr_h("%s: pd name %s\n", __func__, pdinfo->name);

	/* get power domain handle */
	venus_hfi_for_each_power_domain(core, pdinfo) {
		pdinfo->genpd_dev = devm_pd_get(&core->pdev->dev, pdinfo->name);
		if (IS_ERR_OR_NULL(pdinfo->genpd_dev)) {
			rc = PTR_ERR_OR_ZERO(pdinfo->genpd_dev) ?
				PTR_ERR_OR_ZERO(pdinfo->genpd_dev) : -EBADHANDLE;
			d_vpr_e("%s: failed to get pd: %s\n", __func__, pdinfo->name);
			pdinfo->genpd_dev = NULL;
			return rc;
		}
	}

	opp_tbl = core->platform->data.opp_tbl;
	opp_count = core->platform->data.opp_tbl_size;

	/* skip init if opp not supported */
	if (opp_count < 2) {
		d_vpr_h("%s: opp entries not available\n", __func__);
		return 0;
	}

	/* sanitize opp table */
	if (!opp_tbl) {
		d_vpr_e("%s: invalid opp table\n", __func__);
		return -EINVAL;
	}

	/* ignore NULL entry at the end of table */
	opp_count -= 1;

	/* print opp table entries */
	for (cnt = 0; cnt < opp_count; cnt++)
		d_vpr_h("%s: opp name %s\n", __func__, opp_tbl[cnt]);

	/* populate opp power domains(for rails) */
	rc = devm_pm_opp_attach_genpd(&core->pdev->dev, opp_tbl, &opp_vdevs);
	if (rc)
		return rc;

	/* create device_links b/w consumer(dev) and multiple suppliers(mx, mmcx) */
	for (cnt = 0; cnt < opp_count; cnt++) {
		rc = devm_opp_dl_get(&core->pdev->dev, opp_vdevs[cnt]);
		if (rc) {
			d_vpr_e("%s: failed to create dl: %s\n",
				__func__, dev_name(opp_vdevs[cnt]));
			return rc;
		}
	}

	/* initialize opp table from device tree */
	rc = devm_pm_opp_of_add_table(&core->pdev->dev);
	if (rc) {
		d_vpr_e("%s: failed to add opp table\n", __func__);
		return rc;
	}

	/**
	 * 1. power up mx & mmcx supply for RCG(mvs0_clk_src)
	 * 2. power up gdsc0c for mvs0c branch clk
	 * 3. power up gdsc0 for mvs0 branch clk
	 */

	/**
	 * power up mxc, mmcx rails to enable supply for
	 * RCG(video_cc_mvs0_clk_src)
	 */
	/* enable runtime pm */
	rc = devm_pm_runtime_enable(&core->pdev->dev);
	if (rc) {
		d_vpr_e("%s: failed to enable runtime pm\n", __func__);
		return rc;
	}
	/* power up rails(mxc & mmcx) */
	rc = devm_pm_runtime_get_sync(&core->pdev->dev);
	if (rc) {
		d_vpr_e("%s: failed to get sync runtime pm\n", __func__);
		return rc;
	}

	return rc;
}

static int __init_clocks(struct msm_vidc_core *core)
{
	const struct clk_table *clk_tbl;
	struct clock_set *clocks;
	struct clock_info *cinfo = NULL;
	u32 clk_count = 0, cnt = 0;
	int rc = 0;

	clocks = &core->resource->clock_set;

	clk_tbl = core->platform->data.clk_tbl;
	clk_count = core->platform->data.clk_tbl_size;

	if (!clk_tbl || !clk_count) {
		d_vpr_e("%s: invalid clock tbl %p or count %d\n",
			__func__, clk_tbl, clk_count);
		return -EINVAL;
	}

	/* allocate clock_set */
	clocks->clock_tbl = devm_kzalloc(&core->pdev->dev,
					 sizeof(*clocks->clock_tbl) * clk_count,
					 GFP_KERNEL);
	if (!clocks->clock_tbl) {
		d_vpr_e("%s: failed to alloc memory for clock table\n", __func__);
		return -ENOMEM;
	}
	clocks->count = clk_count;

	/* populate clock field from platform data */
	for (cnt = 0; cnt < clocks->count; cnt++) {
		clocks->clock_tbl[cnt].name = clk_tbl[cnt].name;
		clocks->clock_tbl[cnt].clk_id = clk_tbl[cnt].clk_id;
		clocks->clock_tbl[cnt].has_scaling = clk_tbl[cnt].scaling;
	}

	/* print clock fields */
	venus_hfi_for_each_clock(core, cinfo) {
		d_vpr_h("%s: clock name %s clock id %#x scaling %d\n",
			__func__, cinfo->name, cinfo->clk_id, cinfo->has_scaling);
	}

	/* get clock handle */
	venus_hfi_for_each_clock(core, cinfo) {
		cinfo->clk = devm_clk_get(&core->pdev->dev, cinfo->name);
		if (IS_ERR_OR_NULL(cinfo->clk)) {
			d_vpr_e("%s: failed to get clock: %s\n", __func__, cinfo->name);
			rc = PTR_ERR_OR_ZERO(cinfo->clk) ?
				PTR_ERR_OR_ZERO(cinfo->clk) : -EINVAL;
			cinfo->clk = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_reset_clocks(struct msm_vidc_core *core)
{
	const struct clk_rst_table *rst_tbl;
	struct reset_set *rsts;
	struct reset_info *rinfo = NULL;
	u32 rst_count = 0, cnt = 0;
	int rc = 0;

	rsts = &core->resource->reset_set;

	rst_tbl = core->platform->data.clk_rst_tbl;
	rst_count = core->platform->data.clk_rst_tbl_size;

	if (!rst_tbl || !rst_count) {
		d_vpr_e("%s: invalid reset tbl %p or count %d\n",
			__func__, rst_tbl, rst_count);
		return -EINVAL;
	}

	/* allocate reset_set */
	rsts->reset_tbl = devm_kzalloc(&core->pdev->dev,
				       sizeof(*rsts->reset_tbl) * rst_count,
				       GFP_KERNEL);
	if (!rsts->reset_tbl) {
		d_vpr_e("%s: failed to alloc memory for reset table\n", __func__);
		return -ENOMEM;
	}
	rsts->count = rst_count;

	/* populate clock field from platform data */
	for (cnt = 0; cnt < rsts->count; cnt++) {
		rsts->reset_tbl[cnt].name = rst_tbl[cnt].name;
		rsts->reset_tbl[cnt].exclusive_release = rst_tbl[cnt].exclusive_release;
	}

	/* print reset clock fields */
	venus_hfi_for_each_reset_clock(core, rinfo) {
		d_vpr_h("%s: reset clk %s, exclusive %d\n",
			__func__, rinfo->name, rinfo->exclusive_release);
	}

	/* get reset clock handle */
	venus_hfi_for_each_reset_clock(core, rinfo) {
		if (rinfo->exclusive_release)
			rinfo->rst = devm_reset_control_get_exclusive_released(&core->pdev->dev,
									       rinfo->name);
		else
			rinfo->rst = devm_reset_control_get(&core->pdev->dev, rinfo->name);
		if (IS_ERR_OR_NULL(rinfo->rst)) {
			d_vpr_e("%s: failed to get reset clock: %s\n", __func__, rinfo->name);
			rc = PTR_ERR_OR_ZERO(rinfo->rst) ?
				PTR_ERR_OR_ZERO(rinfo->rst) : -EINVAL;
			rinfo->rst = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_subcaches(struct msm_vidc_core *core)
{
	const struct subcache_table *llcc_tbl;
	struct subcache_set *caches;
	struct subcache_info *sinfo = NULL;
	u32 llcc_count = 0, cnt = 0;
	int rc = 0;

	caches = &core->resource->subcache_set;

	/* skip init if subcache not available */
	if (!is_sys_cache_present(core))
		return 0;

	llcc_tbl = core->platform->data.subcache_tbl;
	llcc_count = core->platform->data.subcache_tbl_size;

	if (!llcc_tbl || !llcc_count) {
		d_vpr_e("%s: invalid llcc tbl %p or count %d\n",
			__func__, llcc_tbl, llcc_count);
		return -EINVAL;
	}

	/* allocate clock_set */
	caches->subcache_tbl = devm_kzalloc(&core->pdev->dev,
					    sizeof(*caches->subcache_tbl) * llcc_count,
					    GFP_KERNEL);
	if (!caches->subcache_tbl) {
		d_vpr_e("%s: failed to alloc memory for subcache table\n", __func__);
		return -ENOMEM;
	}
	caches->count = llcc_count;

	/* populate subcache fields from platform data */
	for (cnt = 0; cnt < caches->count; cnt++) {
		caches->subcache_tbl[cnt].name = llcc_tbl[cnt].name;
		caches->subcache_tbl[cnt].llcc_id = llcc_tbl[cnt].llcc_id;
	}

	/* print subcache fields */
	venus_hfi_for_each_subcache(core, sinfo) {
		d_vpr_h("%s: name %s subcache id %d\n",
			__func__, sinfo->name, sinfo->llcc_id);
	}

	/* get subcache/llcc handle */
	venus_hfi_for_each_subcache(core, sinfo) {
		sinfo->subcache = devm_llcc_get(&core->pdev->dev, sinfo->llcc_id);
		if (IS_ERR_OR_NULL(sinfo->subcache)) {
			d_vpr_e("%s: failed to get subcache: %d\n", __func__, sinfo->llcc_id);
			rc = PTR_ERR_OR_ZERO(sinfo->subcache) ?
				PTR_ERR_OR_ZERO(sinfo->subcache) : -EBADHANDLE;
			sinfo->subcache = NULL;
			return rc;
		}
	}

	return rc;
}

static int __init_freq_table(struct msm_vidc_core *core)
{
	struct freq_table *freq_tbl;
	struct freq_set *clks;
	u32 freq_count = 0, cnt = 0;
	int rc = 0;

	clks = &core->resource->freq_set;

	freq_tbl = core->platform->data.freq_tbl;
	freq_count = core->platform->data.freq_tbl_size;

	if (!freq_tbl || !freq_count) {
		d_vpr_e("%s: invalid freq tbl %p or count %d\n",
			__func__, freq_tbl, freq_count);
		return -EINVAL;
	}

	/* allocate freq_set */
	clks->freq_tbl = devm_kzalloc(&core->pdev->dev,
				      sizeof(*clks->freq_tbl) * freq_count,
				      GFP_KERNEL);
	if (!clks->freq_tbl) {
		d_vpr_e("%s: failed to alloc memory for freq table\n", __func__);
		return -ENOMEM;
	}
	clks->count = freq_count;

	/* populate freq field from platform data */
	for (cnt = 0; cnt < clks->count; cnt++)
		clks->freq_tbl[cnt].freq = freq_tbl[cnt].freq;

	/* sort freq table */
	sort(clks->freq_tbl, clks->count, sizeof(*clks->freq_tbl), cmp, NULL);

	/* print freq field freq_set */
	d_vpr_h("%s: updated freq table\n", __func__);
	for (cnt = 0; cnt < clks->count; cnt++)
		d_vpr_h("%s:\t %lu\n", __func__, clks->freq_tbl[cnt].freq);

	return rc;
}

static int __init_context_banks(struct msm_vidc_core *core)
{
	const struct context_bank_table *cb_tbl;
	struct context_bank_set *cbs;
	struct context_bank_info *cbinfo = NULL;
	u32 cb_count = 0, cnt = 0;
	int rc = 0;

	cbs = &core->resource->context_bank_set;

	cb_tbl = core->platform->data.context_bank_tbl;
	cb_count = core->platform->data.context_bank_tbl_size;

	if (!cb_tbl || !cb_count) {
		d_vpr_e("%s: invalid context bank tbl %p or count %d\n",
			__func__, cb_tbl, cb_count);
		return -EINVAL;
	}

	/* allocate context_bank table */
	cbs->context_bank_tbl = devm_kzalloc(&core->pdev->dev,
					     sizeof(*cbs->context_bank_tbl) * cb_count,
					     GFP_KERNEL);
	if (!cbs->context_bank_tbl) {
		d_vpr_e("%s: failed to alloc memory for context_bank table\n", __func__);
		return -ENOMEM;
	}
	cbs->count = cb_count;

	/**
	 * populate context bank field from platform data except
	 * dev & domain which are assigned as part of context bank
	 * probe sequence
	 */
	for (cnt = 0; cnt < cbs->count; cnt++) {
		cbs->context_bank_tbl[cnt].name = cb_tbl[cnt].name;
		cbs->context_bank_tbl[cnt].addr_range.start = cb_tbl[cnt].start;
		cbs->context_bank_tbl[cnt].addr_range.size = cb_tbl[cnt].size;
		cbs->context_bank_tbl[cnt].secure = cb_tbl[cnt].secure;
		cbs->context_bank_tbl[cnt].dma_coherant = cb_tbl[cnt].dma_coherant;
		cbs->context_bank_tbl[cnt].region = cb_tbl[cnt].region;
		cbs->context_bank_tbl[cnt].dma_mask = cb_tbl[cnt].dma_mask;
	}

	/* print context_bank fiels */
	venus_hfi_for_each_context_bank(core, cbinfo) {
		d_vpr_h("%s: name %s addr start %#x size %#x secure %d\n",
			__func__, cbinfo->name, cbinfo->addr_range.start,
			cbinfo->addr_range.size, cbinfo->secure);

		d_vpr_h("%s: coherant %d region %d dma_mask %llu\n",
			__func__, cbinfo->dma_coherant, cbinfo->region,
			cbinfo->dma_mask);
	}

	return rc;
}

static int __enable_power_domains(struct msm_vidc_core *core, const char *name)
{
	struct power_domain_info *pdinfo = NULL;
	int rc = 0;

	/* power up rails(mxc & mmcx) to enable RCG(video_cc_mvs0_clk_src) */
	rc = __opp_set_rate(core, ULONG_MAX);
	if (rc) {
		d_vpr_e("%s: opp setrate failed\n", __func__);
		return rc;
	}

	/* power up (gdsc0/gdsc0c) to enable (mvs0/mvs0c) branch clock */
	venus_hfi_for_each_power_domain(core, pdinfo) {
		if (strcmp(pdinfo->name, name))
			continue;

		rc = pm_runtime_get_sync(pdinfo->genpd_dev);
		if (rc < 0) {
			d_vpr_e("%s: failed to get sync: %s\n", __func__, pdinfo->name);
			return rc;
		}
		d_vpr_h("%s: enabled power doamin %s\n", __func__, pdinfo->name);
	}

	return rc;
}

static int __disable_power_domains(struct msm_vidc_core *core, const char *name)
{
	struct power_domain_info *pdinfo = NULL;
	int rc = 0;

	/* power down (gdsc0/gdsc0c) to disable (mvs0/mvs0c) branch clock */
	venus_hfi_for_each_power_domain(core, pdinfo) {
		if (strcmp(pdinfo->name, name))
			continue;

		rc = pm_runtime_put_sync(pdinfo->genpd_dev);
		if (rc) {
			d_vpr_e("%s: failed to put sync: %s\n", __func__, pdinfo->name);
			return rc;
		}
		d_vpr_h("%s: disabled power doamin %s\n", __func__, pdinfo->name);
	}

	/* power down rails(mxc & mmcx) to disable RCG(video_cc_mvs0_clk_src) */
	rc = __opp_set_rate(core, 0);
	if (rc) {
		d_vpr_e("%s: opp setrate failed\n", __func__);
		return rc;
	}
	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_GDSC_HANDOFF, 0, __func__);

	return rc;
}

static int __hand_off_power_domains(struct msm_vidc_core *core)
{
	msm_vidc_change_core_sub_state(core, 0, CORE_SUBSTATE_GDSC_HANDOFF, __func__);

	return 0;
}

static int __acquire_power_domains(struct msm_vidc_core *core)
{
	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_GDSC_HANDOFF, 0, __func__);

	return 0;
}

static int __disable_subcaches(struct msm_vidc_core *core)
{
	struct subcache_info *sinfo;
	int rc = 0;

	if (!is_sys_cache_present(core))
		return 0;

	/* De-activate subcaches */
	venus_hfi_for_each_subcache_reverse(core, sinfo) {
		if (!sinfo->isactive)
			continue;

		d_vpr_h("%s: De-activate subcache %s\n", __func__, sinfo->name);
		rc = llcc_slice_deactivate(sinfo->subcache);
		if (rc) {
			d_vpr_e("Failed to de-activate %s: %d\n",
				sinfo->name, rc);
		}
		sinfo->isactive = false;
	}

	return 0;
}

static int __enable_subcaches(struct msm_vidc_core *core)
{
	int rc = 0;
	u32 c = 0;
	struct subcache_info *sinfo;

	if (!is_sys_cache_present(core))
		return 0;

	/* Activate subcaches */
	venus_hfi_for_each_subcache(core, sinfo) {
		rc = llcc_slice_activate(sinfo->subcache);
		if (rc) {
			d_vpr_e("Failed to activate %s: %d\n", sinfo->name, rc);
			__fatal_error(true);
			goto err_activate_fail;
		}
		sinfo->isactive = true;
		d_vpr_h("Activated subcache %s\n", sinfo->name);
		c++;
	}

	d_vpr_h("Activated %d Subcaches to Venus\n", c);

	return 0;

err_activate_fail:
	__disable_subcaches(core);
	return rc;
}

static int llcc_enable(struct msm_vidc_core *core, bool enable)
{
	int ret;

	if (enable)
		ret = __enable_subcaches(core);
	else
		ret = __disable_subcaches(core);

	return ret;
}

static int __vote_bandwidth(struct bus_info *bus, unsigned long bw_kbps)
{
	int rc = 0;

	if (!bus->icc) {
		d_vpr_e("%s: invalid bus\n", __func__);
		return -EINVAL;
	}

	d_vpr_p("Voting bus %s to ab %lu kBps\n", bus->name, bw_kbps);

	rc = icc_set_bw(bus->icc, bw_kbps, 0);
	if (rc)
		d_vpr_e("Failed voting bus %s to ab %lu, rc=%d\n",
			bus->name, bw_kbps, rc);

	return rc;
}

static int __unvote_buses(struct msm_vidc_core *core)
{
	int rc = 0;
	struct bus_info *bus = NULL;

	core->power.bw_ddr = 0;
	core->power.bw_llcc = 0;

	venus_hfi_for_each_bus(core, bus) {
		rc = __vote_bandwidth(bus, 0);
		if (rc)
			goto err_unknown_device;
	}

err_unknown_device:
	return rc;
}

static int __vote_buses(struct msm_vidc_core *core,
			unsigned long bw_ddr, unsigned long bw_llcc)
{
	int rc = 0;
	struct bus_info *bus = NULL;
	unsigned long bw_kbps = 0, bw_prev = 0;
	enum vidc_bus_type type;

	venus_hfi_for_each_bus(core, bus) {
		if (bus && bus->icc) {
			type = get_type_frm_name(bus->name);

			if (type == DDR) {
				bw_kbps = bw_ddr;
				bw_prev = core->power.bw_ddr;
			} else if (type == LLCC) {
				bw_kbps = bw_llcc;
				bw_prev = core->power.bw_llcc;
			} else {
				bw_kbps = bus->max_kbps;
				bw_prev = core->power.bw_ddr ?
						bw_kbps : 0;
			}

			/* ensure freq is within limits */
			bw_kbps = clamp_t(typeof(bw_kbps), bw_kbps,
					  bus->min_kbps, bus->max_kbps);

			if (TRIVIAL_BW_CHANGE(bw_kbps, bw_prev) && bw_prev) {
				d_vpr_l("Skip voting bus %s to %lu kBps\n",
					bus->name, bw_kbps);
				continue;
			}

			rc = __vote_bandwidth(bus, bw_kbps);

			if (type == DDR)
				core->power.bw_ddr = bw_kbps;
			else if (type == LLCC)
				core->power.bw_llcc = bw_kbps;
		} else {
			d_vpr_e("No BUS to Vote\n");
		}
	}

	return rc;
}

static int set_bw(struct msm_vidc_core *core, unsigned long bw_ddr,
		  unsigned long bw_llcc)
{
	if (!bw_ddr && !bw_llcc)
		return __unvote_buses(core);

	return __vote_buses(core, bw_ddr, bw_llcc);
}

static int __set_clk_rate(struct msm_vidc_core *core, struct clock_info *cl,
			  u64 rate)
{
	int rc = 0;

	/* bail early if requested clk rate is not changed */
	if (rate == cl->prev)
		return 0;

	d_vpr_p("Scaling clock %s to %llu, prev %llu\n",
		cl->name, rate, cl->prev);

	rc = clk_set_rate(cl->clk, rate);
	if (rc) {
		d_vpr_e("%s: Failed to set clock rate %llu %s: %d\n",
			__func__, rate, cl->name, rc);
		return rc;
	}

	cl->prev = rate;

	return rc;
}

static int __set_clocks(struct msm_vidc_core *core, u64 freq)
{
	struct clock_info *cl;
	int rc = 0;

	/* scale mxc & mmcx rails */
	rc = __opp_set_rate(core, freq);
	if (rc) {
		d_vpr_e("%s: opp setrate failed %lld\n", __func__, freq);
		return rc;
	}

	venus_hfi_for_each_clock(core, cl) {
		if (cl->has_scaling) {
			rc = __set_clk_rate(core, cl, freq);
			if (rc)
				return rc;
		}
	}

	return 0;
}

static int __disable_unprepare_clock(struct msm_vidc_core *core,
				     const char *clk_name)
{
	int rc = 0;
	struct clock_info *cl;
	bool found;

	found = false;
	venus_hfi_for_each_clock(core, cl) {
		if (!cl->clk) {
			d_vpr_e("%s: invalid clock %s\n", __func__, cl->name);
			return -EINVAL;
		}
		if (strcmp(cl->name, clk_name))
			continue;
		found = true;
		clk_disable_unprepare(cl->clk);
		if (cl->has_scaling)
			__set_clk_rate(core, cl, 0);
		cl->prev = 0;
		d_vpr_h("%s: clock %s disable unprepared\n", __func__, cl->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: clock %s not found\n", __func__, clk_name);
		return -EINVAL;
	}

	return rc;
}

static int __prepare_enable_clock(struct msm_vidc_core *core,
				  const char *clk_name)
{
	int rc = 0;
	struct clock_info *cl;
	bool found;
	u64 rate = 0;

	found = false;
	venus_hfi_for_each_clock(core, cl) {
		if (!cl->clk) {
			d_vpr_e("%s: invalid clock\n", __func__);
			return -EINVAL;
		}
		if (strcmp(cl->name, clk_name))
			continue;
		found = true;
		/*
		 * For the clocks we control, set the rate prior to preparing
		 * them.  Since we don't really have a load at this point, scale
		 * it to the lowest frequency possible
		 */
		if (cl->has_scaling) {
			rate = clk_round_rate(cl->clk, 0);
			/**
			 * source clock is already multipled with scaling ratio and __set_clk_rate
			 * attempts to multiply again. So divide scaling ratio before calling
			 * __set_clk_rate.
			 */
			rate = rate / MSM_VIDC_CLOCK_SOURCE_SCALING_RATIO;
			__set_clk_rate(core, cl, rate);
		}

		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			d_vpr_e("%s: failed to enable clock %s\n",
				__func__, cl->name);
			return rc;
		}
		if (!__clk_is_enabled(cl->clk)) {
			d_vpr_e("%s: clock %s not enabled\n",
				__func__, cl->name);
			clk_disable_unprepare(cl->clk);
			if (cl->has_scaling)
				__set_clk_rate(core, cl, 0);
			return -EINVAL;
		}
		d_vpr_h("%s: clock %s prepare enabled\n", __func__, cl->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: clock %s not found\n", __func__, clk_name);
		return -EINVAL;
	}

	return rc;
}

static int __init_resources(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = __init_register_base(core);
	if (rc)
		return rc;

	rc = __init_irq(core);
	if (rc)
		return rc;

	rc = __init_bus(core);
	if (rc)
		return rc;

	rc = call_res_op(core, gdsc_init, core);
	if (rc)
		return rc;

	rc = __init_clocks(core);
	if (rc)
		return rc;

	rc = __init_reset_clocks(core);
	if (rc)
		return rc;

	rc = __init_subcaches(core);
	if (rc)
		return rc;

	rc = __init_freq_table(core);
	if (rc)
		return rc;

	rc = __init_context_banks(core);

	return rc;
}

static int __reset_control_acquire_name(struct msm_vidc_core *core,
					const char *name)
{
	struct reset_info *rcinfo = NULL;
	int rc = 0;
	bool found = false;

	venus_hfi_for_each_reset_clock(core, rcinfo) {
		if (strcmp(rcinfo->name, name))
			continue;

		/* this function is valid only for exclusive_release reset clocks*/
		if (!rcinfo->exclusive_release) {
			d_vpr_e("%s: unsupported reset control (%s), exclusive %d\n",
				__func__, name, rcinfo->exclusive_release);
			return -EINVAL;
		}

		found = true;

		rc = reset_control_acquire(rcinfo->rst);
		if (rc)
			d_vpr_e("%s: failed to acquire reset control (%s), rc = %d\n",
				__func__, rcinfo->name, rc);
		else
			d_vpr_h("%s: acquire reset control (%s)\n",
				__func__, rcinfo->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: reset control (%s) not found\n", __func__, name);
		rc = -EINVAL;
	}

	return rc;
}

static int __reset_control_release_name(struct msm_vidc_core *core,
					const char *name)
{
	struct reset_info *rcinfo = NULL;
	int rc = 0;
	bool found = false;

	venus_hfi_for_each_reset_clock(core, rcinfo) {
		if (strcmp(rcinfo->name, name))
			continue;

		/* this function is valid only for exclusive_release reset clocks*/
		if (!rcinfo->exclusive_release) {
			d_vpr_e("%s: unsupported reset control (%s), exclusive %d\n",
				__func__, name, rcinfo->exclusive_release);
			return -EINVAL;
		}

		found = true;

		reset_control_release(rcinfo->rst);
		if (rc)
			d_vpr_e("%s: release reset control (%s) failed\n",
				__func__, rcinfo->name);
		else
			d_vpr_h("%s: release reset control (%s) done\n",
				__func__, rcinfo->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: reset control (%s) not found\n", __func__, name);
		rc = -EINVAL;
	}

	return rc;
}

static int __reset_control_assert_name(struct msm_vidc_core *core,
				       const char *name)
{
	struct reset_info *rcinfo = NULL;
	int rc = 0;
	bool found = false;

	venus_hfi_for_each_reset_clock(core, rcinfo) {
		if (strcmp(rcinfo->name, name))
			continue;

		found = true;
		rc = reset_control_assert(rcinfo->rst);
		if (rc)
			d_vpr_e("%s: failed to assert reset control (%s), rc = %d\n",
				__func__, rcinfo->name, rc);
		else
			d_vpr_h("%s: assert reset control (%s)\n",
				__func__, rcinfo->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: reset control (%s) not found\n", __func__, name);
		rc = -EINVAL;
	}

	return rc;
}

static int __reset_control_deassert_name(struct msm_vidc_core *core,
					 const char *name)
{
	struct reset_info *rcinfo = NULL;
	int rc = 0;
	bool found = false;

	venus_hfi_for_each_reset_clock(core, rcinfo) {
		if (strcmp(rcinfo->name, name))
			continue;
		found = true;
		rc = reset_control_deassert(rcinfo->rst);
		if (rc)
			d_vpr_e("%s: deassert reset control for (%s) failed, rc %d\n",
				__func__, rcinfo->name, rc);
		else
			d_vpr_h("%s: deassert reset control (%s)\n",
				__func__, rcinfo->name);
		break;
	}
	if (!found) {
		d_vpr_e("%s: reset control (%s) not found\n", __func__, name);
		rc = -EINVAL;
	}

	return rc;
}

static int __reset_control_deassert(struct msm_vidc_core *core)
{
	struct reset_info *rcinfo = NULL;
	int rc = 0;

	venus_hfi_for_each_reset_clock(core, rcinfo) {
		rc = reset_control_deassert(rcinfo->rst);
		if (rc) {
			d_vpr_e("%s: deassert reset control failed. rc = %d\n", __func__, rc);
			continue;
		}
		d_vpr_h("%s: deassert reset control %s\n", __func__, rcinfo->name);
	}

	return rc;
}

static int __reset_control_assert(struct msm_vidc_core *core)
{
	struct reset_info *rcinfo = NULL;
	int rc = 0, cnt = 0;

	venus_hfi_for_each_reset_clock(core, rcinfo) {
		if (!rcinfo->rst) {
			d_vpr_e("%s: invalid reset clock %s\n",
				__func__, rcinfo->name);
			return -EINVAL;
		}
		rc = reset_control_assert(rcinfo->rst);
		if (rc) {
			d_vpr_e("%s: failed to assert reset control %s, rc = %d\n",
				__func__, rcinfo->name, rc);
			goto deassert_reset_control;
		}
		cnt++;
		d_vpr_h("%s: assert reset control %s, count %d\n", __func__, rcinfo->name, cnt);

		usleep_range(1000, 1100);
	}

	return rc;
deassert_reset_control:
	venus_hfi_for_each_reset_clock_reverse_continue(core, rcinfo, cnt) {
		d_vpr_e("%s: deassert reset control %s\n", __func__, rcinfo->name);
		reset_control_deassert(rcinfo->rst);
	}

	return rc;
}

static int __reset_ahb2axi_bridge(struct msm_vidc_core *core)
{
	int rc = 0;

	rc = __reset_control_assert(core);
	if (rc)
		return rc;

	rc = __reset_control_deassert(core);

	return rc;
}

static const struct msm_vidc_resources_ops res_ops = {
	.init = __init_resources,
	.reset_bridge = __reset_ahb2axi_bridge,
	.reset_control_acquire = __reset_control_acquire_name,
	.reset_control_release = __reset_control_release_name,
	.reset_control_assert = __reset_control_assert_name,
	.reset_control_deassert = __reset_control_deassert_name,
	.gdsc_init = __init_power_domains,
	.gdsc_on = __enable_power_domains,
	.gdsc_off = __disable_power_domains,
	.gdsc_hw_ctrl = __hand_off_power_domains,
	.gdsc_sw_ctrl = __acquire_power_domains,
	.llcc = llcc_enable,
	.set_bw = set_bw,
	.set_clks = __set_clocks,
	.clk_enable = __prepare_enable_clock,
	.clk_disable = __disable_unprepare_clock,
};

const struct msm_vidc_resources_ops *get_resources_ops(void)
{
	return &res_ops;
}
