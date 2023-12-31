/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * X1E80100 interconnect IDs
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2023, Linaro Limited
 */

#ifndef __DRIVERS_INTERCONNECT_QCOM_X1E80100_H
#define __DRIVERS_INTERCONNECT_QCOM_X1E80100_H

#define X1E80100_MASTER_A1NOC_SNOC			0
#define X1E80100_MASTER_A2NOC_SNOC			1
#define X1E80100_MASTER_ANOC_PCIE_GEM_NOC		2
#define X1E80100_MASTER_ANOC_PCIE_GEM_NOC_DISP		3
#define X1E80100_MASTER_APPSS_PROC			4
#define X1E80100_MASTER_CAMNOC_HF			5
#define X1E80100_MASTER_CAMNOC_ICP			6
#define X1E80100_MASTER_CAMNOC_SF			7
#define X1E80100_MASTER_CDSP_PROC			8
#define X1E80100_MASTER_CNOC_CFG			9
#define X1E80100_MASTER_CNOC_MNOC_CFG			10
#define X1E80100_MASTER_COMPUTE_NOC			11
#define X1E80100_MASTER_CRYPTO				12
#define X1E80100_MASTER_GEM_NOC_CNOC			13
#define X1E80100_MASTER_GEM_NOC_PCIE_SNOC		14
#define X1E80100_MASTER_GFX3D				15
#define X1E80100_MASTER_GPU_TCU				16
#define X1E80100_MASTER_IPA				17
#define X1E80100_MASTER_LLCC				18
#define X1E80100_MASTER_LLCC_DISP			19
#define X1E80100_MASTER_LPASS_GEM_NOC			20
#define X1E80100_MASTER_LPASS_LPINOC			21
#define X1E80100_MASTER_LPASS_PROC			22
#define X1E80100_MASTER_LPIAON_NOC			23
#define X1E80100_MASTER_MDP				24
#define X1E80100_MASTER_MDP_DISP			25
#define X1E80100_MASTER_MNOC_HF_MEM_NOC			26
#define X1E80100_MASTER_MNOC_HF_MEM_NOC_DISP		27
#define X1E80100_MASTER_MNOC_SF_MEM_NOC			28
#define X1E80100_MASTER_PCIE_0				29
#define X1E80100_MASTER_PCIE_1				30
#define X1E80100_MASTER_QDSS_ETR			31
#define X1E80100_MASTER_QDSS_ETR_1			32
#define X1E80100_MASTER_QSPI_0				33
#define X1E80100_MASTER_QUP_0				34
#define X1E80100_MASTER_QUP_1				35
#define X1E80100_MASTER_QUP_2				36
#define X1E80100_MASTER_QUP_CORE_0			37
#define X1E80100_MASTER_QUP_CORE_1			38
#define X1E80100_MASTER_SDCC_2				39
#define X1E80100_MASTER_SDCC_4				40
#define X1E80100_MASTER_SNOC_SF_MEM_NOC			41
#define X1E80100_MASTER_SP				42
#define X1E80100_MASTER_SYS_TCU				43
#define X1E80100_MASTER_UFS_MEM				44
#define X1E80100_MASTER_USB3_0				45
#define X1E80100_MASTER_VIDEO				46
#define X1E80100_MASTER_VIDEO_CV_PROC			47
#define X1E80100_MASTER_VIDEO_V_PROC			48
#define X1E80100_SLAVE_A1NOC_SNOC			49
#define X1E80100_SLAVE_A2NOC_SNOC			50
#define X1E80100_SLAVE_AHB2PHY_NORTH			51
#define X1E80100_SLAVE_AHB2PHY_SOUTH			52
#define X1E80100_SLAVE_ANOC_PCIE_GEM_NOC		53
#define X1E80100_SLAVE_AOSS				54
#define X1E80100_SLAVE_APPSS				55
#define X1E80100_SLAVE_BOOT_IMEM			56
#define X1E80100_SLAVE_CAMERA_CFG			57
#define X1E80100_SLAVE_CDSP_MEM_NOC			58
#define X1E80100_SLAVE_CLK_CTL				59
#define X1E80100_SLAVE_CNOC_CFG				60
#define X1E80100_SLAVE_CNOC_MNOC_CFG			61
#define X1E80100_SLAVE_CRYPTO_0_CFG			62
#define X1E80100_SLAVE_DISPLAY_CFG			63
#define X1E80100_SLAVE_EBI1				64
#define X1E80100_SLAVE_EBI1_DISP			65
#define X1E80100_SLAVE_GEM_NOC_CNOC			66
#define X1E80100_SLAVE_GFX3D_CFG			67
#define X1E80100_SLAVE_IMEM				68
#define X1E80100_SLAVE_IMEM_CFG				69
#define X1E80100_SLAVE_IPC_ROUTER_CFG			70
#define X1E80100_SLAVE_LLCC				71
#define X1E80100_SLAVE_LLCC_DISP			72
#define X1E80100_SLAVE_LPASS_GEM_NOC			73
#define X1E80100_SLAVE_LPASS_QTB_CFG			74
#define X1E80100_SLAVE_LPIAON_NOC_LPASS_AG_NOC		75
#define X1E80100_SLAVE_LPICX_NOC_LPIAON_NOC		76
#define X1E80100_SLAVE_MEM_NOC_PCIE_SNOC		77
#define X1E80100_SLAVE_MNOC_HF_MEM_NOC			78
#define X1E80100_SLAVE_MNOC_HF_MEM_NOC_DISP		79
#define X1E80100_SLAVE_MNOC_SF_MEM_NOC			80
#define X1E80100_SLAVE_NSP_QTB_CFG			81
#define X1E80100_SLAVE_PCIE_0				82
#define X1E80100_SLAVE_PCIE_0_CFG			83
#define X1E80100_SLAVE_PCIE_1				84
#define X1E80100_SLAVE_PCIE_1_CFG			85
#define X1E80100_SLAVE_PDM				86
#define X1E80100_SLAVE_PRNG				87
#define X1E80100_SLAVE_QDSS_CFG				88
#define X1E80100_SLAVE_QDSS_STM				89
#define X1E80100_SLAVE_QSPI_0				90
#define X1E80100_SLAVE_QUP_1				91
#define X1E80100_SLAVE_QUP_2				92
#define X1E80100_SLAVE_QUP_CORE_0			93
#define X1E80100_SLAVE_QUP_CORE_1			94
#define X1E80100_SLAVE_QUP_CORE_2			95
#define X1E80100_SLAVE_SDCC_2				96
#define X1E80100_SLAVE_SDCC_4				97
#define X1E80100_SLAVE_SERVICE_MNOC			98
#define X1E80100_SLAVE_SNOC_GEM_NOC_SF			99
#define X1E80100_SLAVE_TCSR				100
#define X1E80100_SLAVE_TCU				101
#define X1E80100_SLAVE_TLMM				102
#define X1E80100_SLAVE_TME_CFG				103
#define X1E80100_SLAVE_UFS_MEM_CFG			104
#define X1E80100_SLAVE_USB3_0				105
#define X1E80100_SLAVE_VENUS_CFG			106
#define X1E80100_MASTER_DDR_PERF_MODE			107
#define X1E80100_MASTER_QUP_CORE_2			108
#define X1E80100_MASTER_PCIE_TCU			109
#define X1E80100_MASTER_GIC2				110
#define X1E80100_MASTER_AV1_ENC				111
#define X1E80100_MASTER_EVA				112
#define X1E80100_MASTER_PCIE_NORTH			113
#define X1E80100_MASTER_PCIE_SOUTH			114
#define X1E80100_MASTER_PCIE_3				115
#define X1E80100_MASTER_PCIE_4				116
#define X1E80100_MASTER_PCIE_5				117
#define X1E80100_MASTER_PCIE_2				118
#define X1E80100_MASTER_PCIE_6A				119
#define X1E80100_MASTER_PCIE_6B				120
#define X1E80100_MASTER_GIC1				121
#define X1E80100_MASTER_USB_NOC_SNOC			122
#define X1E80100_MASTER_AGGRE_USB_NORTH			123
#define X1E80100_MASTER_AGGRE_USB_SOUTH			124
#define X1E80100_MASTER_USB2				125
#define X1E80100_MASTER_USB3_MP				126
#define X1E80100_MASTER_USB3_1				127
#define X1E80100_MASTER_USB3_2				128
#define X1E80100_MASTER_USB4_0				129
#define X1E80100_MASTER_USB4_1				130
#define X1E80100_MASTER_USB4_2				131
#define X1E80100_MASTER_ANOC_PCIE_GEM_NOC_PCIE		132
#define X1E80100_MASTER_LLCC_PCIE			133
#define X1E80100_MASTER_PCIE_NORTH_PCIE			134
#define X1E80100_MASTER_PCIE_SOUTH_PCIE			135
#define X1E80100_MASTER_PCIE_3_PCIE			136
#define X1E80100_MASTER_PCIE_4_PCIE			137
#define X1E80100_MASTER_PCIE_5_PCIE			138
#define X1E80100_MASTER_PCIE_0_PCIE			139
#define X1E80100_MASTER_PCIE_1_PCIE			140
#define X1E80100_MASTER_PCIE_2_PCIE			141
#define X1E80100_MASTER_PCIE_6A_PCIE			142
#define X1E80100_MASTER_PCIE_6B_PCIE			143
#define X1E80100_SLAVE_AHB2PHY_2			144
#define X1E80100_SLAVE_AV1_ENC_CFG			145
#define X1E80100_SLAVE_PCIE_2_CFG			146
#define X1E80100_SLAVE_PCIE_3_CFG			147
#define X1E80100_SLAVE_PCIE_4_CFG			148
#define X1E80100_SLAVE_PCIE_5_CFG			149
#define X1E80100_SLAVE_PCIE_6A_CFG			150
#define X1E80100_SLAVE_PCIE_6B_CFG			151
#define X1E80100_SLAVE_PCIE_RSC_CFG			152
#define X1E80100_SLAVE_QUP_0				153
#define X1E80100_SLAVE_SMMUV3_CFG			154
#define X1E80100_SLAVE_USB2				155
#define X1E80100_SLAVE_USB3_1				156
#define X1E80100_SLAVE_USB3_2				157
#define X1E80100_SLAVE_USB3_MP				158
#define X1E80100_SLAVE_USB4_0				159
#define X1E80100_SLAVE_USB4_1				160
#define X1E80100_SLAVE_USB4_2				161
#define X1E80100_SLAVE_PCIE_2				162
#define X1E80100_SLAVE_PCIE_3				163
#define X1E80100_SLAVE_PCIE_4				164
#define X1E80100_SLAVE_PCIE_5				165
#define X1E80100_SLAVE_PCIE_6A				166
#define X1E80100_SLAVE_PCIE_6B				167
#define X1E80100_SLAVE_DDR_PERF_MODE			168
#define X1E80100_SLAVE_PCIE_NORTH			169
#define X1E80100_SLAVE_PCIE_SOUTH			170
#define X1E80100_SLAVE_USB_NOC_SNOC			171
#define X1E80100_SLAVE_AGGRE_USB_NORTH			172
#define X1E80100_SLAVE_AGGRE_USB_SOUTH			173
#define X1E80100_SLAVE_LLCC_PCIE			174
#define X1E80100_SLAVE_EBI1_PCIE			175
#define X1E80100_SLAVE_ANOC_PCIE_GEM_NOC_PCIE		176
#define X1E80100_SLAVE_PCIE_NORTH_PCIE			177
#define X1E80100_SLAVE_PCIE_SOUTH_PCIE			178

#endif
