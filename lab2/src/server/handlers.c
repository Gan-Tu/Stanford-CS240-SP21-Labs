/* #define DEBUG */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "common.h"
#include "server.h"

/*
 * A type-checked send. Use this to send a reply to the client.
 *
 * @param sock the endpoint connected to the client
 * @param reply the reply to send to the client
 * @size the size of the reply
 *
 * @return number of bytes sent on success, < 0 on error
 */
static int send_reply(int sock, snfs_rep *reply, size_t size) {
  return send_data(sock, reply, size, NN_DONTWAIT);
}

/**
 * Takes the attributes in struct stat `st` and stores them in fattr `attr`
 * converting as necessary. Useful for taking a local struct stat and storing
 * its properties in an SNFS fattr.
 *
 * @param st the struct stat to read from
 * @param attr the fattr structure to write to
 */
static void stat_to_fattr(struct stat *st, fattr *attr) {
  assert(st);
  assert(attr);

  // Copying over the relevant fields.
  mode_t mode = st->st_mode;
  if (S_ISREG(mode)) {
    attr->type = SNFREG;
  } else if (S_ISDIR(mode)) {
    attr->type = SNFDIR;
  } else if (S_ISBLK(mode)) {
    attr->type = SNFBLK;
  } else if (S_ISCHR(mode)) {
    attr->type = SNFCHR;
  } else if (S_ISLNK(mode)) {
    attr->type = SNFLNK;
  } else {
    attr->type = SNFNON;
  }

  attr->mode = (uint64_t)mode;
  attr->nlink = (uint64_t)st->st_nlink;
  attr->uid = (uint64_t)st->st_uid;
  attr->gid = (uint64_t)st->st_gid;
  attr->size = (uint64_t)st->st_size;
  attr->rdev = (uint64_t)st->st_rdev;
  attr->fsid = (uint64_t)st->st_dev;
  attr->fileid = (uint64_t)st->st_ino;

  // Now timing information which is, unfortunately, platform dependent
#if defined(__APPLE__)
  attr->atime.useconds = (int64_t)(st->st_atimespec.tv_nsec / 1000);
  attr->atime.seconds = (int64_t)st->st_atimespec.tv_sec;
  attr->mtime.useconds = (int64_t)(st->st_mtimespec.tv_nsec / 1000);
  attr->mtime.seconds = (int64_t)st->st_mtimespec.tv_sec;
  attr->ctime.useconds = (int64_t)(st->st_ctimespec.tv_nsec / 1000);
  attr->ctime.seconds = (int64_t)st->st_ctimespec.tv_sec;
#elif defined(__linux__)
  attr->atime.seconds = (int64_t)st->st_atime;
  attr->mtime.seconds = (int64_t)st->st_mtime;
  attr->ctime.seconds = (int64_t)st->st_ctime;
#endif
}

/**
 * Not a traditional handler: should be called when there's an error.
 *
 * Attempts to send an ERROR reply `error` to the client connected to `sock`.
 *
 * @param sock the endpoint connected to the client
 * @param error the snfs_error being sent to the client
 */
void handle_error(int sock, snfs_error error) {
  debug("Sending error message '%s' to client.\n", strsnfserror(error));
  snfs_rep reply = make_reply(ERROR, .error_rep = {.error = error});

  if (send_reply(sock, &reply, snfs_rep_size(error)) < 0) {
    print_err("Failed to send error message to client.\n");
  }
}

/**
 * The NOOP handler. Handles the NOOP message by sending a NOOP reply.
 *
 * @param sock the endpoint connected to the client
 */
void handle_noop(int sock) {
  snfs_msg_type msg = NOOP;
  if (send_reply(sock, (snfs_rep *)&msg, sizeof(snfs_msg_type)) < 0) {
    print_err("Failed to send NOOP reply.\n");
  }
}

/**
 * The GETATTR handler.
 *
 * Determines the attributes for the file referred to by handle `args->fh` and
 * sends a GETATTR reply to the client with fattr attributes. If the handle is
 * invalid, an SNFS_ENOENT error reply is sent to the client. If the file no
 * longer exists, an SNFS_NOENT error reply is sent to the client. If `stat`
 * fails for any other reason, an SNFS_EINTERNAL error reply is sent to the
 * client.
 *
 * @param sock the endpoint connected to the client
 * @param args the client's arguments
 */
