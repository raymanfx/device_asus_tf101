//
/* This program is free software. It comes without any warranty, to
* the extent permitted by applicable law. You can redistribute it
* and/or modify it under the terms of the Do What The Fuck You Want
* To Public License, Version 2, as published by Sam Hocevar. See
* http://sam.zoy.org/wtfpl/COPYING for more details. */
//
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define LOG_TAG "Wifimacwriter"
#include <cutils/log.h>

char* read_file(const char *fn)
{
    struct stat st;
    char *data = NULL;

    int fd = open(fn, O_RDONLY);
    if (fd < 0) return data;

    if (fstat(fd, &st)) goto oops;

    data = malloc(st.st_size + 1);
    if (!data) goto oops;

    if (read(fd, data, st.st_size) != st.st_size) goto oops;
    close(fd);
    data[st.st_size + 1] = 0;
    return data;

oops:
    close(fd);
    if (data) free(data);
    return NULL;
}

int copy_nvram(char *src) {

    struct stat st;
    char *mac = NULL;
    char *nvram = NULL;
    char *out = NULL;
    char dest[] = "/data/misc/wifi/nvram.txt";
    char mac_addr[] = "\nmacaddr=\0";
    char over_ride[] = "\nnvram_override=1\n\0";
    int fd = 0;
    int err = 0;

    if (stat(dest, &st) == 0 ) {
        SLOGI("%s exists", dest);
        goto permissions;
    }

    nvram = read_file(src);

    if (!nvram) {
        SLOGE("Failed to read %s %s", src, strerror(errno));
        err = 1;
        goto exit;
    }

    if (stat("/data/wifimac", &st) != 0 ) {
        if (mkdir("/data/wifimac", S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
            SLOGE("Failed to create mount point /data/wifimac %s", strerror(errno));
            err = 1;
            goto write;
        }
    } else {
        chown("/data/wifimac", 0, 0);
        chmod("/data/wifimac", S_IRWXU | S_IRWXG | S_IRWXO);
    }

    if (mount("/dev/block/mmcblk0p5", "/data/wifimac", "vfat", 0, NULL)) {
        SLOGE("Failed to mount /data/wifimac %s", strerror(errno));
        err = 1;
        goto write;
    }

    mac = read_file("/data/wifimac/wifi_mac");

    if (!mac) {
        SLOGE("Failed to read /data/wifimac/wifi_mac %s", strerror(errno));
        err = 1;
        goto write;
    }

    out = malloc((strlen(nvram) + strlen(mac_addr) + strlen(over_ride) + strlen(mac)));

    if (!out) {
        SLOGE("Failed to allocate memory %s", strerror(errno));
        err = 1;
        goto write;
    }

    sprintf(out, "%s%s%s%s", nvram, mac_addr, mac, over_ride);

write:
    if (stat("/data/misc", &st) != 0) {
        SLOGW("Creating directory /data/misc");
        mkdir("/data/misc", S_IRWXU | S_IRWXG);
        chown("/data/misc", 1000, 9998);
    }

    if (stat("/data/misc/wifi", &st) != 0) {
        SLOGW("Creating directory /data/misc/wifi");
        mkdir("/data/misc/wifi", S_IRWXU | S_IRWXG);
        chown("/data/misc/wifi", 1010, 1010);
    }

    fd = open(dest, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (fd < 0) {
        SLOGE("Failed to open %s %s", dest, strerror(errno));
        err = 1;
        goto exit;
    }

    if (!out) {
        write(fd, nvram, strlen(nvram));
    } else {
        write(fd, out, strlen(out));
    }
    close(fd);

permissions:
    if (chown(dest, 1000, 1010) < 0) {
        SLOGE("Failed to set owner/group %s %s", dest, strerror(errno));
        err = 1;
    }
    if (chmod(dest, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) < 0) {
        SLOGE("Failed to set permissions %s %s", dest, strerror(errno));
        err = 1;
    }

exit:
    if (mac) free(mac);
    if (nvram) free(nvram);
    if (out) free(out);
    umount("/data/wifimac");
    rmdir("/data/wifimac");
    return err;
}

int main(int argc, char *argv[])
{
    FILE *fstream = NULL;
    char project_id[16] = { 0 };
    char *src = NULL;
    int err = 0;

    fstream = fopen("/sys/devices/platform/cardhu_misc/cardhu_projectid", "r");

    if (!fstream) {
        SLOGE("Failed to read sysfs %s", strerror(errno));
        err = 1;
        goto exit;
    }

    fscanf(fstream, "%s", project_id);
    fclose(fstream);

    SLOGI("Found project id %s", project_id);

    switch(atoi(project_id)) {
        case 2:
            src = "/system/etc/nvram_nh615.txt";
            err = copy_nvram(src);
            property_set("ro.epad.model_id","02");
            property_set("wifi.module.type","1");
            property_set("wlan.driver.p2p","0");
            break;
        case 4:
            src = "/system/etc/nvram_nh665.txt";
            err = copy_nvram(src);
            property_set("ro.epad.model_id","04");
            property_set("wifi.module.type","2");
            break;
        default:
            SLOGE("Unsupported project id");
            err = 1;
            goto exit;
    }

exit:
    if (err == 0) {
        SLOGI("Completed without error");
    } else {
        SLOGE("Completed with error(s)");
    }
    return err;
}
