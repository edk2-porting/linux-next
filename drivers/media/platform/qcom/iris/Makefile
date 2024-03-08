# SPDX-License-Identifier: GPL-2.0
# Makefile for Qualcomm Iris driver

# venus-core-objs += core.o helpers.o firmware.o \
# 		   hfi_venus.o hfi_msgs.o hfi_cmds.o hfi.o \
# 		   hfi_parser.o pm_helpers.o dbgfs.o \
# 		   hfi_platform.o hfi_platform_v4.o \
# 		   hfi_platform_v6.o hfi_plat_bufs_v6.o \


msm_video-objs += msm_vidc_kalama.o

msm_video-objs += msm_vidc_buffer_iris3.o \
                  msm_vidc_power_iris3.o \
                  msm_vidc_iris3.o

msm_video-objs += msm_vidc_v4l2.o \
                  msm_vidc_vb2.o \
                  msm_vidc.o \
                  msm_vdec.o \
                  msm_venc.o \
                  msm_vidc_driver.o \
                  msm_vidc_control.o \
                  msm_vidc_buffer.o \
                  msm_vidc_power.o \
                  msm_vidc_probe.o \
                  msm_vidc_dt.o \
                  msm_vidc_debug.o \
                  msm_vidc_memory.o \
                  msm_vidc_fence.o \
                  venus_hfi.o \
                  hfi_packet.o \
                  venus_hfi_response.o \
                  msm_vidc_platform.o

# venus-dec-objs += vdec.o vdec_ctrls.o
# venus-enc-objs += venc.o venc_ctrls.o

# obj-$(CONFIG_VIDEO_QCOM_VENUS) += venus-core.o
# obj-$(CONFIG_VIDEO_QCOM_VENUS) += venus-dec.o
# obj-$(CONFIG_VIDEO_QCOM_VENUS) += venus-enc.o

obj-$(CONFIG_VIDEO_QCOM_IRIS) += msm_video.o
