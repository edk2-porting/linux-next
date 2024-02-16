// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Linaro Ltd.
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/jiffies.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pwrseq/provider.h>
#include <linux/string.h>
#include <linux/types.h>

struct pwrseq_qca6390_vreg {
	const char *name;
	unsigned int load_uA;
};

struct pwrseq_qca6390_pdata {
	const struct pwrseq_qca6390_vreg *vregs_common;
	size_t num_vregs_common;
	const struct pwrseq_qca6390_vreg *vregs_wlan;
	size_t num_vregs_wlan;
	unsigned int pwup_delay_msec;
};

struct pwrseq_qca6390_ctx {
	struct pwrseq_device *pwrseq;
	struct device_node *of_node;
	const struct pwrseq_qca6390_pdata *pdata;
	struct regulator_bulk_data *regs_common;
	struct regulator_bulk_data *regs_wlan;
	struct gpio_desc *bt_gpio;
	struct gpio_desc *wlan_gpio;
	unsigned long last_gpio_enable;
};

static const struct pwrseq_qca6390_vreg pwrseq_qca6390_vregs_common[] = {
	{
		.name = "vddio",
		.load_uA = 20000,
	},
	{
		.name = "vddaon",
		.load_uA = 100000,
	},
	{
		.name = "vddpmu",
		.load_uA = 1250000,
	},
	{
		.name = "vddrfa0p95",
		.load_uA = 200000,
	},
	{
		.name = "vddrfa1p3",
		.load_uA = 400000,
	},
	{
		.name = "vddrfa1p9",
		.load_uA = 400000,
	},
};

static const struct pwrseq_qca6390_vreg pwrseq_qca6390_vregs_wlan[] = {
	{
		.name = "vddpcie1p3",
		.load_uA = 35000,
	},
	{
		.name = "vddpcie1p9",
		.load_uA = 15000,
	},
};

static void pwrseq_qca6390_ensure_gpio_delay(struct pwrseq_qca6390_ctx *ctx)
{
	unsigned long diff_jiffies = jiffies - ctx->last_gpio_enable;
	unsigned int diff_msecs = jiffies_to_msecs(diff_jiffies);

	if (diff_msecs < 100)
		msleep(100 - diff_msecs);
}

static const struct pwrseq_qca6390_pdata pwrseq_qca6390_of_data = {
	.vregs_common = pwrseq_qca6390_vregs_common,
	.num_vregs_common = ARRAY_SIZE(pwrseq_qca6390_vregs_common),
	.vregs_wlan = pwrseq_qca6390_vregs_wlan,
	.num_vregs_wlan = ARRAY_SIZE(pwrseq_qca6390_vregs_wlan),
	.pwup_delay_msec = 16,
};

static int pwrseq_qca6390_vregs_enable(struct pwrseq_device *pwrseq)
{
	struct pwrseq_qca6390_ctx *ctx = pwrseq_device_get_drvdata(pwrseq);

	return regulator_bulk_enable(ctx->pdata->num_vregs_common,
				     ctx->regs_common);
}

static int pwrseq_qca6390_vregs_disable(struct pwrseq_device *pwrseq)
{
	struct pwrseq_qca6390_ctx *ctx = pwrseq_device_get_drvdata(pwrseq);

	return regulator_bulk_disable(ctx->pdata->num_vregs_common,
				      ctx->regs_common);
}

static const struct pwrseq_unit_data pwrseq_qca6390_vregs_unit_data = {
	.name = "regulators-enable",
	.enable = pwrseq_qca6390_vregs_enable,
	.disable = pwrseq_qca6390_vregs_disable,
};

static const struct pwrseq_unit_data *pwrseq_qca6390_unit_deps[] = {
	&pwrseq_qca6390_vregs_unit_data,
	NULL
};

