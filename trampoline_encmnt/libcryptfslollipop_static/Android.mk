LOCAL_PATH := $(call my-dir)

# ANCLARK MODIFIED ON 2017-4-3
# A specified static-linked libcryptfslollipop for MultiRom Trampoline_encmnt.

ifneq ($(wildcard bootable/recovery/crypto/lollipop/cryptfs.h),)
    mr_twrp_path := bootable/recovery
else ifneq ($(wildcard bootable/recovery-twrp/crypto/lollipop/cryptfs.h),)
    mr_twrp_path := bootable/recovery-twrp
else
    $(error Failed to find path to TWRP, which is required to build MultiROM with encryption support)
endif


include $(CLEAR_VARS)

LOCAL_MODULE := libcryptfslollipop_static
LOCAL_MODULE_TAGS := eng optional
LOCAL_CFLAGS :=
LOCAL_SRC_FILES = cryptfs.c
LOCAL_STATIC_LIBRARIES := libcrypto_static libhardware_static libcutils
LOCAL_C_INCLUDES := external/openssl/include $(mr_twrp_path)/crypto/scrypt/lib/crypto

ifeq ($(TARGET_HW_DISK_ENCRYPTION),true)
    ifeq ($(TARGET_CRYPTFS_HW_PATH),)
        LOCAL_C_INCLUDES += device/qcom/common/cryptfs_hw
    else
        LOCAL_C_INCLUDES += $(TARGET_CRYPTFS_HW_PATH)
    endif
    LOCAL_WHOLE_STATIC_LIBRARIES += libcryptfs_hw_static
    LOCAL_CFLAGS += -DCONFIG_HW_DISK_ENCRYPTION
endif

ifneq ($(wildcard hardware/libhardware/include/hardware/keymaster0.h),)
    LOCAL_CFLAGS += -DTW_CRYPTO_HAVE_KEYMASTERX
    LOCAL_C_INCLUDES +=  external/boringssl/src/include
endif

LOCAL_WHOLE_STATIC_LIBRARIES += libscrypttwrp_static

include $(BUILD_STATIC_LIBRARY)