void handle_getattr(int sock, snfs_getattr_args *args) {
  debug("Handling getattr for %" PRIu64 "\n", args->fh);

  const char *file_path = get_file(args->fh);
  if (!file_path) {
    debug("Did not find path for file handle: %" PRIu64 "\n", args->fh);
    return handle_error(sock, SNFS_ENOENT);
  }

  struct stat st;
  if (stat(file_path, &st)) {
    debug("Bad stat for file path %s\n", file_path);
    snfs_error err = (errno == ENOENT) ? SNFS_ENOENT : SNFS_EINTERNAL;
    free((void *)file_path);
    return handle_error(sock, err);
  }

  fattr attributes;
  stat_to_fattr(&st, &attributes);

  snfs_rep reply =
      make_reply(GETATTR, .getattr_rep = {.attributes = attributes});

  debug("Found %s. Sending file attributes\n", file_path);
  if (send_reply(sock, &reply, snfs_rep_size(getattr)) < 0) {
    print_err("Failed to send reply to getattr for %s.\n", file_path);
  }

  free((void *)file_path);
}

/**
 * The READDIR handler.
 *
 * Replies with `args->count` number of `snfsentry`s found in the directory
 * referred to by the `args->dir` file handle. If the handle is invalid or the
 * file it refers to no longer exists, an SNFS_ENOENT error reply is sent to the
 * client.  If the file referred to by the handle is not a directory, an
 * SNFS_ENOTDIR error reply is sent to the client. If a `stat` on the file fails
 * for any other reason, an SNFS_EINTERNAL error is sent to the client.
 *
 * @param sock the endpoint connected to the client
 * @param args the client's arguments
 */
void handle_readdir(int sock, snfs_readdir_args *args) {
  debug("Handling readdir: count %" PRIu64 "\n", args->count);

  // Get the path from the fhandle
  const char *dir_path = get_file(args->dir);
  if (!dir_path) {
    debug("Did not find path for readdir dir: %" PRIu64 "\n", args->dir);
    return handle_error(sock, SNFS_ENOENT);
  }

  // Ensure the file exists and the path supplied was valid
  struct stat st;
  if (stat(dir_path, &st)) {
    debug("Bad stat for dir path %s\n", dir_path);
    if (errno == ENOENT) {
      handle_error(sock, SNFS_ENOENT);
    } else {
      handle_error(sock, SNFS_EINTERNAL);
    }
    return free((void *)dir_path);
  }

  // Make sure the handle points to a directory
  if (!S_ISDIR(st.st_mode)) {
    debug("Not a directory: %s\n", dir_path);
    handle_error(sock, SNFS_ENOTDIR);
    return free((void *)dir_path);
  }

  // Okay, let's try opening the directory
  DIR *dir = opendir(dir_path);
  if (!dir) {
    debug("Failed to open directory: %s\n", dir_path);
    if (errno == ENOENT) {
      handle_error(sock, SNFS_ENOENT);
    } else if (errno == ENOTDIR) {
      handle_error(sock, SNFS_ENOTDIR);
    } else if (errno == EACCES) {
      handle_error(sock, SNFS_EACCES);
    } else {
      handle_error(sock, SNFS_EINTERNAL);
    }
    return free((void *)dir_path);
  }

  // Now, let's iterate the directory entries
  snfsentry entries[args->count];
  struct dirent *entry;
  uint64_t entries_read = 0;
  // Since readdir returns NULL for both error conditions and end of stream, we
  // set the errono to zero before calling the function in order to distinguish
  // them, as advised by https://man7.org/linux/man-pages/man3/readdir.3.html
  errno = 0;

  while ((entry = readdir(dir)) != NULL && entries_read < args->count) {
    snfsentry snfs_entry = {.fileid = entry->d_ino};
    memset(snfs_entry.filename, '\0', sizeof(uint8_t) * SNFS_MAX_FILENAME_BUF);
    memcpy(snfs_entry.filename, (uint8_t *)entry->d_name,
           sizeof(uint8_t) * SNFS_MAX_FILENAME_LENGTH);

    entries[entries_read] = snfs_entry;
    entries_read++;
  }

  // Return any error encountered
  if (errno != 0) {
    debug("Error encountered when reading directory: %s\n", dir_path);
    handle_error(sock, SNFS_EINTERNAL);
    free((void *)dir_path);
    return;
  }

  size_t reply_size = snfs_rep_size(readdir) + sizeof(snfsentry) * entries_read;
  snfs_rep *reply = (snfs_rep *)malloc(reply_size);
  assert_malloc(reply);

  reply->type = READDIR;
  reply->content.readdir_rep.num_entries = entries_read;
  memcpy(reply->content.readdir_rep.entries, entries,
         sizeof(snfsentry) * entries_read);

  debug("Found %" PRIu64 " entries for %s.\n", entries_read, dir_path);
  if (send_reply(sock, reply, reply_size) < 0) {
    print_err("Failed to send reply to readdir for %s.\n", dir_path);
  }

  free((void *)reply);
  free((void *)dir_path);
}

