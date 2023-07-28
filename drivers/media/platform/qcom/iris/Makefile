KBUILD_OPTIONS+= VIDEO_ROOT=$(KERNEL_SRC)/$(M)

VIDEO_COMPILE_TIME = $(shell date)
VIDEO_COMPILE_BY = $(shell whoami | sed 's/\\/\\\\/')
VIDEO_COMPILE_HOST = $(shell uname -n)
VIDEO_GEN_PATH = $(srctree)/$(src)/vidc/inc/video_generated_h

$(shell echo '#define VIDEO_COMPILE_TIME "$(VIDEO_COMPILE_TIME)"' > $(VIDEO_GEN_PATH))
$(shell echo '#define VIDEO_COMPILE_BY "$(VIDEO_COMPILE_BY)"' >> $(VIDEO_GEN_PATH))
$(shell echo '#define VIDEO_COMPILE_HOST "$(VIDEO_COMPILE_HOST)"' >> $(VIDEO_GEN_PATH))

iris-objs += vidc/src/msm_vidc_v4l2.o \
                  vidc/src/msm_vidc_vb2.o \
                  vidc/src/msm_vidc.o \
                  vidc/src/msm_vdec.o \
                  vidc/src/msm_venc.o \
                  vidc/src/msm_vidc_driver.o \
                  vidc/src/msm_vidc_control.o \
                  vidc/src/msm_vidc_buffer.o \
                  vidc/src/msm_vidc_power.o \
                  vidc/src/msm_vidc_probe.o \
                  vidc/src/resources.o \
                  vidc/src/firmware.o \
                  vidc/src/msm_vidc_debug.o \
                  vidc/src/msm_vidc_memory.o \
                  vidc/src/venus_hfi.o \
                  vidc/src/venus_hfi_queue.o \
                  vidc/src/hfi_packet.o \
                  vidc/src/venus_hfi_response.o \
                  vidc/src/msm_vidc_state.o \
                  platform/common/src/msm_vidc_platform.o \
                  platform/sm8550/src/msm_vidc_sm8550.o \
                  variant/common/src/msm_vidc_variant.o \
                  variant/iris3/src/msm_vidc_buffer_iris3.o \
                  variant/iris3/src/msm_vidc_iris3.o \
                  variant/iris3/src/msm_vidc_power_iris3.o \
                  variant/iris3/src/msm_vidc_bus_iris3.o \
                  variant/iris3/src/msm_vidc_clock_iris3.o

obj-$(CONFIG_VIDEO_QCOM_IRIS) += iris.o

ccflags-y += -I$(srctree)/$(src)/vidc/inc
ccflags-y += -I$(srctree)/$(src)/platform/common/inc
ccflags-y += -I$(srctree)/$(src)/platform/sm8550/inc
ccflags-y += -I$(srctree)/$(src)/variant/common/inc
ccflags-y += -I$(srctree)/$(src)/variant/iris3/inc
