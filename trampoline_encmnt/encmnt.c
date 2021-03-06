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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include <malloc.h>

#include "../lib/log.h"
#include "../lib/fstab.h"
#include "../lib/framebuffer.h"
#include "../lib/util.h"

#include "crypto/lollipop/cryptfs.h"

#include "pw_ui.h"
#include "encmnt_defines.h"

#define CMD_NONE 0
#define CMD_DECRYPT 1
#define CMD_REMOVE 2
#define CMD_PWTYPE 3

static int get_footer_from_opts(char *output, size_t output_size, const char *opts2)
{
    char *r, *saveptr;
    char *dup = strdup(opts2);
    int res = -1;
    int i;

    r = strtok_r(dup, ",", &saveptr);

    static const char *names[] = {
        "encryptable=",
        "forceencrypt=",
        NULL
    };

    while(r)
    {
        for(i = 0; names[i]; ++i)
        {
            if(strstartswith(r, names[i]))
            {
                snprintf(output, output_size, "%s", r + strlen(names[i]));
                res = 0;
                goto exit;
            }
        }

        r = strtok_r(NULL, ",", &saveptr);
    }

exit:
    free(dup);
    return res;
}

static void print_help(char *argv[]) {
#ifdef MR_SHOW_CRYPTFS_LOG_TO_KMSG
    fprintf(stderr, "Usage: %s [-d|--debug] COMMAND ARGUMENTS\n"
        "Available commands:\n"
        "     decrypt PASSWORD - decrypt data using PASSWORD.\n"
        "             Prints out dm block device path on success.\n"
        "     remove - unmounts encrypted data\n"
        "     pwtype - prints password type as integer\n",
        argv[0]);

    fprintf(stderr, "\nAdditional switch:\n"
        "     -d, --debug - Print cryptfs's debug messages to terminal.\n"
        "         If not specified, it will be printed to kernel log.\n"
        );
#else
    fprintf(stderr, "Usage: %s COMMAND ARGUMENTS\n"
        "Available commands:\n"
        "     decrypt PASSWORD - decrypt data using PASSWORD.\n"
        "             Prints out dm block device path on success.\n"
        "     remove - unmounts encrypted data\n"
        "     pwtype - prints password type as integer\n"
        "\nNOTICE: Debug messages from cryptfs will be printed.\n",
        argv[0]);
#endif
}

static int handle_pwtype(int stdout_fd)
{
    INFO("WORK MODE: pwtype");
    printf("======== WORK MODE: pwtype ========\n\n");

    if(cryptfs_check_footer() < 0)
    {
        ERROR("cryptfs_check_footer failed!");
        return -1;
    }

    int pwtype = cryptfs_get_password_type();
    if(pwtype < 0)
    {
        ERROR("cryptfs_get_password_type failed!");
        return -1;
    }

    char buff[32];
    snprintf(buff, sizeof(buff), "%d\n", pwtype);
    write(stdout_fd, buff, strlen(buff));
    fsync(stdout_fd);

    //ANCLARK MODIFY 2017-4-5
    //Log password type to dmesg
    INFO("Get password type succeed.");
    INFO("Queried password type ID => %s", buff);
    //Also, log to stdout
    printf("[Queried password type ID => %s ]", buff);
    return 0;
}

static int handle_decrypt(int stdout_fd, const char *password)
{
    INFO("WORK MODE: decrypt");
    printf("======== WORK MODE: decrypt ========\n\n");


    DIR *d;
    struct dirent *de;
    char buff[256];
    int res = -1;
    static const char *default_password = "default_password";


    printf("\n*** Checking footer...\n");

    if(cryptfs_check_footer() < 0)
    {
        ERROR("cryptfs_check_footer failed!");
        return -1;
    }


    printf("\n*** Getting password type...\n");

    int pwtype = cryptfs_get_password_type();
    if(pwtype < 0)
    {
        ERROR("cryptfs_get_password_type failed!");
        return -1;
    }
    else if (pwtype == CRYPT_TYPE_DEFAULT)
        password = default_password;

    printf("\n*** Ready to decrypt...\n");

    if(password)
    {
        printf("\n*** Attempting to decrypt and mount data partition...\n");

        if(cryptfs_check_passwd(password) < 0)
        {
            ERROR("CORE DECRYPT FUNCTION FAILED: cryptfs_check_passwd failed!");
            return -1;
        }
    }
    else
    {
        printf("\n*** Senior encryption detected. Switching to Password UI...\n");

        switch(pw_ui_run(pwtype))
        {
            default:
            case ENCMNT_UIRES_ERROR:
                ERROR("pw_ui_run() failed!");
                return -1;
            case ENCMNT_UIRES_BOOT_INTERNAL:
                INFO("Wants to boot internal!");
                write(stdout_fd, ENCMNT_BOOT_INTERNAL_OUTPUT, strlen(ENCMNT_BOOT_INTERNAL_OUTPUT));
                fsync(stdout_fd);
                return 0;
            case ENCMNT_UIRES_PASS_OK:
                break;
        }
    }

    d = opendir("/dev/block/");
    if(!d)
    {
        ERROR("Failed to open /dev/block, wth? %s", strerror(errno));
        return -1;
    }

    // find the block device
    while((de = readdir(d)))
    {
        if(de->d_type == DT_BLK && strncmp(de->d_name, "dm-", 3) == 0)
        {
            snprintf(buff, sizeof(buff), "/dev/block/%s\n", de->d_name);
            INFO("Found block device %s", buff);
            write(stdout_fd, buff, strlen(buff));
            fsync(stdout_fd);
            res = 0;
            break;
        }
    }

    closedir(d);
    return res;
}

