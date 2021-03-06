LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(multirom_local_path) $(multirom_local_path)/lib
LOCAL_SRC_FILES:= \
    trampoline.c \
    devices.c \
    adb.c \

LOCAL_MODULE:= trampoline
LOCAL_MODULE_TAGS := eng

LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_UNSTRIPPED)
LOCAL_STATIC_LIBRARIES := libcutils libc libmultirom_static libbootimg
LOCAL_FORCE_STATIC_EXECUTABLE := true

ifeq ($(MR_INIT_DEVICES),)
    $(info MR_INIT_DEVICES was not defined in device files!)
endif
LOCAL_SRC_FILES += ../../../../$(MR_INIT_DEVICES)

# for adb
LOCAL_CFLAGS += -DPRODUCT_MODEL="\"$(PRODUCT_MODEL)\"" -DPRODUCT_MANUFACTURER="\"$(PRODUCT_MANUFACTURER)\""

# to find fstab
LOCAL_CFLAGS += -DTARGET_DEVICE="\"$(TARGET_DEVICE)\""

# ANCLARK MODIFIED ON 2017-4-1
# Define _GNU_SOURCE to deal with ambiguous errors
LOCAL_CFLAGS += -D_GNU_SOURCE -D__GNU_SOURCE

ifneq ($(MR_DEVICE_HOOKS),)
ifeq ($(MR_DEVICE_HOOKS_VER),)
    $(info MR_DEVICE_HOOKS is set but MR_DEVICE_HOOKS_VER is not specified!)
else
    LOCAL_CFLAGS += -DMR_DEVICE_HOOKS=$(MR_DEVICE_HOOKS_VER)
    LOCAL_SRC_FILES += ../../../../$(MR_DEVICE_HOOKS)
endif
endif

ifeq ($(MR_ENCRYPTION),true)
    LOCAL_CFLAGS += -DMR_ENCRYPTION
    LOCAL_SRC_FILES += encryption.c

    # ANCLARK MODIFIED ON 2017-4-7
    # Add keymaster support
    ifeq ($(MR_USE_KEYMASTER), true)
        LOCAL_CFLAGS += -DMR_USE_KEYMASTER
    endif

endif

# ANCLARK MODIFIED on 2017-04-21
# Allow keep keystore.default.so module for further debugging when in Android
ifeq ($(MR_DEBUG_KEEP_KEYMASTER_DEFAULT_SO), true)
    LOCAL_CFLAGS += -DMR_DEBUG_KEEP_KEYMASTER_DEFAULT_SO
endif

include $(BUILD_EXECUTABLE)
