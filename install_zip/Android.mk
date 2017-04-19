LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

install_zip_path := $(multirom_local_path)/install_zip

MULTIROM_ZIP_TARGET := $(PRODUCT_OUT)/multirom
MULTIROM_INST_DIR := $(PRODUCT_OUT)/multirom_installer
multirom_binary := $(TARGET_ROOT_OUT)/multirom
trampoline_binary := $(TARGET_ROOT_OUT)/trampoline

ifeq ($(MR_FSTAB),)
    $(info MR_FSTAB not defined in device files)
endif

multirom_extra_dep :=
ifeq ($(MR_ENCRYPTION),true)
	multirom_extra_dep += trampoline_encmnt linker64 linker
	
	# ANCLARK MODIFIED on 2017-4-4
	# Dependency file list for trampoline_encmnt.
	# NOTICE: Qualcomm devices have their own essential vendor-lib: libcryptfs_hw. This file links to a number of shared libraries.	
	TRAMPOLINE_ENCMNT_DEPENDENCIES := \
		libbacktrace.so libbase.so libcryptfslollipop.so \
		libcrypto.so libc.so libc++.so libcutils.so libdl.so \
		libhardware.so liblog.so libm.so libunwind.so libutils.so
	TRAMPOLINE_ENCMNT_DEPENDENCIES_VENDOR := libcryptfs_hw.so

else
	MR_ENCRYPTION := false
endif

MR_DEVICES := $(TARGET_DEVICE)
ifneq ($(MR_DEVICE_VARIANTS),)
	MR_DEVICES += $(MR_DEVICE_VARIANTS)
endif

