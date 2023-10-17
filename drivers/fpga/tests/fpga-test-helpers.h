/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KUnit test for the FPGA Manager
 *
 * Copyright (C) 2023 Red Hat, Inc.
 *
 * Author: Marco Pagani <marpagan@redhat.com>
 */

#ifndef _FPGA_KUNIT_HELPERS_H
#define _FPGA_KUNIT_HELPERS_H

#define TEST_PDEV_NAME	"fpga-test-pdev"

#define TEST_PLATFORM_DRIVER(__drv_name)			\
	__TEST_PLATFORM_DRIVER(__drv_name, TEST_PDEV_NAME)
/*
 * Helper macro for defining a minimal platform driver that can
 * be registered to support the parent platform devices used for
 * testing.
 */
#define __TEST_PLATFORM_DRIVER(__drv_name, __dev_name)		\
static struct platform_driver __drv_name = {			\
	.driver = {						\
		.name = __dev_name,				\
	},							\
}

#endif	/* _FPGA_KUNIT_HELPERS_H */