/**
 * The LOOKUP handler.
 *
 * Finds the file named `args->filename` inside the directory referred to by the
 * file handle `args->dir` and returns the file handle and `fattr` attributes
 * for the file. If the directory handle is invalid, the file it refers to no
 * longer exists, or the file `args->filename` is not in that directory, an
 * SNFS_ENOENT error reply is sent to the client.  If the file referred to by
 * the directory handle is not a directory, an SNFS_ENOTDIR error
 * reply is sent to the client. If a `stat` on the directory file fails for any
 * other reason, an SNFS_EINTERNAL error is sent to the client. If the filename
 * in `args->filename` is not well-formed (stat returns ENOTDIR), an SNFS_EBADOP
 * error is sent to the client.
 *
 * @param sock the endpoint connected to the client
 * @param args the client's arguments
 */
void handle_lookup(int sock, snfs_lookup_args *args) {
  debug("Looking up %s in %" PRIu64 "\n", args->filename, args->dir);

  // Get the path from the fhandle
  const char *dir_path = get_file(args->dir);
  if (!dir_path) {
    debug("Did not find path for lookup dir: %" PRIu64 "\n", args->dir);
    return handle_error(sock, SNFS_ENOENT);
  }

  // Ensure the file exists and the path supplied was valid
  struct stat st1;
  if (stat(dir_path, &st1)) {
    debug("Bad stat for dir path %s\n", dir_path);
    if (errno == ENOENT) {
      handle_error(sock, SNFS_ENOENT);
    } else {
      handle_error(sock, SNFS_EINTERNAL);
    }

    goto cleanup_dir_path;
  }

  // Make sure the handle points to a directory
  if (!S_ISDIR(st1.st_mode)) {
    debug("Not a directory: %s\n", dir_path);
    handle_error(sock, SNFS_ENOTDIR);
    goto cleanup_dir_path;
  }

  // Okay, generate path for file asked for. Length includes '/' and '\0'
  size_t dplen = strlen(dir_path);
  size_t flen = strlen((char *)args->filename);
  size_t buf_size = dplen + flen + 2;
  char *file_path = (char *)malloc(buf_size);
  assert_malloc(file_path);

  if (dir_path[dplen - 1] == '/' || args->filename[0] == '/') {
    snprintf(file_path, buf_size, "%s%s", dir_path, args->filename);
  } else {
    snprintf(file_path, buf_size, "%s/%s", dir_path, args->filename);
  }

  // Ensure the file exists.
  struct stat st2;
  if (stat(file_path, &st2)) {
    debug("Bad stat for file path %s\n", file_path);
    if (errno == ENOENT) {
      handle_error(sock, SNFS_ENOENT);
    } else if (errno == ENOTDIR) {
      handle_error(sock, SNFS_EBADOP);
    } else {
      handle_error(sock, SNFS_EINTERNAL);
    }

    goto cleanup_file_path;
  }

  // Okay, we're golden. Get (or create) the fhandle for the file
  fhandle handle = name_find_or_insert(file_path);
  snfs_rep reply = make_reply(LOOKUP, .lookup_rep = {.handle = handle});

  // Fill in the attributes
  fattr *attr = &reply.content.lookup_rep.attributes;
  stat_to_fattr(&st2, attr);

  // Send off the message
  debug("Found '%s', sending handle %" PRIu64 "\n", file_path, handle);
  if (send_reply(sock, &reply, snfs_rep_size(lookup)) < 0) {
    print_err("Failed to send reply to readdir for %s.\n", dir_path);
  }

cleanup_file_path:
  free(file_path);

cleanup_dir_path:
  free((void *)dir_path);
}