# ANCLARK MODIFIED on 2017-4-7
# If keymaster switch is turned on, it will be compulsory, or encryption will not work properly.
ifeq ($(MR_ENCRYPTION), true)
    ifeq ($(MR_USE_KEYMASTER), true)
        ifndef MR_KEYMASTER_LIB_PATH
            $(info *******************************[ E R R O R ! ]*************************************)
            $(info You must specify the path to keymaster lib in MR_KEYMASTER_LIB_PATH, or encryption won't work.)
            $(info **********************************************************************************)
            $(error stop)
        endif
    endif
endif

# ANCLARK MODIFIED on 2017-4-4
# Qualcomm devices require their own fstab format (fstab.qcom) to work with encryption. 
# However, this format is not supported by extract_boot_dev.sh. So an exception will occur.
# To resume making multirom zip, we must prepare another fstab to feed its appetite.
ifeq ($(MR_USE_QCOM_SPECIFIED_FSTAB), true)
    $(info =================================================================================================================)
    $(info -                                         N O T I C E !)
    $(info -----------------------------------------------------------------------------------------------------------------)
    $(info This device uses Qualcomm specified fstab file.)
    $(info ------ You can know its syntax by reading /fstab.qcom on your device.)
    $(info =================================================================================================================)
    ifndef MR_FSTAB_FOR_EXTRACTING_BOOTDEV
        $(info *******************************[ E R R O R ! ]*************************************)
        $(info You must specify a standard fstab which Multirom can recognize to generate bootdev info-file. 
        $(info Set it through var MR_FSTAB_FOR_EXTRACTING_BOOTDEV.)
        $(info **********************************************************************************)
        $(error stop)
    else
        MR_FSTAB_FOR_EXTRACTING_BOOTDEV := $(MR_FSTAB_FOR_EXTRACTING_BOOTDEV)
    endif
else
    $(info =================================================================================================================)
    $(info -                                         N O T I C E !)
    $(info -----------------------------------------------------------------------------------------------------------------)
    $(info   Now Multirom will directly use mrom.fstab you specified.)
    $(info   If you use a Qualcomm device with encryption, you may have to specify a fstab written in Qualcomm's format.)
    $(info   ------ You can know its syntax by reading /fstab.qcom on your device.)
    $(info =================================================================================================================)
    MR_FSTAB_FOR_EXTRACTING_BOOTDEV := $(MR_FSTAB)
endif



$(MULTIROM_ZIP_TARGET): multirom trampoline signapk bbootimg mrom_kexec_static mrom_adbd $(multirom_extra_dep)
	@echo
	@echo
	@echo "A crowdfunding campaign for MultiROM took place in 2013. These people got perk 'The Tenth':"
	@echo "    * Bibi"
	@echo "    * flash5000"
	@echo "Thank you. See DONORS.md in MultiROM's folder for more informations."
	@echo
	@echo

	@echo ----- Making MultiROM ZIP installer ------
	rm -rf $(MULTIROM_INST_DIR)
	mkdir -p $(MULTIROM_INST_DIR)
	cp -a $(install_zip_path)/prebuilt-installer/* $(MULTIROM_INST_DIR)/
	cp -a $(TARGET_ROOT_OUT)/multirom $(MULTIROM_INST_DIR)/multirom/
	cp -a $(TARGET_ROOT_OUT)/trampoline $(MULTIROM_INST_DIR)/multirom/
	cp -a $(TARGET_OUT_OPTIONAL_EXECUTABLES)/mrom_kexec_static $(MULTIROM_INST_DIR)/multirom/kexec
	cp -a $(TARGET_OUT_OPTIONAL_EXECUTABLES)/mrom_adbd $(MULTIROM_INST_DIR)/multirom/adbd

	# ANCLARK MODIFIED ON 2017-4-3
	# Bugfix: All possible dependencies should be added.
	# NOTICE: Should specify dependency file list above (TRAMPOLINE_ENCMNT_DEPENDENCIES and TRAMPOLINE_ENCMNT_DEPENDENCIES_VENDOR).
	if $(MR_ENCRYPTION); then \
		mkdir -p $(MULTIROM_INST_DIR)/multirom/enc/res; \
		cp -a $(TARGET_ROOT_OUT)/trampoline_encmnt $(MULTIROM_INST_DIR)/multirom/enc/; \
		cp -a $(TARGET_OUT_EXECUTABLES)/linker $(MULTIROM_INST_DIR)/multirom/enc/; \
		cp -a $(TARGET_OUT_EXECUTABLES)/linker64 $(MULTIROM_INST_DIR)/multirom/enc/; \
		cp -a $(install_zip_path)/prebuilt-installer/multirom/res/Roboto-Regular.ttf $(MULTIROM_INST_DIR)/multirom/enc/res/; \
		\
		for f in $(TRAMPOLINE_ENCMNT_DEPENDENCIES); do cp -av $(TARGET_OUT_SHARED_LIBRARIES)/$$f $(MULTIROM_INST_DIR)/multirom/enc/; done; \
		for f in $(TRAMPOLINE_ENCMNT_DEPENDENCIES_VENDOR); do cp -av $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/$$f $(MULTIROM_INST_DIR)/multirom/enc/; done; \
                if [ -e "$(MR_KEYMASTER_LIB_PATH)" ]; then cp -av "$(MR_KEYMASTER_LIB_PATH)" $(MULTIROM_INST_DIR)/multirom/enc/keystore.default.so; fi; \
		if [ -n "$(MR_ENCRYPTION_SETUP_SCRIPT)" ]; then sh "$(ANDROID_BUILD_TOP)/$(MR_ENCRYPTION_SETUP_SCRIPT)" "$(ANDROID_BUILD_TOP)" "$(MULTIROM_INST_DIR)/multirom/enc"; fi; \
	fi

	mkdir $(MULTIROM_INST_DIR)/multirom/infos
	if [ -n "$(MR_INFOS)" ]; then cp -r $(PWD)/$(MR_INFOS)/* $(MULTIROM_INST_DIR)/multirom/infos/; fi
	cp -a $(TARGET_OUT_OPTIONAL_EXECUTABLES)/bbootimg $(MULTIROM_INST_DIR)/scripts/
	cp $(PWD)/$(MR_FSTAB) $(MULTIROM_INST_DIR)/multirom/mrom.fstab
	$(install_zip_path)/extract_boot_dev.sh $(PWD)/$(MR_FSTAB_FOR_EXTRACTING_BOOTDEV) $(MULTIROM_INST_DIR)/scripts/bootdev
	$(install_zip_path)/make_updater_script.sh "$(MR_DEVICES)" $(MULTIROM_INST_DIR)/META-INF/com/google/android "Installing MultiROM for"
	rm -f $(MULTIROM_ZIP_TARGET).zip $(MULTIROM_ZIP_TARGET)-unsigned.zip
	cd $(MULTIROM_INST_DIR) && zip -qr ../$(notdir $@)-unsigned.zip *
	java -jar $(HOST_OUT_JAVA_LIBRARIES)/signapk.jar $(DEFAULT_SYSTEM_DEV_CERTIFICATE).x509.pem $(DEFAULT_SYSTEM_DEV_CERTIFICATE).pk8 $(MULTIROM_ZIP_TARGET)-unsigned.zip $(MULTIROM_ZIP_TARGET).zip
	$(install_zip_path)/rename_zip.sh $(MULTIROM_ZIP_TARGET) $(TARGET_DEVICE) $(PWD)/$(multirom_local_path)/version.h
	@echo ----- Made MultiROM ZIP installer -------- $@.zip

.PHONY: multirom_zip
multirom_zip: $(MULTIROM_ZIP_TARGET)



MULTIROM_UNINST_TARGET := $(PRODUCT_OUT)/multirom_uninstaller
MULTIROM_UNINST_DIR := $(PRODUCT_OUT)/multirom_uninstaller

$(MULTIROM_UNINST_TARGET): signapk bbootimg
	@echo ----- Making MultiROM uninstaller ------
	rm -rf $(MULTIROM_UNINST_DIR)
	mkdir -p $(MULTIROM_UNINST_DIR)
	cp -a $(install_zip_path)/prebuilt-uninstaller/* $(MULTIROM_UNINST_DIR)/
	cp -a $(TARGET_OUT_OPTIONAL_EXECUTABLES)/bbootimg $(MULTIROM_UNINST_DIR)/scripts/
	$(install_zip_path)/extract_boot_dev.sh $(PWD)/$(MR_FSTAB) $(MULTIROM_UNINST_DIR)/scripts/bootdev
	echo $(MR_RD_ADDR) > $(MULTIROM_UNINST_DIR)/scripts/rd_addr
	$(install_zip_path)/make_updater_script.sh "$(MR_DEVICES)" $(MULTIROM_UNINST_DIR)/META-INF/com/google/android "MultiROM uninstaller -"
	rm -f $(MULTIROM_UNINST_TARGET).zip $(MULTIROM_UNINST_TARGET)-unsigned.zip
	cd $(MULTIROM_UNINST_DIR) && zip -qr ../$(notdir $@)-unsigned.zip *
	java -jar $(HOST_OUT_JAVA_LIBRARIES)/signapk.jar $(DEFAULT_SYSTEM_DEV_CERTIFICATE).x509.pem $(DEFAULT_SYSTEM_DEV_CERTIFICATE).pk8 $(MULTIROM_UNINST_TARGET)-unsigned.zip $(MULTIROM_UNINST_TARGET).zip
	@echo ----- Made MultiROM uninstaller -------- $@.zip

.PHONY: multirom_uninstaller
multirom_uninstaller: $(MULTIROM_UNINST_TARGET)
