#ifndef SNFS_COMMON_REQUESTS_H
#define SNFS_COMMON_REQUESTS_H

#include "ftypes.h"

typedef struct packed snfs_mount_args_struct {
  /* No arguments. */
} snfs_mount_args;

typedef struct packed snfs_noop_args_struct {
  /* No arguments. */
} snfs_noop_args;

typedef struct packed snfs_getattr_args_struct {
  fhandle fh;  // The file handle to getattrs for.
} snfs_getattr_args;

typedef struct packed snfs_read_args_struct {
  fhandle file;    // Handle of file being read from.
  int64_t offset;  // Offset (in bytes) of where to start reading.
  uint64_t count;  // Max number of bytes to read.
} snfs_read_args;

typedef struct packed snfs_write_args_struct {
  fhandle file;    // Handle of the file to write to.
  int64_t offset;  // Where to begin writing.
  uint64_t count;  // Number of bytes to write.
  uint8_t data[];  // The data.
} snfs_write_args;

typedef struct snfs_readdir_args_struct {
  fhandle dir;     // Handle of directory to read from.
  uint64_t count;  // Number of entries to return.
} snfs_readdir_args;

typedef struct snfs_lookup_args_struct {
  fhandle dir;                              // Which directory to lookup in.
  uint8_t filename[SNFS_MAX_FILENAME_BUF];  // Name of the file.
} snfs_lookup_args;

typedef struct snfs_setattr_args_struct {
  fhandle file;        // Handle of file to set attrs for.
  uint64_t which;      // A union (ORed set of) the 'which' bits
  uint64_t mode;       // Mode to set if SNFS_SETMODE is in which
  uint64_t uid;        // uid to set if SNFS_SETUID is in which
  uint64_t gid;        // gid to set if SNFS_SETGID is in which
  int64_t size;        // size to set if SNFS_SETSIZE is in which
  snfs_timeval atime;  // access time to set if SNFS_SETTIMES is in which
  snfs_timeval mtime;  // mod time to set if SNFS_SETTIMES is in which
} snfs_setattr_args;

// Extra Credit
typedef struct snfs_create_args_struct {
  /* FIXME. */
} snfs_create_args;

typedef struct snfs_unlink_args_struct {
  /* FIXME. */
} snfs_unlink_args;

typedef struct snfs_rename_args_struct {
  /* FIXME. */
} snfs_rename_args;

typedef struct snfs_mkdir_args_struct {
  /* FIXME. */
} snfs_mkdir_args;

typedef struct snfs_rmdir_args_struct {
  /* FIXME. */
} snfs_rmdir_args;

typedef struct packed snfs_req_struct {
  snfs_msg_type type;
  union {
    snfs_getattr_args getattr_args;
    snfs_read_args read_args;
    snfs_readdir_args readdir_args;
    snfs_lookup_args lookup_args;
    snfs_write_args write_args;
    snfs_setattr_args setattr_args;
    // Extra Credit
    snfs_create_args create_args;
    snfs_unlink_args unlink_args;
    snfs_rename_args rename_args;
    snfs_mkdir_args mkdir_args;
    snfs_rmdir_args rmdir_args;
  } content;
} snfs_req;

#endif