/**
 * The MOUNT handler.
 *
 * Determines (or creates) the file handle for the root directory '/' and sets
 * the `root` property in the reply to be the file handle.
 *
 * @param sock the endpoint connected to the client
 */
void handle_mount(int sock) {
  debug("Handling MOUNT.\n");

  snfs_rep reply =
      make_reply(MOUNT, .mount_rep = {.root = name_find_or_insert("/")});

  if (send_reply(sock, &reply, snfs_rep_size(mount)) < 0) {
    print_err("Failed to send root fhandle to client!\n");
  }
}

/**
 * The READ handler.
 *
 * Reads at most `args->count` bytes beginning at offset `args->offset` for the
 * file referred to by the handle `args->file`. If the handle is invalid or the
 * file it refers to no longer exists, an SNFS_ENOENT error reply is sent to the
 * client. If a `stat` on the file fails for any other reason, an SNFS_EINTERNAL
 * error is sent to the client. If the offet is invalid for any reason, an
 * SNFS_EBADOP error reply is sent to the client. If the read fails for any
 * reason, an SNFS_EINTERNAL error reply is sent to the client. The number of
 * bytes actually read is returned in the `count` field of the reply. If the EOF
 * was reached (that is, if `args->offset` + `args->count` >= file size), then
 * the `eof` field is set to true.
 *
 * @param sock the endpoint connected to the client
 * @param args the client's arguments
 */
void handle_read(int sock, snfs_read_args *args) {
  UNUSED(sock);
  UNUSED(args);

  // FIXME: Lookup `args->file`, open, seek, then read into a READ reply
  handle_unimplemented(sock, READ);
}

/**
 * The WRITE handler.
 *
 * Writes `args->count` bytes of `args->data` beginning at offset `args->offset`
 * for the file referred to by the handle `args->file`. If the handle is invalid
 * or the file it refers to no longer exists, an SNFS_ENOENT error reply is sent
 * to the client. If a `stat` on the file fails for any other reason, an
 * SNFS_EINTERNAL error is sent to the client. If the offet is invalid for any
 * reason, an SNFS_EBADOP error reply is sent to the client. The number of bytes
 * actually written is returned in the `count` field of the reply. If the write
 * fails for any reason, an SNFS_EINTERNAL error is sent to the client.
 *
 * @param sock the endpoint connected to the client
 * @param args the client's arguments
 */
void handle_write(int sock, snfs_write_args *args) {
  debug("Handling write to %" PRIu64 " ", args->file);
  debug("[%" PRId64 ":%" PRId64 "]\n", args->offset,
        args->offset + args->count);

  const char *file_path = get_file(args->file);
  if (!file_path) {
    debug("Did not find path for read: %" PRIu64 "\n", args->file);
    return handle_error(sock, SNFS_ENOENT);
  }

  // Try to open the file, and exit early if there's an issue.
  int fd = open(file_path, O_WRONLY);
  if (fd < 0) {
    debug("Could not open(%s)\n", file_path);
    handle_error(sock, (errno == ENOENT) ? SNFS_ENOENT : SNFS_EINTERNAL);
    goto cleanup_file_path;
  }

  // Try to set the offset requested by the user.
  off_t offset = lseek(fd, args->offset, SEEK_SET);
  if (offset != args->offset) {
    debug("Couldn't set client offset.\n");
    handle_error(sock, (offset < 0) ? SNFS_EINTERNAL : SNFS_EBADOP);
    goto cleanup_fd;
  }

  // All set to write the data.
  ssize_t bytes = write(fd, args->data, args->count);
  if (bytes < 0) {
    print_err("Internal issue write()! %s\n", strerror(errno));
    handle_error(sock, SNFS_EINTERNAL);
    goto cleanup_fd;
  }

  debug("Write for %" PRIu64 " done! Wrote %zd bytes.\n", args->file, bytes);
  snfs_rep reply = make_reply(WRITE, .write_rep = {.count = bytes});

  // Send it off!
  if (send_reply(sock, &reply, snfs_rep_size(write)) < 0) {
    print_err("Failed to send reply to read for %s.\n", file_path);
  }

cleanup_fd:
  close(fd);

cleanup_file_path:
  free((void *)file_path);
}

