#include <stdio.h>

#include "common/strings.h"

/**
 * Returns a string representation of the `msg_type`. The returned string is
 * statically allocated and should not be free()d.
 *
 * @param msg_type the message type
 *
 * @return a statically allocated string respresenting the `msg_type`
 */
const char *strmsgtype(snfs_msg_type msg_type) {
  switch (msg_type) {
    case NOOP:
      return "NOOP";
    case MOUNT:
      return "MOUNT";
    case GETATTR:
      return "GETATTR";
    case READ:
      return "READ";
    case READDIR:
      return "READDIR";
    case WRITE:
      return "WRITE";
    case LOOKUP:
      return "LOOKUP";
    case SETATTR:
      return "SETATTR";
    case ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

/**
 * Returns a string representation of the `error`. The returned string is
 * statically allocated and should not be free()d.
 *
 * @param error the error
 *
 * @return a statically allocated string respresenting the `error`
 */
const char *strsnfserror(snfs_error error) {
  switch (error) {
    case SNFS_ENOTIMPL:
      return "The requested operation has not been implemented.";
    case SNFS_EBADOP:
      return "The requested operation is invalid.";
    case SNFS_ENOENT:
      return "The entry specified does not exist or is invalid.";
    case SNFS_EACCES:
      return "Permission was denied for the specified entry.";
    case SNFS_ENOTDIR:
      return "A directory entry was expected but was not found.";
    case SNFS_EINTERNAL:
      return "There was an internal server error.";
    default:
      return "UNKNOWN";
  }
}

/**
 * Pretty prints `n` bytes of `buf` in hex with 32 hex numbers per line.
 *
 * @param buf the buffer to print from
 * @param n number of bytes in `buf` to print
 */
void printbuf(void *buf, size_t n) {
  size_t i;
  uchar *uchar_buf = (uchar *)buf;
  for (i = 0; i < n; i++) {
    printf("%02X", uchar_buf[i]);
    if ((i + 1) % 32 == 0) printf("\n");
  }

  if (i % 32 != 0) printf("\n");
}
