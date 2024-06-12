/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __DT_BINDINGS_QCOM_SCMI_VENDOR_H
#define __DT_BINDINGS_QCOM_SCMI_VENDOR

/* Memory IDs */
#define QCOM_MEM_TYPE_DDR	0x0
#define QCOM_MEM_TYPE_LLCC	0x1
#define QCOM_MEM_TYPE_DDR_QOS	0x2

/*
 * QCOM_MEM_TYPE_DDR_QOS supports the following states.
 *
 * %QCOM_DDR_LEVEL_AUTO:	DDR operates with LPM enabled
 * %QCOM_DDR_LEVEL_PERF:	DDR operates with LPM disabled
 */
#define QCOM_DDR_LEVEL_AUTO	0x0
#define QCOM_DDR_LEVEL_PERF	0x1

#endif /* __DT_BINDINGS_QCOM_SCMI_VENDOR_H */
