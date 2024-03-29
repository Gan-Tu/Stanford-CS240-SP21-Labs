#define FUSE_USE_VERSION 26

#ifndef SNFS_FUSEOPS_MAIN_H
#define SNFS_FUSEOPS_MAIN_H

#include <fuse.h>
#include <stdbool.h>

#include "common.h"

// These names are way too long.
typedef struct fuse_file_info ffi;
typedef fuse_fill_dir_t ffdt;

snfs_rep *send_request(snfs_req *request, size_t size);
bool lookup(const char *path, fhandle *handle);
bool cached_lookup(const char *path, fhandle *handle);

int snfs_getattr(const char *path, struct stat *stbuf);
int snfs_open(const char *path, ffi *fi);
int snfs_read(const char *, char *, size_t, off_t, ffi *);
int snfs_write(const char *, const char *, size_t, off_t, ffi *);
int snfs_readdir(const char *, void *, ffdt, off_t, ffi *);
int snfs_truncate(const char *, off_t);
int snfs_chmod(const char *path, mode_t mode);
int snfs_chown(const char *path, uid_t uid, gid_t gid);
int snfs_utimens(const char *path, const struct timespec tv[2]);
int snfs_setattr(const char *path, uint64_t which, off_t size, mode_t mode,
                 gid_t uid, uid_t gid, const struct timespec tv[2]);

// Extra Credit

int snfs_create(const char *path, mode_t mode, ffi *fi);
int snfs_unlink(const char *path);
int snfs_rename(const char *oldpath, const char *newpath);
int snfs_release(const char *path, ffi *fi);

int snfs_opendir(const char *path, ffi *fi);
int snfs_mkdir(const char *path, mode_t mode);
int snfs_releasedir(const char *path, ffi *fi);
int snfs_rmdir(const char *path);

#endif
