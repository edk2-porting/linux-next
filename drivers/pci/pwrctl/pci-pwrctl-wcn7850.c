// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Linaro Ltd.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pci-pwrctl.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>

struct pci_pwrctl_wcn7850_vreg {
	const char *name;
	unsigned int load_uA;
};

struct pci_pwrctl_wcn7850_pdata {
	struct pci_pwrctl_wcn7850_vreg *vregs;
	size_t num_vregs;
	unsigned int delay_msec;
};

struct pci_pwrctl_wcn7850_ctx {
	struct pci_pwrctl pwrctl;
	const struct pci_pwrctl_wcn7850_pdata *pdata;
	struct regulator_bulk_data *regs;
	struct gpio_desc *en_gpio;
	struct clk *clk;
};

static struct pci_pwrctl_wcn7850_vreg pci_pwrctl_wcn7850_vregs[] = {
	{
		.name = "vdd",
		.load_uA = 16000,
	},
	{
		.name = "vddio",
		.load_uA = 5000,
	},
	{
		.name = "vddio1p2",
		.load_uA = 16000,
	},
	{
		.name = "vddaon",
		.load_uA = 26000,
	},
	{
		.name = "vdddig",
		.load_uA = 126000,
	},
	{
		.name = "vddrfa1p2",
		.load_uA = 257000,
	},
	{
		.name = "vddrfa1p8",
		.load_uA = 302000,
	},
};

static struct pci_pwrctl_wcn7850_pdata pci_pwrctl_wcn7850_of_data = {
	.vregs = pci_pwrctl_wcn7850_vregs,
	.num_vregs = ARRAY_SIZE(pci_pwrctl_wcn7850_vregs),
	.delay_msec = 50,
};

static int pci_pwrctl_wcn7850_power_on(struct pci_pwrctl_wcn7850_ctx *ctx)
{
	int ret;

	ret = regulator_bulk_enable(ctx->pdata->num_vregs, ctx->regs);
	if (ret)
		return ret;

	ret = clk_prepare_enable(ctx->clk);
	if (ret)
		return ret;

	gpiod_set_value_cansleep(ctx->en_gpio, 1);

	if (ctx->pdata->delay_msec)
		msleep(ctx->pdata->delay_msec);

	return 0;
}

static int pci_pwrctl_wcn7850_power_off(struct pci_pwrctl_wcn7850_ctx *ctx)
{
	gpiod_set_value_cansleep(ctx->en_gpio, 0);
	clk_disable_unprepare(ctx->clk);

	return regulator_bulk_disable(ctx->pdata->num_vregs, ctx->regs);
}

static void devm_pci_pwrctl_wcn7850_power_off(void *data)
{
	struct pci_pwrctl_wcn7850_ctx *ctx = data;

	pci_pwrctl_wcn7850_power_off(ctx);
}

static int pci_pwrctl_wcn7850_probe(struct platform_device *pdev)
{
	struct pci_pwrctl_wcn7850_ctx *ctx;
	struct device *dev = &pdev->dev;
	int ret, i;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->pdata = of_device_get_match_data(dev);
	if (!ctx->pdata)
		return dev_err_probe(dev, -ENODEV,
				     "Failed to obtain platform data\n");

	if (ctx->pdata->vregs) {
		ctx->regs = devm_kcalloc(dev, ctx->pdata->num_vregs,
					 sizeof(*ctx->regs), GFP_KERNEL);
		if (!ctx->regs)
			return -ENOMEM;

		for (i = 0; i < ctx->pdata->num_vregs; i++)
			ctx->regs[i].supply = ctx->pdata->vregs[i].name;

		ret = devm_regulator_bulk_get(dev, ctx->pdata->num_vregs,
					      ctx->regs);
		if (ret < 0)
			return dev_err_probe(dev, ret,
					     "Failed to get all regulators\n");

		for (i = 0; i < ctx->pdata->num_vregs; i++) {
			if (!ctx->pdata->vregs[1].load_uA)
				continue;

			ret = regulator_set_load(ctx->regs[i].consumer,
						 ctx->pdata->vregs[i].load_uA);
			if (ret)
				return dev_err_probe(dev, ret,
						     "Failed to set vreg load\n");
		}
	}

	ctx->clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(ctx->clk))
		return dev_err_probe(dev, PTR_ERR(ctx->clk),
				     "Failed to get clock\n");

	ctx->en_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->en_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->en_gpio),
				     "Failed to get enable the GPIO\n");

	ret = pci_pwrctl_wcn7850_power_on(ctx);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to power on the device\n");

	ret = devm_add_action_or_reset(dev, devm_pci_pwrctl_wcn7850_power_off,
				       ctx);
	if (ret)
		return ret;

	ctx->pwrctl.dev = dev;

	ret = devm_pci_pwrctl_device_set_ready(dev, &ctx->pwrctl);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to register the pwrctl wrapper\n");

	return 0;
}

static const struct of_device_id pci_pwrctl_wcn7850_of_match[] = {
	{
		.compatible = "pci17cb,1107",
		.data = &pci_pwrctl_wcn7850_of_data,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, pci_pwrctl_wcn7850_of_match);

static struct platform_driver pci_pwrctl_wcn7850_driver = {
	.driver = {
		.name = "pci-pwrctl-wcn7850",
		.of_match_table = pci_pwrctl_wcn7850_of_match,
	},
	.probe = pci_pwrctl_wcn7850_probe,
};
module_platform_driver(pci_pwrctl_wcn7850_driver);

MODULE_AUTHOR("Bartosz Golaszewski <bartosz.golaszewski@linaro.org>");
MODULE_DESCRIPTION("PCI Power Sequencing module for WCN7850");
MODULE_LICENSE("GPL");
