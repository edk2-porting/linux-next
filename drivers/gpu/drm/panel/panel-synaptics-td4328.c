// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree.
 * Copyright (c) 2024 Xilin Wu <wuxilin123@gmail.com>
 * Copyright (c) 2024 Junhao Xie <bigfoot@classfun.cn>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct td4328 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
};

static inline struct td4328 *to_td4328(struct drm_panel *panel)
{
	return container_of(panel, struct td4328, panel);
}

static void td4328_reset(struct td4328 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int td4328_on(struct td4328 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xb3, 0x31);
	mipi_dsi_dcs_write_seq(dsi, 0xd6, 0x00);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(70);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	return 0;
}

static int td4328_off(struct td4328 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(50);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	return 0;
}

static int td4328_prepare(struct drm_panel *panel)
{
	struct td4328 *ctx = to_td4328(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	td4328_reset(ctx);

	ret = td4328_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
		return ret;
	}

	return 0;
}

static int td4328_unprepare(struct drm_panel *panel)
{
	struct td4328 *ctx = to_td4328(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = td4328_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);

	return 0;
}

static const struct drm_display_mode td4328_mode = {
	.clock = (1080 + 93 + 1 + 47) * (1920 + 40 + 2 + 60) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 93,
	.hsync_end = 1080 + 93 + 1,
	.htotal = 1080 + 93 + 1 + 47,
	.vdisplay = 1920,
	.vsync_start = 1920 + 40,
	.vsync_end = 1920 + 40 + 2,
	.vtotal = 1920 + 40 + 2 + 60,
	.width_mm = 75,
	.height_mm = 133,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int td4328_get_modes(struct drm_panel *panel,
			 struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &td4328_mode);
}

static enum drm_panel_orientation td4328_get_orientation(struct drm_panel *panel)
{
	return DRM_MODE_PANEL_ORIENTATION_RIGHT_UP;
}

static const struct drm_panel_funcs td4328_panel_funcs = {
	.prepare = td4328_prepare,
	.disable = td4328_unprepare,
	.get_modes = td4328_get_modes,
	.get_orientation = td4328_get_orientation,
};

static int td4328_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct td4328 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supplies[0].supply = "vddio";
	ctx->supplies[1].supply = "vdd";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &td4328_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void td4328_remove(struct mipi_dsi_device *dsi)
{
	struct td4328 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id td4328_of_match[] = {
	{ .compatible = "syna,td4328" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, td4328_of_match);

static struct mipi_dsi_driver td4328_driver = {
	.probe = td4328_probe,
	.remove = td4328_remove,
	.driver = {
		.name = "panel-td4328",
		.of_match_table = td4328_of_match,
	},
};
module_mipi_dsi_driver(td4328_driver);

MODULE_DESCRIPTION("DRM driver for td4328-equipped DSI panels");
MODULE_LICENSE("GPL");
