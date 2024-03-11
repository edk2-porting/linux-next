/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Qualcomm Protection Domain mapper
 *
 * Copyright (c) 2023 Linaro Ltd.
 */
#ifndef __QCOM_PD_MAPPER__
#define __QCOM_PD_MAPPER__

struct qcom_pdm_domain_data {
	const char *domain;
	u32 instance_id;
	/* NULL-terminated array */
	const char * services[];
};

#if IS_ENABLED(CONFIG_QCOM_PD_MAPPER)

int qcom_pdm_add_domains(const struct qcom_pdm_domain_data * const *data,
			 size_t num_data);
void qcom_pdm_del_domains(const struct qcom_pdm_domain_data * const *data,
			  size_t num_data);

#else

static inline int qcom_pdm_add_domains(const struct qcom_pdm_domain_data * const *data,
				      size_t num_data)
{
	return 0;
}

static inline void qcom_pdm_del_domains(const struct qcom_pdm_domain_data * const *data,
					size_t num_data)
{
}

#endif

#endif
