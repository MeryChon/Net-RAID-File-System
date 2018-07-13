#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "log.h"



static int raid1_getattr(const char *path, struct stat *stbuf);
static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int raid1_open(const char *path, struct fuse_file_info *fi);
static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int raid1_opendir (const char* path, struct fuse_file_info* fi);
static int raid1_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int raid1_mkdir(const char* path, mode_t mode);
static int raid1_rmdir(const char* path);


static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";


struct fuse_operations raid1_operations = {
	.getattr	= raid1_getattr,
	.readdir	= raid1_readdir,
	.open		= raid1_open,
	.read		= raid1_read,
	// .write 		= raid1_write, 
	// .releasedir = raid1_releasedir,
	// .rename		= raid1_rename, 
	// .unlink		= raid1_unlink, 
	.rmdir		= raid1_rmdir, 
	.mkdir		= raid1_mkdir, 
	.opendir	= raid1_opendir, 
	// .releasedir = raid1_releasedir, 
	.create 	= raid1_create, 
};



static void fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, raid1_root);
    if(raid1_root[strlen(raid1_root) - 1] != '/' && path[0] != '/')
    	strncat(fpath, "/", PATH_MAX);
    if(raid1_root[strlen(raid1_root) - 1] == '/' && path[0] == '/') {
        strncat(fpath, path+1, PATH_MAX); // ridiculously long paths will break here
    } else {
    	strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here
    }
    log_msg(fpath);
}


static int raid1_getattr(const char *path, struct stat *stbuf) {
	(void) fi;
	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}


static int raid1_opendir (const char* path, struct fuse_file_info* fi) {
	int res = open(path, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}


static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
		off_t offset, struct fuse_file_info *fi) 
{
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	char fpath[PATH_MAX];
	fullpath(fpath, path);

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		log_msg(de->d_name);
		if (filler(buf, de->d_name, NULL, 0))
			break;
	}

	closedir(dp);
	return 0;
}



static int raid1_mkdir(const char* path, mode_t mode) {
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}


static int raid1_rmdir(const char* path){
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}


static int raid1_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
	int res;

	res = open(path, fi->flags, mode);
	if (res == -1)
		return -errno;
	fi->fh = res;

	return 0;
}



static int raid1_open(const char *path, struct fuse_file_info *fi) {
	uint64_t fd = (uint64_t) open(path, fi->flags);
	fi->fh = fd;
	if (fd < 0)
		return -1;
	return 0;
}


static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	int fd;
	int res;

	if(fi == NULL)
		fd = open(path, O_RDONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}



