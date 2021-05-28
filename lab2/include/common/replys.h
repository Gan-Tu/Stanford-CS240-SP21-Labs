#ifndef SNFS_COMMON_REPLYS_H
#define SNFS_COMMON_REPLYS_H

#include "ftypes.h"

typedef struct packed snfs_getattr_rep_struct {
  fattr attributes;  // The attributes for the file.
} snfs_getattr_rep;

typedef struct packed snfs_error_rep_struct {
  snfs_error error;  // The error type.
} snfs_error_rep;

typedef struct packed snfs_mount_rep_struct {
  fhandle root;  // The root file handle.
} snfs_mount_rep;

typedef struct packed snfs_readdir_rep_struct {
  uint64_t num_entries;  // The number of entries in entries[].
  snfsentry entries[];   // The entries themselves.
} snfs_readdir_rep;

typedef struct packed snfs_lookup_rep_struct {
  fhandle handle;    // Handle for the file that was found.
  fattr attributes;  // Attributes of said file.
} snfs_lookup_rep;

typedef struct packed snfs_read_rep_struct {
  uint64_t count;  // Number of bytes in data.
  uint64_t eof;    // Whether EOF was reached.
  uint8_t data[];  // The data itself.
} snfs_read_rep;

typedef struct packed snfs_write_rep_struct {
  uint64_t count;  // Number of bytes actually written.
} snfs_write_rep;

typedef struct snfs_setattr_rep_struct {
  uint64_t which;  // a union (ORed set of) the 'which' bits changed
} snfs_setattr_rep;

// Extra Credit
typedef struct snfs_create_rep_struct {
  fhandle handle;  // Handle for the file that was created.
} snfs_create_rep;

typedef struct snfs_remove_rep_struct {
  /* No reply. */
} snfs_remove_rep;

typedef struct snfs_rename_rep_struct {
  fhandle handle;  // Handle for the new file that was created.
} snfs_rename_rep;

typedef struct snfs_mkdir_rep_struct {
  fhandle handle;    // Handle for the directory that was created.
} snfs_mkdir_rep;

typedef struct snfs_rmdir_rep_struct {
  /* No reply. */
} snfs_rmdir_rep;

typedef struct packed snfs_rep_struct {
  snfs_msg_type type;
  union {
    snfs_error_rep error_rep;
    snfs_getattr_rep getattr_rep;
    snfs_mount_rep mount_rep;
    snfs_readdir_rep readdir_rep;
    snfs_lookup_rep lookup_rep;
    snfs_read_rep read_rep;
    snfs_write_rep write_rep;
    snfs_setattr_rep setattr_rep;
    // Extra Credit
    snfs_create_rep create_rep;
    snfs_remove_rep remove_rep;
    snfs_rename_rep rename_rep;
    snfs_mkdir_rep mkdir_rep;
    snfs_rmdir_rep rmdir_rep;
  } content;
} snfs_rep;

#endif
