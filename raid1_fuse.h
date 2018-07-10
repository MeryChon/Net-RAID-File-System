#ifndef _RAID1_FUSE_
#define _RAID1_FUSE_

#define FUSE_USE_VERSION 26
#define PATH_MAX 120

#include <fuse.h>



// static const char *raid1_str = "Hello World!\n";
const char *raid1_root;



int raid1_fuse_main(const char* process_name, const char* mountpoint);
int raid1_getattr(const char *path, struct stat *stbuf);
int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int raid1_open(const char *path, struct fuse_file_info *fi);
int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// int raid1_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);


struct fuse_operations raid1_operations = {
	.getattr	= raid1_getattr,
	.readdir	= raid1_readdir,
	.open		= raid1_open,
	.read		= raid1_read,
	// .write 		= raid1_write, 
	// .releasedir = raid1_releasedir,
	// .rename		= raid1_rename, 
	// .unlink		= raid1_unlink, 
	// .rmdir		= raid1_rmdir, 
	// .mkdir		= mkdir, 
	// .opendir	= opendir, 
	// .releasedir = raid1_releasedir, 
	// .create 	= raid1_create, 
};




#endif