static int handle_remove(void)
{
    INFO("WORK MODE: remove");
    printf("======== WORK MODE: remove ========\n\n");

    if(delete_crypto_blk_dev("userdata") < 0)
    {
        ERROR("delete_crypto_blk_dev failed!");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int i;
    int res = 1;
    int cmd = CMD_NONE;
    int stdout_fd;
    char footer_location[256];
    struct fstab *fstab;
    struct fstab_part *p;
    char *argument = NULL;


    klog_init();

    // output all messages to dmesg,
    // but it is possible to filter out INFO messages
    klog_set_level(6);

    mrom_set_log_tag("trampoline_encmnt");
    mrom_set_dir("/mrom_enc/");

#ifdef MR_SHOW_CRYPTFS_LOG_TO_KMSG
    //ANCLARK MODIFIED on 2017-04-21
    // Add debug switch --- in debug mode, all info from cryptfs will directedly print to stdout.
    int debug_mode = 0;
#endif

    for(i = 1; i < argc; ++i)
    {
        if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            print_help(argv);
            return 0;
        }

#ifdef MR_SHOW_CRYPTFS_LOG_TO_KMSG
        //ANCLARK MODIFIED on 2017-04-20
        // Use argument to switch debug mode.
        else if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug"))
        {
            debug_mode = 1;
            INFO("Debug mode activitated --- cryptfs debug info will print on stdout");
        }
#endif

        else if(cmd == CMD_NONE)
        {
            if(strcmp(argv[i], "decrypt") == 0)
                cmd = CMD_DECRYPT;
            else if(strcmp(argv[i], "remove") == 0)
                cmd = CMD_REMOVE;
            else if(strcmp(argv[i], "pwtype") == 0)
                cmd = CMD_PWTYPE;
        }
        else if(!argument)
        {
            argument = argv[i];
        }
    }

    if(argc == 1 || cmd == CMD_NONE)
    {
        print_help(argv);
        return 0;
    }

    INFO("Trampoline_encmnt started");

    fstab = fstab_auto_load();
    if(!fstab)
    {
        ERROR("Failed to load fstab!");
        return 1;
    }

    p = fstab_find_first_by_path(fstab, "/data");
    if(!p)
    {
        ERROR("Failed to find /data partition in fstab");
        goto exit;
    }

    if(get_footer_from_opts(footer_location, sizeof(footer_location), p->options2) < 0)
        goto exit;

    INFO("Setting encrypted partition data to %s %s %s", p->device, footer_location, p->type);
    set_partition_data(p->device, footer_location, p->type);

    // cryptfs prints informations, we don't want that
    // AnClark DISAGREED 2017-4-5: This will supress debug messages!!!!! STOP DOING THIS!!!!!
    //stdout_fd = dup(1);
    //freopen("/dev/null", "ae", stdout);


#ifdef MR_SHOW_CRYPTFS_LOG_TO_KMSG
    /**
       ANCLARK MODIFIED on 2017-04-20
       Buffer cryptfs log to a string. Notice that cryptfs prints debug info directedly to stdout. 
       But if we are in debug mode, We will keep stdout original.
    **/

    // Variables for gathering cryptfs's debug info in stdout
    FILE *stream;
    char buff_cryptfs[65536]={'\0'};

    // Configure streams
    // If not in debug mode, redirect stdout to our buffer.
    if(!debug_mode)
    {
        stream = freopen("/dev/null", "a", stdout);
        setbuf(stdout, buff_cryptfs);
    }

    /** Modification end **/

#endif


    switch(cmd)
    {
        case CMD_PWTYPE:
            if(handle_pwtype(stdout_fd) < 0)
                goto exit;
            break;
        case CMD_DECRYPT:
            if(handle_decrypt(stdout_fd, argument) < 0)
                goto exit;
            break;
        case CMD_REMOVE:
            if(handle_remove() < 0)
                goto exit;
            break;
    }

    res = 0;
exit:

#ifdef MR_SHOW_CRYPTFS_LOG_TO_KMSG
    /**
       ANCLARK MODIFIED on 2017-04-20
       Print out buffered cryptfs' debug info through kernel log.
       Use strtok() to split and output log line by line.
    **/    
    if(!debug_mode)
    {
        fflush(stdout);
        INFO("============= LOG INFO OF cryptfs =============");
        
        char *curr_line = strtok(buff_cryptfs, "\n");
        while(curr_line){
                INFO("***  %s", curr_line);
                curr_line = strtok(NULL, "\n");
        }

        INFO("===============================================");
    }
    /** Modification end **/
#endif


    INFO("Exit trampoline_encmnt...");
    fstab_destroy(fstab);
    return res;
}