/**
 * The SETATTR handler.
 *
 * Sets the file attributes referred to in the bit flags in `args->which` for
 * the file referred to by the handle `args->file`. Sets the bit flags in the
 * reply's `which` field for the properties that were successfully set. If the
 * handle is invalid or the file it refers to no longer exists, an SNFS_ENOENT
 * error reply is sent to the client. If a `stat` on the file fails for any
 * other reason, an SNFS_EINTERNAL error is sent to the client.
 *
 * @param sock the endpoint connected to the client
 * @param args the client's arguments
 */
void handle_setattr(int sock, snfs_setattr_args *args) {
  uint64_t which = args->which;
  uint64_t which_set = 0;

  const char *file_path = get_file(args->file);
  if (!file_path) {
    debug("Did not find path for setattr: %" PRIu64 "\n", args->file);
    return handle_error(sock, SNFS_ENOENT);
  }

  struct stat st;
  if (stat(file_path, &st)) {
    snfs_error err = (errno == ENOENT) ? SNFS_ENOENT : SNFS_EINTERNAL;
    free((void *)file_path);
    return handle_error(sock, err);
  }

  if (which & SNFS_SETMODE) {
    debug("Setting mode '%s'...\n", file_path);
    if (!chmod(file_path, args->mode)) {
      which_set |= SNFS_SETMODE;
    }
  }

  if (which & SNFS_SETUID) {
    debug("Setting uid '%s'...\n", file_path);
    if (!chown(file_path, args->uid, -1)) {
      which_set |= SNFS_SETUID;
    }
  }

  if (which & SNFS_SETGID) {
    debug("Setting gid '%s'...\n", file_path);
    if (!chown(file_path, -1, args->gid)) {
      which_set |= SNFS_SETGID;
    }
  }

  if (which & SNFS_SETSIZE) {
    debug("Setting size '%s'...\n", file_path);
    if (!truncate(file_path, args->size)) {
      which_set |= SNFS_SETSIZE;
    }
  }

  if (which & SNFS_SETTIMES) {
    debug("Setting times '%s'...\n", file_path);

    struct timeval tv[2];
    tv[0].tv_sec = args->atime.seconds;
    tv[0].tv_usec = args->atime.useconds;
    tv[1].tv_sec = args->mtime.seconds;
    tv[1].tv_usec = args->mtime.useconds;

    // Try once with utimes, which fails at times, then utime, whch should work
    if (!utimes(file_path, tv)) {
      which_set |= SNFS_SETTIMES;
    } else {
      debug("utimes call failed. trying utime\n");
      struct utimbuf time = (struct utimbuf){
          .actime = args->atime.seconds,
          .modtime = args->mtime.seconds,
      };

      if (!utime(file_path, &time)) {
        which_set |= SNFS_SETTIMES;
      }
    }
  }

  // Make and send the reply.
  snfs_rep reply = make_reply(SETATTR, .setattr_rep = {.which = which_set});

  debug("Setattr for %s. Set: %" PRIu64 ".\n", file_path, which_set);
  if (send_reply(sock, &reply, snfs_rep_size(setattr)) < 0) {
    print_err("Failed to send reply to setattr for %s.\n", file_path);
  }

  free((void *)file_path);
}

/**
 * Not a traditional handler: should be called when a message doesn't have an
 * implementation. Simply prints a note and sends an SNFS_ENOTIMPL error reply
 * to the client.
 *
 * @param sock the endpoint connected to the client
 * @param error the msg_type that hasn't been implemented
 */
void handle_unimplemented(int sock, snfs_msg_type msg_type) {
  printf("NOTE: Handler for '%s' is unimplemented.\n", strmsgtype(msg_type));
  handle_error(sock, SNFS_ENOTIMPL);
}
