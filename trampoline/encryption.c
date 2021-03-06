/*
 * This file is part of MultiROM.
 *
 * MultiROM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MultiROM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MultiROM.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <string.h>

#include "../lib/fstab.h"
#include "../lib/util.h"
#include "../lib/log.h"
#include "encryption.h"
#include "../trampoline_encmnt/encmnt_defines.h"

static char encmnt_cmd_arg[64] = { 0 };
static char *const encmnt_cmd[] = { "/mrom_enc/trampoline_encmnt", encmnt_cmd_arg, NULL };
static char *const encmnt_envp[] = { "LD_LIBRARY_PATH=/mrom_enc/", NULL };
static int g_decrypted = 0;

int encryption_before_mount(struct fstab *fstab)
{
    int exit_code = -1;
    char *output = NULL, *itr;
    int res = ENC_RES_ERR;

    mkdir_recursive("/system/bin", 0755);

    //ANCLARK MODIFIED on 2017-04-19
    //Fix linker support
    //Implement linker64 environment (for ARM64 devices)
    remove("/system/bin/linker64");
    symlink("/mrom_enc/linker64", "/system/bin/linker64");
    chmod("/mrom_enc/linker64", 0775);
    chmod("/mrom_enc/trampoline_encmnt", 0775);

    //Implement linker32 environment (for legacy ARM devices)
    remove("/system/bin/linker");
    symlink("/mrom_enc/linker", "/system/bin/linker");
    chmod("/mrom_enc/linker", 0775);
    chmod("/mrom_enc/trampoline_encmnt", 0775);



#ifdef MR_USE_KEYMASTER
    //ANCLARK MODIFIED 2017-4-7
    // Prepare keystore library
    int ret_symlink_keystore[2] = {-8};
    
    remove("/system/lib64");
    mkdir_recursive("/system/lib64/hw", 0755);
    ret_symlink_keystore[0] = symlink("/mrom_enc/keystore.default.so", "/system/lib64/hw/keystore.default.so");

    remove("/system/lib");
    mkdir_recursive("/system/lib/hw", 0755);
    ret_symlink_keystore[1] = symlink("/mrom_enc/keystore.default.so", "/system/lib/hw/keystore.default.so");


    //Check if keystore module prepared or not
    if(ret_symlink_keystore[0] == 0)
        INFO("Keystore library 64-bit prepared.");
    else
        ERROR("FATAL: Keystore library 64-bit prepare failure!");
    if(ret_symlink_keystore[1] == 0)
        INFO("Keystore library 32-bit prepared.");
    else 
        ERROR("FATAL: Keystore library 32-bit prepare failure!");    

#endif

    remove("/vendor");
    symlink("/mrom_enc/vendor", "/vendor");

    mkdir("/firmware", 0775);
    struct fstab_part *fwpart = fstab_find_first_by_path(fstab, "/firmware");
    if(fwpart && strcmp(fwpart->type, "emmc") != 0)
    {
        if(mount(fwpart->device, "/firmware", fwpart->type, fwpart->mountflags, NULL) < 0)
            ERROR("Mounting /firmware for encryption failed with %s\n", strerror(errno));
    }

    INFO("Running trampoline_encmnt\n");

    strcpy(encmnt_cmd_arg, "decrypt");
    output = run_get_stdout_with_exit_with_env(encmnt_cmd, &exit_code, encmnt_envp);
    if(exit_code != 0 || !output)
    {
        ERROR("Failed to run trampoline_encmnt, exit code %d: %s\n", exit_code, output);
        goto exit;
    }

    itr = output + strlen(output) - 1;
    while(itr >= output && isspace(*itr))
        *itr-- = 0;

    if(strcmp(output, ENCMNT_BOOT_INTERNAL_OUTPUT) == 0)
    {
        INFO("trampoline_encmnt requested to boot internal ROM.\n");
        res = ENC_RES_BOOT_INTERNAL;
        goto exit;
    }

    if(!strstartswith(output, "/dev"))
    {
        ERROR("Invalid trampoline_encmnt output: %s\n", output);
        goto exit;
    }

    g_decrypted = 1;

    struct fstab_part *datap = fstab_find_first_by_path(fstab, "/data");
    if(!datap)
    {
        ERROR("Failed to find /data in fstab!\n");
        goto exit;
    }

    INFO("Updating device %s to %s in fstab due to encryption.\n", datap->device, output);
    fstab_update_device(fstab, datap->device, output);

    res = ENC_RES_OK;
exit:
    free(output);
    return res;
}

void encryption_destroy(void)
{
    int res = -1;
    int exit_code = -1;
    char *output = NULL;
    struct stat info;

    if(g_decrypted)
    {
        strcpy(encmnt_cmd_arg, "remove");
        output = run_get_stdout_with_exit_with_env(encmnt_cmd, &exit_code, encmnt_envp);
        if(exit_code != 0)
            ERROR("Failed to run trampoline_encmnt: %s\n", output);
        g_decrypted = 0;
        free(output);
    }

	
    // Make sure we're removing our symlink and not ROM's linker
	//ANCLARK MODIFIED on 2017-4-18
	//Don't forget 64-bit linker!

    if(lstat("/system/bin/linker64", &info) >= 0 && S_ISLNK(info.st_mode))
        remove("/system/bin/linker64");

    if(lstat("/system/bin/linker", &info) >= 0 && S_ISLNK(info.st_mode))
        remove("/system/bin/linker");


}

int encryption_cleanup(void)
{
    remove("/vendor");

//ANCLARK MODIFIED ON 2017-04-21
//Also cleanup keymaster lib. But you can keep it with MR_DEBUG_KEEP_KEYMASTER_DEFAULT.
#ifdef MR_USE_KEYMASTER
    #ifndef MR_DEBUG_KEEP_KEYMASTER_DEFAULT_SO
        remove("/system/lib64/hw/keystore.default.so");
        remove("/system/lib/hw/keystore.default.so");
    #endif
#endif

    if(access("/firmware", R_OK) >= 0 && umount("/firmware") < 0)
        ERROR("encryption_cleanup: failed to unmount /firmware: %s\n", strerror(errno));

    rmdir("/firmware");
    return 0;
}
