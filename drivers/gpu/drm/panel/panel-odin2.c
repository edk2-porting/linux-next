// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2023 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct sim {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline struct sim *to_sim(struct drm_panel *panel)
{
	return container_of(panel, struct sim, panel);
}

static void sim_reset(struct sim *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int sim_on(struct sim *ctx)
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

static int sim_off(struct sim *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	// ret = mipi_dsi_dcs_set_display_off(dsi);
	// if (ret < 0) {
	// 	dev_err(dev, "Failed to set display off: %d\n", ret);
	// 	return ret;
	// }
	// msleep(50);

	// ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	// if (ret < 0) {
	// 	dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
	// 	return ret;
	// }
	msleep(120);

	return 0;
}

static int sim_prepare(struct drm_panel *panel)
{
	struct sim *ctx = to_sim(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	sim_reset(ctx);

	ret = sim_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int sim_unprepare(struct drm_panel *panel)
{
	struct sim *ctx = to_sim(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = sim_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode sim_mode = {
	.clock = (1080 + 93 + 1 + 47) * (1920 + 40 + 2 + 60) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 93,
	.hsync_end = 1080 + 93 + 1,
	.htotal = 1080 + 93 + 1 + 47,
	.vdisplay = 1920,
	.vsync_start = 1920 + 40,
	.vsync_end = 1920 + 40 + 2,
	.vtotal = 1920 + 40 + 2 + 60,
	.width_mm = 0,
	.height_mm = 0,
};

static int sim_get_modes(struct drm_panel *panel,
			 struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &sim_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs sim_panel_funcs = {
	.prepare = sim_prepare,
	.unprepare = sim_unprepare,
	.get_modes = sim_get_modes,
};

static int sim_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct sim *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &sim_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		dev_err(dev, "Failed to get backlight: %d\n", ret);
		// return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void sim_remove(struct mipi_dsi_device *dsi)
{
	struct sim *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id sim_of_match[] = {
	{ .compatible = "odin,panel" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sim_of_match);

static struct mipi_dsi_driver sim_driver = {
	.probe = sim_probe,
	.remove = sim_remove,
	.driver = {
		.name = "panel-sim",
		.of_match_table = sim_of_match,
	},
};
module_mipi_dsi_driver(sim_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for nt35532 video mode dsi panel without DSC");
MODULE_LICENSE("GPL");