// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2022, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/stringify.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#include "msm_vidc_core.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_driver.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_memory.h"
#include "msm_vidc_platform.h"
#include "msm_vidc_state.h"
#include "venus_hfi.h"

#define BASE_DEVICE_NUMBER 32

struct msm_vidc_core *g_core;

static inline bool is_video_device(struct device *dev)
{
	return !!(of_device_is_compatible(dev->of_node, "qcom,sm8550-vidc"));
}

static inline bool is_video_context_bank_device(struct device *dev)
{
	return !!(of_device_is_compatible(dev->of_node, "qcom,vidc,cb-ns"));
}

static int msm_vidc_init_resources(struct msm_vidc_core *core)
{
	struct msm_vidc_resource *res = NULL;
	int rc = 0;

	res = devm_kzalloc(&core->pdev->dev, sizeof(*res), GFP_KERNEL);
	if (!res) {
		d_vpr_e("%s: failed to alloc memory for resource\n", __func__);
		return -ENOMEM;
	}
	core->resource = res;

	rc = call_res_op(core, init, core);
	if (rc) {
		d_vpr_e("%s: Failed to init resources: %d\n", __func__, rc);
		return rc;
	}

	return 0;
}

static const struct of_device_id msm_vidc_dt_match[] = {
	{.compatible = "qcom,sm8550-vidc"},
	{.compatible = "qcom,vidc,cb-ns"},
	MSM_VIDC_EMPTY_BRACE
};
MODULE_DEVICE_TABLE(of, msm_vidc_dt_match);

static void msm_vidc_release_video_device(struct video_device *vdev)
{
	d_vpr_e("%s: video device released\n", __func__);
}

static void msm_vidc_unregister_video_device(struct msm_vidc_core *core,
					     enum msm_vidc_domain_type type)
{
	int index;

	if (type == MSM_VIDC_DECODER)
		index = 0;
	else if (type == MSM_VIDC_ENCODER)
		index = 1;
	else
		return;

	v4l2_m2m_release(core->vdev[index].m2m_dev);

	video_set_drvdata(&core->vdev[index].vdev, NULL);
	video_unregister_device(&core->vdev[index].vdev);
}

static int msm_vidc_register_video_device(struct msm_vidc_core *core,
					  enum msm_vidc_domain_type type, int nr)
{
	int rc = 0;
	int index;

	d_vpr_h("%s: domain %d\n", __func__, type);

	if (type == MSM_VIDC_DECODER)
		index = 0;
	else if (type == MSM_VIDC_ENCODER)
		index = 1;
	else
		return -EINVAL;

	core->vdev[index].vdev.release =
		msm_vidc_release_video_device;
	core->vdev[index].vdev.fops = core->v4l2_file_ops;
	if (type == MSM_VIDC_DECODER)
		core->vdev[index].vdev.ioctl_ops = core->v4l2_ioctl_ops_dec;
	else
		core->vdev[index].vdev.ioctl_ops = core->v4l2_ioctl_ops_enc;
	core->vdev[index].vdev.vfl_dir = VFL_DIR_M2M;
	core->vdev[index].type = type;
	core->vdev[index].vdev.v4l2_dev = &core->v4l2_dev;
	core->vdev[index].vdev.device_caps = core->capabilities[DEVICE_CAPS].value;
	rc = video_register_device(&core->vdev[index].vdev,
				   VFL_TYPE_VIDEO, nr);
	if (rc) {
		d_vpr_e("Failed to register the video device\n");
		return rc;
	}
	video_set_drvdata(&core->vdev[index].vdev, core);

	core->vdev[index].m2m_dev = v4l2_m2m_init(core->v4l2_m2m_ops);
	if (IS_ERR(core->vdev[index].m2m_dev)) {
		d_vpr_e("Failed to initialize V4L2 M2M device\n");
		rc = PTR_ERR(core->vdev[index].m2m_dev);
		goto m2m_init_failed;
	}

	return 0;

m2m_init_failed:
	video_unregister_device(&core->vdev[index].vdev);
	return rc;
}

