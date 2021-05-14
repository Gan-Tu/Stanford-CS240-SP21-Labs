#ifndef SNFS_COMMON_FTYPES_H
#define SNFS_COMMON_FTYPES_H

#include <limits.h>
#include <stdint.h>

// Typing that long attribute is hard
#define packed __attribute__((__packed__))

// Filename lengths
#define SNFS_MAX_FILENAME_LENGTH 255
#define SNFS_MAX_FILENAME_BUF 256

// The 'which' bits
#define SNFS_SETMODE    1 << 0
#define SNFS_SETUID     1 << 1
#define SNFS_SETGID     1 << 2
#define SNFS_SETSIZE    1 << 3
#define SNFS_SETTIMES   1 << 4

// Because unsigned char is long...
typedef unsigned char uchar;

// Because unsigned int is long...
typedef unsigned int uint;

// A randomly generated handle to a file.
typedef uint64_t fhandle;

typedef struct packed snfs_timeval_struct {
  int64_t seconds;
  int64_t useconds;
} snfs_timeval;

typedef struct packed snfsentry {
  uint64_t fileid;                          // A unique file id.
  uint8_t filename[SNFS_MAX_FILENAME_BUF];  // The name of the file (truncated)
} snfsentry;

typedef enum packed ftype_enum {
  SNFNON = 0,
  SNFREG = 1,
  SNFDIR = 2,
  SNFBLK = 3,
  SNFCHR = 4,
  SNFLNK = 5,
  FTYPE_PAD_NUM = INT_MAX
} ftype;

// Attributes are just as in stat, except for fsid, which is `dev` in stat
typedef struct packed fattr_struct {
  ftype         type;
  uint64_t      mode;
  uint64_t      nlink;
  uint64_t      uid;
  uint64_t      gid;
  uint64_t      size;
  uint64_t      rdev;
  uint64_t      fsid;
  uint64_t      fileid;
  snfs_timeval  atime;
  snfs_timeval  mtime;
  snfs_timeval  ctime;
} fattr;

typedef enum packed snfs_msg_type_enum {
  NOOP,
  MOUNT,
  GETATTR,
  READDIR,
  LOOKUP,
  READ,
  WRITE,
  SETATTR,
  ERROR,
  PAD_MSG_TYPE_ENUM = INT_MAX
} snfs_msg_type;

typedef enum packed snfs_error_enum {
  SNFS_ENOTIMPL,
  SNFS_EBADOP,
  SNFS_ENOENT,
  SNFS_EACCES,
  SNFS_ENOTDIR,
  SNFS_EINTERNAL,
  PAD_ERROR_ENUM = INT_MAX
} snfs_error;

#endif
