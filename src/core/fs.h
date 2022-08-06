#pragma once
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "db/database.h"

int // returns zero on success
fs_copy(
    const char *dst,
    const char *src);

int // returns zero on success
fs_delete(
    const char *fn);

int // returns zero on success
fs_mkdir(
    const char *pathname,
    mode_t      mode);

void  // returns the directory where the actual binary (not the symlink) resides
fs_basedir(
    char *basedir,  // output will be copied here
    size_t maxlen);  // allocation size

int // return the number of devices found
fs_find_usb_block_devices(
    char devname[20][20],
    int  mounted[20]);

int ap_fs_get_subdirs(const char *parent, ap_nodes_t **nodes);
int dt_fs_filter_dir(const struct dirent *d);
int dt_fs_filter_file(const struct dirent *d);
int ap_fs_get_images(const char *node, const int type, ap_image_t **images);
