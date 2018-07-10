#include "raid1_fuse.h"

#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>



int raid1_fuse_main(const char* process_name, const char* mountpoint) {
	raid1_root = strdup(mountpoint);
	
	char* fuse_args[2];
	fuse_args[0] = strdup(process_name);
	fuse_args[1] = strdup(mountpoint);
	
	return fuse_main(2, fuse_args, &raid1_operations, NULL);
}


static void fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, raid1_root);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here
    printf("%s\n", fpath);

    // log_msg("    bb_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
	   //  BB_DATA->rootdir, path, fpath);
}



int raid1_getattr(const char *path, struct stat *stbuf) {
	// int res = 0;

	// memset(stbuf, 0, sizeof(struct stat));
	// if (strcmp(path, "/") == 0) {
	// 	stbuf->st_mode = S_IFDIR | 0755;
	// 	stbuf->st_nlink = 2;
	// } else if (strcmp(path, raid1_path) == 0) {
	// 	stbuf->st_mode = S_IFREG | 0444;
	// 	stbuf->st_nlink = 1;
	// 	stbuf->st_size = strlen(raid1_str);
	// } else
	// 	res = -ENOENT;
	char fpath[PATH_MAX];
	fullpath(fpath, path);

	return lstat(path, stbuf);
}

int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	// (void) offset;
	// (void) fi;

	// if (strcmp(path, "/") != 0)
	// 	return -ENOENT;

	// filler(buf, ".", NULL, 0);
	// filler(buf, "..", NULL, 0);
	// filler(buf, raid1_path + 1, NULL, 0);
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	DIR* dp = (DIR *) (uintptr_t) fi->fh;
	struct dirent* de = readdir(dp);
	if(de == 0) {
		return 1;
	}

	while ((de = readdir(dp)) != NULL) {
		if (filler(buf, de->d_name, NULL, 0) != 0) {
		    return -ENOMEM;
		}
	}

	return 0;
}

int raid1_open(const char *path, struct fuse_file_info *fi)
{
	int fd;
	int retstat = 0;
    char fpath[PATH_MAX];
    fullpath(fpath, path);

    fd = open(fpath, fi->flags);
    if (fd < 0)
    	retstat = -1;
    fi->fh = fd;
    return retstat;
	// printf("%s\n", "OPEN");
	// int fd = open(path, fi->flags);
	// fi->fh = fd;
	// return fd;
}



int raid1_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	// size_t len;
	// (void) fi;
	// if(strcmp(path, raid1_path) != 0)
	// 	return -ENOENT;

	// len = strlen(raid1_str);
	// if (offset < len) {
	// 	if (offset + size > len)
	// 		size = len - offset;
	// 	memcpy(buf, raid1_str + offset, size);
	// } else
	// 	size = 0;
	return pread(fi->fh, buf, size, offset);
}


// int raid1_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
// 	int fd = fi->fh;
// 	return write(fd, buf, size);
// }