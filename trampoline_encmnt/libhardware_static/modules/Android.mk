hardware_modules := gralloc local_time \
	power consumerir sensors vibrator input
include $(call all-named-subdir-makefiles,$(hardware_modules))