static int msm_vidc_deinitialize_core(struct msm_vidc_core *core)
{
	int rc = 0;

	if (!core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	mutex_destroy(&core->lock);
	msm_vidc_update_core_state(core, MSM_VIDC_CORE_DEINIT, __func__);

	if (core->batch_workq)
		destroy_workqueue(core->batch_workq);

	if (core->pm_workq)
		destroy_workqueue(core->pm_workq);

	core->batch_workq = NULL;
	core->pm_workq = NULL;

	return rc;
}

static int msm_vidc_initialize_core(struct msm_vidc_core *core)
{
	int rc = 0;

	msm_vidc_update_core_state(core, MSM_VIDC_CORE_DEINIT, __func__);

	core->pm_workq = create_singlethread_workqueue("pm_workq");
	if (!core->pm_workq) {
		d_vpr_e("%s: create pm workq failed\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	core->batch_workq = create_singlethread_workqueue("batch_workq");
	if (!core->batch_workq) {
		d_vpr_e("%s: create batch workq failed\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	core->packet_size = VIDC_IFACEQ_VAR_HUGE_PKT_SIZE;
	core->packet = devm_kzalloc(&core->pdev->dev, core->packet_size, GFP_KERNEL);
	if (!core->packet) {
		d_vpr_e("%s: failed to alloc core packet\n", __func__);
		rc = -ENOMEM;
		goto exit;
	}

	core->response_packet = devm_kzalloc(&core->pdev->dev, core->packet_size, GFP_KERNEL);
	if (!core->packet) {
		d_vpr_e("%s: failed to alloc core response packet\n", __func__);
		rc = -ENOMEM;
		goto exit;
	}

	mutex_init(&core->lock);
	INIT_LIST_HEAD(&core->instances);
	INIT_LIST_HEAD(&core->dangling_instances);

	INIT_DELAYED_WORK(&core->pm_work, venus_hfi_pm_work_handler);
	INIT_DELAYED_WORK(&core->fw_unload_work, msm_vidc_fw_unload_handler);

	return 0;
exit:
	if (core->batch_workq)
		destroy_workqueue(core->batch_workq);
	if (core->pm_workq)
		destroy_workqueue(core->pm_workq);
	core->batch_workq = NULL;
	core->pm_workq = NULL;

	return rc;
}

static void msm_vidc_devm_deinit_core(void *res)
{
	struct msm_vidc_core *core = res;

	msm_vidc_deinitialize_core(core);
}

static int msm_vidc_devm_init_core(struct device *dev, struct msm_vidc_core *core)
{
	int rc = 0;

	if (!dev || !core) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_initialize_core(core);
	if (rc) {
		d_vpr_e("%s: init failed with %d\n", __func__, rc);
		return rc;
	}

	rc = devm_add_action_or_reset(dev, msm_vidc_devm_deinit_core, (void *)core);
	if (rc)
		return -EINVAL;

	return rc;
}

static void msm_vidc_devm_debugfs_put(void *res)
{
	struct dentry *parent = res;

	debugfs_remove_recursive(parent);
}

static struct dentry *msm_vidc_devm_debugfs_get(struct device *dev)
{
	struct dentry *parent = NULL;
	int rc = 0;

	if (!dev) {
		d_vpr_e("%s: invalid params\n", __func__);
		return NULL;
	}

	parent = msm_vidc_debugfs_init_drv();
	if (!parent)
		return NULL;

	rc = devm_add_action_or_reset(dev, msm_vidc_devm_debugfs_put, (void *)parent);
	if (rc)
		return NULL;

	return parent;
}

static int msm_vidc_setup_context_bank(struct msm_vidc_core *core,
				       struct device *dev)
{
	struct context_bank_info *cb = NULL;
	int rc = 0;

	cb = msm_vidc_get_context_bank_for_device(core, dev);
	if (!cb) {
		d_vpr_e("%s: Failed to get context bank device for %s\n",
			__func__, dev_name(dev));
		return -EIO;
	}

	/* populate dev & domain field */
	cb->dev = dev;
	cb->domain = iommu_get_domain_for_dev(cb->dev);
	if (!cb->domain) {
		d_vpr_e("%s: Failed to get iommu domain for %s\n", __func__, dev_name(dev));
		return -EIO;
	}

	if (cb->dma_mask) {
		rc = dma_set_mask_and_coherent(cb->dev, cb->dma_mask);
		if (rc) {
			d_vpr_e("%s: dma_set_mask_and_coherent failed\n", __func__);
			return rc;
		}
	}

	/*
	 * configure device segment size and segment boundary to ensure
	 * iommu mapping returns one mapping (which is required for partial
	 * cache operations)
	 */
	if (!dev->dma_parms)
		dev->dma_parms =
			devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
	dma_set_max_seg_size(dev, (unsigned int)DMA_BIT_MASK(32));
	dma_set_seg_boundary(dev, (unsigned long)DMA_BIT_MASK(64));

	iommu_set_fault_handler(cb->domain, msm_vidc_smmu_fault_handler, (void *)core);

	d_vpr_h("%s: name %s addr start %x size %x secure %d\n",
		__func__, cb->name, cb->addr_range.start,
		cb->addr_range.size, cb->secure);
	d_vpr_h("%s: dma_coherant %d region %d dev_name %s domain %pK dma_mask %llu\n",
		__func__, cb->dma_coherant, cb->region, dev_name(cb->dev),
		cb->domain, cb->dma_mask);

	return rc;
}

static int msm_vidc_remove_video_device(struct platform_device *pdev)
{
	struct msm_vidc_core *core;

	if (!pdev) {
		d_vpr_e("%s: invalid input %pK", __func__, pdev);
		return -EINVAL;
	}

	core = dev_get_drvdata(&pdev->dev);
	if (!core) {
		d_vpr_e("%s: invalid core\n", __func__);
		return -EINVAL;
	}

	msm_vidc_core_deinit(core, true);
	venus_hfi_queue_deinit(core);

	msm_vidc_unregister_video_device(core, MSM_VIDC_ENCODER);
	msm_vidc_unregister_video_device(core, MSM_VIDC_DECODER);

	v4l2_device_unregister(&core->v4l2_dev);

	d_vpr_h("depopulating sub devices\n");
	/*
	 * Trigger remove for each sub-device i.e. qcom,context-bank,xxxx
	 * When msm_vidc_remove is called for each sub-device, destroy
	 * context-bank mappings.
	 */
	of_platform_depopulate(&pdev->dev);

	dev_set_drvdata(&pdev->dev, NULL);
	g_core = NULL;
	d_vpr_h("%s(): succssful\n", __func__);

	return 0;
}

static int msm_vidc_remove_context_bank(struct platform_device *pdev)
{
	d_vpr_h("%s(): %s\n", __func__, dev_name(&pdev->dev));

	return 0;
}

static int msm_vidc_remove(struct platform_device *pdev)
{
	/*
	 * Sub devices remove will be triggered by of_platform_depopulate()
	 * after core_deinit(). It return immediately after completing
	 * sub-device remove.
	 */
	if (is_video_device(&pdev->dev))
		return msm_vidc_remove_video_device(pdev);
	else if (is_video_context_bank_device(&pdev->dev))
		return msm_vidc_remove_context_bank(pdev);

	/* How did we end up here? */
	WARN_ON(1);
	return -EINVAL;
}

static int msm_vidc_probe_video_device(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_vidc_core *core = NULL;
	int nr = BASE_DEVICE_NUMBER;

	d_vpr_h("%s: %s\n", __func__, dev_name(&pdev->dev));

	core = devm_kzalloc(&pdev->dev, sizeof(struct msm_vidc_core), GFP_KERNEL);
	if (!core) {
		d_vpr_e("%s: failed to alloc memory for core\n", __func__);
		return -ENOMEM;
	}
	g_core = core;

	core->pdev = pdev;
	dev_set_drvdata(&pdev->dev, core);

	core->debugfs_parent = msm_vidc_devm_debugfs_get(&pdev->dev);
	if (!core->debugfs_parent)
		d_vpr_h("Failed to create debugfs for msm_vidc\n");

	rc = msm_vidc_devm_init_core(&pdev->dev, core);
	if (rc) {
		d_vpr_e("%s: init core failed with %d\n", __func__, rc);
		goto init_core_failed;
	}

	rc = msm_vidc_init_platform(core);
	if (rc) {
		d_vpr_e("%s: init platform failed with %d\n", __func__, rc);
		rc = -EINVAL;
		goto init_plat_failed;
	}

	rc = msm_vidc_init_resources(core);
	if (rc) {
		d_vpr_e("%s: init resource failed with %d\n", __func__, rc);
		goto init_res_failed;
	}

	rc = msm_vidc_init_core_caps(core);
	if (rc) {
		d_vpr_e("%s: init core caps failed with %d\n", __func__, rc);
		goto init_res_failed;
	}

	rc = msm_vidc_init_instance_caps(core);
	if (rc) {
		d_vpr_e("%s: init inst cap failed with %d\n", __func__, rc);
		goto init_inst_caps_fail;
	}

	core->debugfs_root = msm_vidc_debugfs_init_core(core);
	if (!core->debugfs_root)
		d_vpr_h("Failed to init debugfs core\n");

	d_vpr_h("populating sub devices\n");
	/*
	 * Trigger probe for each sub-device i.e. qcom,msm-vidc,context-bank.
	 * When msm_vidc_probe is called for each sub-device, parse the
	 * context-bank details.
	 */
	rc = of_platform_populate(pdev->dev.of_node, msm_vidc_dt_match, NULL,
				  &pdev->dev);
	if (rc) {
		d_vpr_e("Failed to trigger probe for sub-devices\n");
		goto sub_dev_failed;
	}

	rc = v4l2_device_register(&pdev->dev, &core->v4l2_dev);
	if (rc) {
		d_vpr_e("Failed to register v4l2 device\n");
		goto v4l2_reg_failed;
	}

	/* setup the decoder device */
	rc = msm_vidc_register_video_device(core, MSM_VIDC_DECODER, nr);
	if (rc) {
		d_vpr_e("Failed to register video decoder\n");
		goto dec_reg_failed;
	}

	/* setup the encoder device */
	rc = msm_vidc_register_video_device(core, MSM_VIDC_ENCODER, nr + 1);
	if (rc) {
		d_vpr_e("Failed to register video encoder\n");
		goto enc_reg_failed;
	}

	rc = venus_hfi_queue_init(core);
	if (rc) {
		d_vpr_e("%s: interface queues init failed\n", __func__);
		goto queues_init_failed;
	}

	rc = msm_vidc_core_init(core);
	if (rc) {
		d_vpr_e("%s: sys init failed\n", __func__);
		goto core_init_failed;
	}

	d_vpr_h("%s(): succssful\n", __func__);

	return rc;

core_init_failed:
	venus_hfi_queue_deinit(core);
queues_init_failed:
	of_platform_depopulate(&pdev->dev);
sub_dev_failed:
	msm_vidc_unregister_video_device(core, MSM_VIDC_ENCODER);
enc_reg_failed:
	msm_vidc_unregister_video_device(core, MSM_VIDC_DECODER);
dec_reg_failed:
	v4l2_device_unregister(&core->v4l2_dev);
v4l2_reg_failed:
init_inst_caps_fail:
init_res_failed:
init_plat_failed:
init_core_failed:
	dev_set_drvdata(&pdev->dev, NULL);
	g_core = NULL;

	return rc;
}

static int msm_vidc_probe_context_bank(struct platform_device *pdev)
{
	struct msm_vidc_core *core = NULL;
	int rc = 0;

	if (!pdev) {
		d_vpr_e("%s: Invalid platform device %pK", __func__, pdev);
		return -EINVAL;
	} else if (!pdev->dev.parent) {
		d_vpr_e("%s: Failed to find a parent for %s\n",
			__func__, dev_name(&pdev->dev));
		return -ENODEV;
	}

	d_vpr_h("%s(): %s\n", __func__, dev_name(&pdev->dev));

	core = dev_get_drvdata(pdev->dev.parent);
	if (!core) {
		d_vpr_e("%s: core not found in device %s",
			__func__, dev_name(pdev->dev.parent));
		return -EINVAL;
	}

	rc = msm_vidc_setup_context_bank(core, &pdev->dev);
	if (rc) {
		d_vpr_e("%s: Failed to probe context bank %s\n",
			__func__, dev_name(&pdev->dev));
		return rc;
	}

	return rc;
}

static int msm_vidc_probe(struct platform_device *pdev)
{
	if (!pdev) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/*
	 * Sub devices probe will be triggered by of_platform_populate() towards
	 * the end of the probe function after msm-vidc device probe is
	 * completed. Return immediately after completing sub-device probe.
	 */
	if (is_video_device(&pdev->dev))
		return msm_vidc_probe_video_device(pdev);
	else if (is_video_context_bank_device(&pdev->dev))
		return msm_vidc_probe_context_bank(pdev);

	/* How did we end up here? */
	WARN_ON(1);
	return -EINVAL;
}

static int msm_vidc_pm_suspend(struct device *dev)
{
	int rc = 0;
	struct msm_vidc_core *core;
	enum msm_vidc_allow allow = MSM_VIDC_DISALLOW;

	/*
	 * Bail out if
	 * - driver possibly not probed yet
	 * - not the main device. We don't support power management on
	 *   subdevices (e.g. context banks)
	 */
	if (!dev || !dev->driver || !is_video_device(dev))
		return 0;

	core = dev_get_drvdata(dev);
	if (!core) {
		d_vpr_e("%s: invalid core\n", __func__);
		return -EINVAL;
	}

	core_lock(core, __func__);
	allow = msm_vidc_allow_pm_suspend(core);

	if (allow == MSM_VIDC_IGNORE) {
		d_vpr_h("%s: pm already suspended\n", __func__);
		msm_vidc_change_core_sub_state(core, 0, CORE_SUBSTATE_PM_SUSPEND, __func__);
		rc = 0;
		goto unlock;
	} else if (allow != MSM_VIDC_ALLOW) {
		d_vpr_h("%s: pm suspend not allowed\n", __func__);
		rc = 0;
		goto unlock;
	}

	rc = msm_vidc_suspend(core);
	if (rc == -EOPNOTSUPP)
		rc = 0;
	else if (rc)
		d_vpr_e("Failed to suspend: %d\n", rc);
	else
		msm_vidc_change_core_sub_state(core, 0, CORE_SUBSTATE_PM_SUSPEND, __func__);

unlock:
	core_unlock(core, __func__);
	return rc;
}

static int msm_vidc_pm_resume(struct device *dev)
{
	struct msm_vidc_core *core;

	/*
	 * Bail out if
	 * - driver possibly not probed yet
	 * - not the main device. We don't support power management on
	 *   subdevices (e.g. context banks)
	 */
	if (!dev || !dev->driver || !is_video_device(dev))
		return 0;

	core = dev_get_drvdata(dev);
	if (!core) {
		d_vpr_e("%s: invalid core\n", __func__);
		return -EINVAL;
	}

	/* remove PM suspend from core sub_state */
	core_lock(core, __func__);
	msm_vidc_change_core_sub_state(core, CORE_SUBSTATE_PM_SUSPEND, 0, __func__);
	core_unlock(core, __func__);

	return 0;
}

static const struct dev_pm_ops msm_vidc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(msm_vidc_pm_suspend, msm_vidc_pm_resume)
};

struct platform_driver msm_vidc_driver = {
	.probe = msm_vidc_probe,
	.remove = msm_vidc_remove,
	.driver = {
		.name = "msm_vidc_v4l2",
		.of_match_table = msm_vidc_dt_match,
		.pm = &msm_vidc_pm_ops,
	},
};

module_platform_driver(msm_vidc_driver);
MODULE_LICENSE("GPL");