static int pwrseq_qca6390_bt_enable(struct pwrseq_device *pwrseq)
{
	struct pwrseq_qca6390_ctx *ctx = pwrseq_device_get_drvdata(pwrseq);

	pwrseq_qca6390_ensure_gpio_delay(ctx);
	gpiod_set_value_cansleep(ctx->bt_gpio, 1);
	ctx->last_gpio_enable = jiffies;

	return 0;
}

static int pwrseq_qca6390_bt_disable(struct pwrseq_device *pwrseq)
{
	struct pwrseq_qca6390_ctx *ctx = pwrseq_device_get_drvdata(pwrseq);

	gpiod_set_value_cansleep(ctx->bt_gpio, 0);

	return 0;
}

static const struct pwrseq_unit_data pwrseq_qca6390_bt_unit_data = {
	.name = "bluetooth-enable",
	.deps = pwrseq_qca6390_unit_deps,
	.enable = pwrseq_qca6390_bt_enable,
	.disable = pwrseq_qca6390_bt_disable,
};

static int pwrseq_qca6390_wlan_enable(struct pwrseq_device *pwrseq)
{
	struct pwrseq_qca6390_ctx *ctx = pwrseq_device_get_drvdata(pwrseq);
	int ret;

	ret = regulator_bulk_enable(ctx->pdata->num_vregs_wlan, ctx->regs_wlan);
	if (ret)
		return ret;

	pwrseq_qca6390_ensure_gpio_delay(ctx);
	gpiod_set_value_cansleep(ctx->wlan_gpio, 1);
	ctx->last_gpio_enable = jiffies;

	return 0;
}

static int pwrseq_qca6390_wlan_disable(struct pwrseq_device *pwrseq)
{
	struct pwrseq_qca6390_ctx *ctx = pwrseq_device_get_drvdata(pwrseq);

	gpiod_set_value_cansleep(ctx->wlan_gpio, 0);

	return regulator_bulk_disable(ctx->pdata->num_vregs_wlan,
				      ctx->regs_wlan);
}

static const struct pwrseq_unit_data pwrseq_qca6390_wlan_unit_data = {
	.name = "wlan-enable",
	.deps = pwrseq_qca6390_unit_deps,
	.enable = pwrseq_qca6390_wlan_enable,
	.disable = pwrseq_qca6390_wlan_disable,
};

static int pwrseq_qca6390_pwup_delay(struct pwrseq_device *pwrseq)
{
	struct pwrseq_qca6390_ctx *ctx = pwrseq_device_get_drvdata(pwrseq);

	if (ctx->pdata->pwup_delay_msec)
		msleep(ctx->pdata->pwup_delay_msec);

	return 0;
}

static const struct pwrseq_target_data pwrseq_qca6390_bt_target_data = {
	.name = "bluetooth",
	.unit = &pwrseq_qca6390_bt_unit_data,
	.post_enable = pwrseq_qca6390_pwup_delay,
};

static const struct pwrseq_target_data pwrseq_qca6390_wlan_target_data = {
	.name = "wlan",
	.unit = &pwrseq_qca6390_wlan_unit_data,
	.post_enable = pwrseq_qca6390_pwup_delay,
};

static const struct pwrseq_target_data *pwrseq_qca6390_targets[] = {
	&pwrseq_qca6390_bt_target_data,
	&pwrseq_qca6390_wlan_target_data,
	NULL
};

static int pwrseq_qca6390_match(struct pwrseq_device *pwrseq,
				struct device *dev)
{
	struct pwrseq_qca6390_ctx *ctx = pwrseq_device_get_drvdata(pwrseq);
	struct device_node *dev_node = dev->of_node;

	/*
	 * The PMU supplies power to the Bluetooth and WLAN modules. both
	 * consume the PMU AON output so check the presence of the
	 * 'vddaon-supply' property and whether it leads us to the right
	 * device.
	 */
	if (!of_property_present(dev_node, "vddaon-supply"))
		return 0;

	struct device_node *reg_node __free(device_node) =
			of_parse_phandle(dev_node, "vddaon-supply", 0);
	if (!reg_node)
		return 0;

