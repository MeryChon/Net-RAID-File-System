#include "raid1_fuse.h"

#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>



static int raid1_getattr(const char *path, struct stat *stbuf);
static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int raid1_open(const char *path, struct fuse_file_info *fi);
static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// static int raid1_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);

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
	// .rmdir		= raid1_rmdir, 
	// .mkdir		= mkdir, 
	// .opendir	= opendir, 
	// .releasedir = raid1_releasedir, 
	// .create 	= raid1_create, 
};


void log_to_file(const char* log_text) {
	if(log_text != NULL){
		FILE* log_file = fopen("/home/vagrant/code/personal/Net-RAID-File-System/error.log", "a");
		if(log_file) {
			time_t t = time(0);
			struct tm* tm = localtime(&t);
			char time_string [26];
			strftime(time_string, 26, "%Y-%m-%d %H:%M:%S", tm);
			fprintf(log_file, "%s --- %s\n", time_string, log_text);
			fclose(log_file);
		}
	}	
}


static void fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, raid1_root);
    log_to_file(fpath);

    if(raid1_root[strlen(raid1_root) - 1] != '/' && path[0] != '/')
    	strncat(fpath, "/", PATH_MAX);
    log_to_file(fpath);
    if(raid1_root[strlen(raid1_root) - 1] == '/' && path[0] == '/') {
        strncat(fpath, path+1, PATH_MAX); // ridiculously long paths will break here
    } else {
    	strncat(fpath, path, PATH_MAX); // ridiculously long paths will break here
    }
    log_to_file(fpath);
    
    // printf("%s\n", fpath);
}



// static int raid1_getattr(const char *path, struct stat *stbuf) {
// 	// char fpath[PATH_MAX];
// 	// fullpath(fpath, path);

// 	return lstat(path, stbuf);
// }

static int raid1_getattr(const char *path, struct stat *stbuf) {
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	} else
		res = -ENOENT;

	return res;
}


// static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
// 		off_t offset, struct fuse_file_info *fi)
// {
// 	filler(buf, ".", NULL, 0);
// 	filler(buf, "..", NULL, 0);
// 	DIR* dp = (DIR *) (uintptr_t) fi->fh;
// 	struct dirent* de = readdir(dp);
// 	if(de == 0) {
// 		return 1;
// 	}

// 	while ((de = readdir(dp)) != NULL) {
// 		if (filler(buf, de->d_name, NULL, 0) != 0) {
// 		    return -ENOMEM;
// 		}
// 	}
// 	return 0;
// }




static int raid1_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
		off_t offset, struct fuse_file_info *fi) 
{
	(void) offset;
	// (void) fi;
	char fpath[PATH_MAX];
	
	fullpath(fpath, path);
	
	DIR* dp = opendir(fpath);
	
	if(dp == NULL){
		return 1;
	}

	struct dirent* de = readdir(dp);

	// while((de = readdir(dp)) != NULL) {
	// 	printf("%s\n", de->d_name);
		int buf_res = filler(buf, de->d_name, NULL, 0);
		if(buf_res != 0)
			return -ENOMEM;
		closedir(dp);
	// 	return 0;
	// }

	// if (strcmp(path, "/") != 0)
	// 	return -ENOENT;

	// if(de == NULL)
	// 	return -1;
	// while(de != NULL) {
	// 	int buf_res = filler(buf, de->d_name, NULL, 0);
	// 	if(buf_res != 0)
	// 		return -ENOMEM;
	// 	de = readdir(dp);
	// }

	// filler(buf, ".", NULL, 0);
	// filler(buf, "..", NULL, 0);
	// filler(buf, hello_path + 1, NULL, 0);

	return 0;
}

// static int raid1_open(const char *path, struct fuse_file_info *fi) {
// 	int fd;
// 	int retstat = 0;
//     // char fpath[PATH_MAX];
//     // fullpath(fpath, path);

//     fd = open(path, fi->flags);
//     if (fd < 0)
//     	retstat = -1;
//     fi->fh = fd;
//     return retstat;
// }



static int raid1_open(const char *path, struct fuse_file_info *fi) {
	int fd = open(path, fi->flags);
	fi->fh = fd;
	if (fd < 0)
		return -1;
	return 0;
}



// static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
// 	return pread(fi->fh, buf, size, offset);
// }

static int raid1_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
	size_t len;
	(void) fi;
	if(strcmp(path, hello_path) != 0)
		return -ENOENT;

	len = strlen(hello_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, hello_str + offset, size);
	} else
		size = 0;

	return size;
}

// static int raid1_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
// 	int fd = fi->fh;
// 	return write(fd, buf, size);
// }



int raid1_fuse_main(const char* process_name, const char* mountpoint) {
	raid1_root = strdup(mountpoint);
	
	char* fuse_args[2];
	fuse_args[0] = strdup(process_name);
	fuse_args[1] = strdup(mountpoint);
	
	return fuse_main(2, fuse_args, &raid1_operations, NULL);
}