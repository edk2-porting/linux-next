// SPDX-License-Identifier: GPL-2.0
#include <opencsd/c_api/opencsd_c_api.h>

/*
 * Check OpenCSD library version is sufficient to provide required features
 */
#define OCSD_MIN_VER ((1 << 16) | (2 << 8) | (1))
#if !defined(OCSD_VER_NUM) || (OCSD_VER_NUM < OCSD_MIN_VER)
#error "OpenCSD >= 1.2.1 is required"
#endif

int main(void)
{
	(void)ocsd_get_version();
	return 0;
}