	/*
	 * `reg_node` is the PMU AON regulator, its parent is the `regulators`
	 * node and finally its grandparent is the PMU device node that we're
	 * looking for.
	 */
	if (!reg_node->parent || !reg_node->parent->parent ||
	    reg_node->parent->parent != ctx->of_node)
		return 0;

	return 1;
}

static struct regulator_bulk_data *
pwrseq_qca6390_get_regs(struct device *dev, size_t num_regs,
			const struct pwrseq_qca6390_vreg *pdata)
{
	struct regulator_bulk_data *regs;
	int ret, i;

	regs = devm_kcalloc(dev, num_regs, sizeof(*regs), GFP_KERNEL);
	if (!regs)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num_regs; i++)
		regs[i].supply = pdata[i].name;

	ret = devm_regulator_bulk_get(dev, num_regs, regs);
	if (ret < 0)
		return ERR_PTR(ret);

	for (i = 0; i < num_regs; i++) {
		if (!pdata[i].load_uA)
			continue;

		ret = regulator_set_load(regs[i].consumer, pdata[i].load_uA);
		if (ret)
			return ERR_PTR(ret);
	}

	return regs;
}

static int pwrseq_qca6390_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pwrseq_qca6390_ctx *ctx;
	struct pwrseq_config config;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->of_node = dev->of_node;

	ctx->pdata = of_device_get_match_data(dev);
	if (!ctx->pdata)
		return dev_err_probe(dev, -ENODEV,
				     "Failed to obtain platform data\n");

	ctx->regs_common = pwrseq_qca6390_get_regs(dev,
						   ctx->pdata->num_vregs_common,
						   ctx->pdata->vregs_common);
	if (IS_ERR(ctx->regs_common))
		return dev_err_probe(dev, PTR_ERR(ctx->regs_common),
				     "Failed to get all regulators\n");

	ctx->regs_wlan = pwrseq_qca6390_get_regs(dev,
						 ctx->pdata->num_vregs_wlan,
						 ctx->pdata->vregs_wlan);
	if (IS_ERR(ctx->regs_wlan))
		return dev_err_probe(dev, PTR_ERR(ctx->regs_wlan),
				     "Failed to get all regulators\n");

	ctx->bt_gpio = devm_gpiod_get_optional(dev, "bt-enable", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->bt_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->bt_gpio),
				     "Failed to get the Bluetooth enable GPIO\n");

	ctx->wlan_gpio = devm_gpiod_get_optional(dev, "wlan-enable",
						 GPIOD_OUT_LOW);
	if (IS_ERR(ctx->wlan_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->wlan_gpio),
				     "Failed to get the WLAN enable GPIO\n");

	memset(&config, 0, sizeof(config));

	config.parent = dev;
	config.owner = THIS_MODULE;
	config.drvdata = ctx;
	config.match = pwrseq_qca6390_match;
	config.targets = pwrseq_qca6390_targets;

	ctx->pwrseq = devm_pwrseq_device_register(dev, &config);
	if (IS_ERR(ctx->pwrseq))
		return dev_err_probe(dev, PTR_ERR(ctx->pwrseq),
				     "Failed to register the power sequencer\n");

	return 0;
}

static const struct of_device_id pwrseq_qca6390_of_match[] = {
	{
		.compatible = "qcom,qca6390-pmu",
		.data = &pwrseq_qca6390_of_data,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, pwrseq_qca6390_of_match);

static struct platform_driver pwrseq_qca6390_driver = {
	.driver = {
		.name = "pwrseq-qca6390",
		.of_match_table = pwrseq_qca6390_of_match,
	},
	.probe = pwrseq_qca6390_probe,
};
module_platform_driver(pwrseq_qca6390_driver);

MODULE_AUTHOR("Bartosz Golaszewski <bartosz.golaszewski@linaro.org>");
MODULE_DESCRIPTION("QCA6390 PMU power sequencing driver");
MODULE_LICENSE("GPL");
