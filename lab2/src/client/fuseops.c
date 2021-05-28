/* #define DEBUG */
#define FUSE_USE_VERSION 26

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <getopt.h>
#include <inttypes.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "common.h"

/**
 * Takes the attributes in fattr `attr` and stores them in struct state `st`,
 * converting as necessary. Useful for taking a server's fattr response and
 * storing it in a local struct stat for FUSE.
 *
 * @param attr the fattr structure to read from
 * @param st the struct stat to write to
 */
static void fattr_to_stat(fattr *attr, struct stat *st) {
  assert(st);
  assert(attr);

  st->st_mode = attr->mode;
  st->st_nlink = attr->nlink;
  st->st_uid = attr->uid;
  st->st_gid = attr->gid;
  st->st_size = attr->size;
  st->st_ino = attr->fileid;

#if defined(__APPLE__)
  st->st_atimespec.tv_nsec = attr->atime.useconds * 1000;
  st->st_atimespec.tv_sec = attr->atime.seconds;
  st->st_ctimespec.tv_nsec = attr->ctime.useconds * 1000;
  st->st_ctimespec.tv_sec = attr->ctime.seconds;
  st->st_mtimespec.tv_nsec = attr->mtime.useconds * 1000;
  st->st_mtimespec.tv_sec = attr->mtime.seconds;
#else
  st->st_atime = attr->atime.seconds;
  st->st_mtime = attr->mtime.seconds;
  st->st_ctime = attr->ctime.seconds;
#endif
}

/**
 * Sends a request to the server using `STATE->server_sock` and
 * `STATE->server_url` and returns the reply. If the server didn't reply or the
 * reply was an error, returns NULL.
 *
 * @param request the request to send to the server
 * @param size the size in bytes of the request
 *
 * @return the reply if there was a valid one, NULL otherwise
 */
snfs_rep *send_request(snfs_req *request, size_t size) {
  assert(request);
  assert(size >= sizeof(snfs_msg_type));
  debug("Sending %s request...\n", strmsgtype(request->type));

  int sock = STATE->server_sock;
  const char *url = STATE->server_url;
  snfs_rep *reply = snfs_req_rep_f(sock, url, request, size, NN_DONTWAIT);
  if (!reply) {
    debug("Reply was empty. Likely an error response from the server.\n");
    return NULL;
  }

  if (reply->type != request->type) {
    debug("Bad reply type: %s (%d).\n", strmsgtype(reply->type), reply->type);
    nn_freemsg(reply);
    return NULL;
  }

  /**
   * TODO: Should also check that the server's reply is well-formed as far as
   * its size goes; we don't want the client reading bytes that don't exist. For
   * now, we just trust the server to send valid messages.
   */
  debug("%s request was successful!\n", strmsgtype(request->type));
  return reply;
}

/**
 * Looks up the `path` at the SNFS server and sets `handle` to the path's handle
 * if it is found. It does this by first checking a local cache. If the cache is
 * consistent, no further network requests are done. If the cache appears to be
 * inconsistent, iterative LOOKUP requests for each component of the `path` are
 * sent to the server.
 *
 * @param path the path to lookup
 * @param handle a pointer to where to set the handle if it is found
 *
 * @return true if the lookup is successful (handle is found), false otherwise
 */
bool cached_lookup(const char *path, fhandle *handle) {
  UNUSED(path);
  UNUSED(handle);

  // TODO: Implement me for extra credit.

  return false;
}

/**
 * Looks up the `path` at the SNFS server and sets `handle` to the path's
 * handle if it is found. It does this by iteratively sending LOOKUP requests
 * for each component of the `path`.
 *
 * @param path the path to lookup
 * @param handle a pointer to where to set the handle if it is found
 *
 * @return true if the lookup is successful (handle is found), false otherwise
 */
bool lookup(const char *path, fhandle *handle) {
  assert(handle);
  assert(path);

  verbose(STATE->options.verbose, "Looking up %s.\n", path);

  // Return root handle, if applicable
  if (!strcmp(path, "/")) {
    *handle = STATE->root_fhandle;
    return true;
  }

  fhandle cur_handle = STATE->root_fhandle;

  // Make a copy of the path in order to tokenize by "/" delimiter
  char path_copy[strlen(path) + 1];
  strcpy(path_copy, path);

  // Prepare lookup request and reply object
  snfs_req request = make_request(LOOKUP, /* fill in later */);
  snfs_lookup_args *args = &request.content.lookup_args;
  snfs_rep *reply;

  char *filename;
  filename = strtok(path_copy, "/");
  while (filename != NULL) {
    // Skip empty spaces, which can occur due to "//"
    if (strlen(filename) == 0) {
      filename = strtok(NULL, "/");
      continue;
    }

    args->dir = cur_handle;
    memset(args->filename, '\0', sizeof(uint8_t) * SNFS_MAX_FILENAME_BUF);
    memcpy(args->filename, (uint8_t *)filename,
           sizeof(uint8_t) * SNFS_MAX_FILENAME_LENGTH);

    reply = send_request(&request, snfs_req_size(lookup));
    if (!reply) {
      return false;
    }

    cur_handle = reply->content.lookup_rep.handle;
    nn_freemsg(reply);

    filename = strtok(NULL, "/");
  }

  *handle = cur_handle;
  return true;
}

/**
 * The FUSE getattr callback.
 *
 * Sets file attributes. The 'stat' structure is described in detail in the
 * stat(2) manual page. For the given pathname, this should fill in the elements
 * of the 'stat' structure.
 *
 * @param path the pathname of the file
 * @param stbuf the stat structure being filled in
 *
 * @return 0 on success, < 0 (a -errno error code) otherwise
 */
int snfs_getattr(const char *path, struct stat *stbuf) {
  verbose(STATE->options.verbose, "-- GETATTR START: %s\n", path);

  fhandle handle;
  if (!lookup(path, &handle)) {
    return -ENOENT;
  }

  snfs_req request = make_request(GETATTR, .getattr_args = {.fh = handle});
  snfs_rep *reply = send_request(&request, snfs_req_size(getattr));
  if (!reply) {
    return -EIO;
  }

  fattr_to_stat(&reply->content.getattr_rep.attributes, stbuf);

  nn_freemsg(reply);

  return 0;
}

/**
 * The FUSE readdir callback.
 *
 * Fills in the `buf` structure with `struct dirents` directory entries in
 * directory at `path`. This should not be done directly. Instead, the passed in
 * `filler` function pointer should be used: filler(buf, filename, NULL, 0);
 *
 * From the Harvey Mudd CS135 documentation:
 *   From FUSE's point of view, the offset is an uninterpreted off_t (i.e., an
 *   unsigned integer). You provide an offset when you call filler, and it's
 *   possible that such an offset might come back to you as an argument later.
 *   Typically, it's simply the byte offset (within your directory layout) of
 *   the directory entry, but it's really up to you.
 *
 * For most purposes, simply setting the offset to 0 each time is fine.
 *
 * @param path the path to the directory
 * @param buf the buffer to be filled in with entries
 * @param filler the FUSE filler function
 * @param offset conditionally used by FUSE - can be ignored here
 * @param fi the FUSE file information
 *
 * @return 0 on success, < 0 (a -errno) on error
 */
int snfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
  UNUSED(offset);
  UNUSED(fi);

  fhandle handle;
  if (!lookup(path, &handle)) {
    return -ENOENT;
  }

  snfs_req request = make_request(
      READDIR,
      .readdir_args = {
          .dir = handle,
          .count =
              256,  // Max number of directories supported...kind of foolish
      });

  // Send off the readdir request and ensure it's valid
  snfs_rep *reply = send_request(&request, snfs_req_size(readdir));
  if (!reply) {
    return -EIO;
  }

  // Parse out the filenames and fill them in
  snfs_readdir_rep *content = &reply->content.readdir_rep;
  for (uint64_t i = 0; i < content->num_entries; ++i) {
    const char *filename = (const char *)content->entries[i].filename;
    debug("Adding entry: %s\n", filename);
    filler(buf, filename, NULL, 0);
  }

  // Cleanup
  nn_freemsg(reply);
  return 0;
}

/**
 * The FUSE open callback.
 *
 * Opens a file for use by later operations. Checks that `path` is a valid file
 * and sets `fi->fh` to be the file handle for the file.
 *
 * @param path the path to the file
 * @param[out] fi the FUSE file information; fi->fh is set to the file handle
 *
 * @return 0 on success, < 0 (a -errno) otherwise
 */
int snfs_open(const char *path, struct fuse_file_info *fi) {
  assert(path);
  assert(fi);

  fhandle handle;
  if (!lookup(path, &handle)) {
    return -ENOENT;
  }
  fi->fh = handle;

  return 0;
}

/**
 * The FUSE read callback.
 *
 * Reads `count` bytes into `buf` for the file referred to by the handle at
 * `fi->fh` beginning at `offset`.
 *
 * @param path the path to the file; should be unused. handle is in fi->fh
 * @param buf the buffer to read into
 * @param count the maximum number of bytes to read
 * @param offset the offset to start reading at
 * @param the FUSE file info. fi->fh is the file handle
 *
 * @return the number of bytes read or < 0 (-errno) on error
 */
int snfs_read(const char *path, char *buf, size_t count, off_t offset,
              struct fuse_file_info *fi) {
  verbose(STATE->options.verbose, "-- READ: '%s', %" PRIu64 " ", path, fi->fh);
  verbose(STATE->options.verbose, "[%" PRId64 ":%" PRId64 "\n", offset,
          count + offset);

  snfs_req request = make_request(READ, .read_args = {
                                            .file = fi->fh,
                                            .offset = offset,
                                            .count = count,
                                        });

  snfs_rep *reply = send_request(&request, snfs_req_size(read));
  if (!reply) {
    return -EIO;
  }

  // Actually copy over the contents and return the number of bytes copied
  snfs_read_rep *content = &reply->content.read_rep;
  size_t to_copy = min(content->count, count);
  memcpy(buf, content->data, to_copy);

  nn_freemsg(reply);
  return to_copy;
}

/**
 * The FUSE write callback.
 *
 * Writes `count` bytes from `buf` into the file referred to by the handle at
 * `fi->fh` beginning at `offset`. According to the FUSE documentation, this
 * function should return exactly the number of bytes requested except when
 * there is an error.
 *
 * @param path the path to the file; should be unused. handle is in fi->fh
 * @param buf the buffer to write bytes from
 * @param count the number of bytes to write
 * @param offset the offset to start writing to
 * @param the FUSE file info. fi->fh is the file handle
 *
 * @return exactly the number of bytes requested or something else on error
 */
int snfs_write(const char *path, const char *buf, size_t count, off_t offset,
               struct fuse_file_info *fi) {
  assert(path);
  assert(buf);
  assert(fi);

  verbose(STATE->options.verbose, "-- WRITE: '%s', %" PRIu64 " ", path, fi->fh);
  verbose(STATE->options.verbose, "[%" PRId64 ":%" PRId64 "]\n", offset,
          count + offset);

  size_t request_size = snfs_req_size(write) + count;
  snfs_req *request = (snfs_req *)malloc(request_size);
  assert_malloc(request);

  request->type = WRITE;
  request->content.write_args.file = fi->fh;
  request->content.write_args.offset = offset;
  request->content.write_args.count = count;
  memcpy(request->content.write_args.data, buf, count);

  snfs_rep *reply = send_request(request, request_size);
  if (!reply) {
    free(request);
    return -EIO;
  }

  uint64_t bytes_written = reply->content.write_rep.count;
  free(request);
  nn_freemsg(reply);
  return bytes_written;
}

/**
 * Sets the remote attributes for the file at `path` flagged in `which`. Any
 * attribute not flagged is ignored.
 *
 * @param path the path to the file
 * @param which which attributes to set for the file
 * @param size the size to set if SNFS_SETSIZE is set in `which`
 * @param mode mode to set if SNFS_SETMODE is in which
 * @param uid uid to set if SNFS_SETUID is in which
 * @param gid gid to set if SNFS_SETGID is in which
 * @param tv access and mod time ([0], [1]) to set if SNFS_SETTIMES is in which
 *
 * @return 0 on success, < 0 (-errno or -1) otherwise
 */
int snfs_setattr(const char *path, uint64_t which, off_t size, mode_t mode,
                 gid_t uid, uid_t gid, const struct timespec tv[2]) {
  verbose(STATE->options.verbose, "-- SETATTR: '%s', %" PRIX64 "\n", path,
          which);

  fhandle handle;
  if (!lookup(path, &handle)) {
    debug("-- SETATTR lookup failed for %s\n", path);
    return -ENOENT;
  }

  snfs_timeval atime = {0, 0};
  snfs_timeval mtime = {0, 0};

  if (tv) {
    atime.seconds = tv[0].tv_sec;
    atime.useconds = tv[0].tv_nsec * 1000;
    mtime.seconds = tv[1].tv_sec, mtime.useconds = tv[1].tv_nsec * 1000;
  }

  snfs_req request =
      make_request(SETATTR, .setattr_args = {.file = handle,
                                             .which = (uint64_t)which,
                                             .size = (uint64_t)size,
                                             .mode = (uint64_t)mode,
                                             .uid = (uint64_t)uid,
                                             .gid = (uint64_t)gid,
                                             .atime = atime,
                                             .mtime = mtime});

  snfs_rep *reply = send_request(&request, snfs_req_size(setattr));
  if (!reply) {
    return -EIO;
  }

  bool success = (reply->content.setattr_rep.which == which);
  if (!success) {
    debug("Server failed to setattr!\n");
  }

  return (success) ? 0 : -1;
}

/**
 * The FUSE truncate callback.
 *
 * Sets the size attribute of the file at `path` to `size`.
 *
 * @param path the path to the file
 * @param size the size to set
 *
 * @return 0 on success, < 0 (-errno) on error
 */
int snfs_truncate(const char *path, off_t size) {
  verbose(STATE->options.verbose, "-- TRUNCATE: '%s', %" PRIu64 "\n", path,
          size);
  return snfs_setattr(path, SNFS_SETSIZE, size, 0, 0, 0, 0);
}

/**
 * The FUSE chmod callback.
 *
 * Sets the mode attribute of the file at `path` to `mode`.
 *
 * @param path the path to the file
 * @param mode the mode to set
 *
 * @return 0 on success, < 0 (-errno) on error
 */
int snfs_chmod(const char *path, mode_t mode) {
  verbose(STATE->options.verbose, "-- CHMOD: '%s', o%o\n", path, mode);
  return snfs_setattr(path, SNFS_SETMODE, 0, mode, 0, 0, 0);
}

/**
 * The FUSE chown callback.
 *
 * Sets the uid and gid attributes of the file at `path` to `uid` and `gid`.
 *
 * @param path the path to the file
 * @param uid the uid to set
 * @param gid the gid to set
 *
 * @return 0 on success, < 0 (-errno) on error
 */
int snfs_chown(const char *path, uid_t uid, gid_t gid) {
  verbose(STATE->options.verbose, "-- CHOWN: '%s', (%d, %d)\n", path, uid, gid);
  uint64_t which = 0;

  if ((int)uid != -1) which |= SNFS_SETUID;
  if ((int)gid != -1) which |= SNFS_SETGID;

  return snfs_setattr(path, which, 0, 0, uid, gid, 0);
}

/**
 * The FUSE utimens callback.
 *
 * Sets the atime and mtime attributes of the file at `path` to tv[0] and tv[1].
 *
 * @param path the path to the file
 * @param tv an array of two timespec structs: access and mod time, respectively
 *
 * @return 0 on success, < 0 (-errno) on error
 */
int snfs_utimens(const char *path, const struct timespec tv[2]) {
  verbose(STATE->options.verbose, "-- UTIME: '%s', (%ld.%ld), (%ld.%ld)\n",
          path, tv[0].tv_sec, tv[0].tv_nsec, tv[1].tv_sec, tv[1].tv_nsec);
  return snfs_setattr(path, SNFS_SETTIMES, 0, 0, 0, 0, tv);
}

/**
 * The FUSE create callback
 *
 * Create and open a file. If the file does not exist, first create it with the
 * specified mode, and then open it.
 */
int snfs_create(const char *path, mode_t mode, ffi *fi) {
  verbose(STATE->options.verbose, "Creating file %s.\n", path);

  if (!strcmp(path, "/")) {
    print_err("Cannot create a file that is the mounted root handle");
    return -ENOENT;
  }

  // Prepare create request and reply object
  snfs_req request = make_request(CREATE, /* fill in later */);
  snfs_create_args *args = &request.content.create_args;
  args->mode = (mode_t)mode;
  memset(args->filename, '\0', sizeof(uint8_t) * SNFS_MAX_FILENAME_BUF);
  memcpy(args->filename, (uint8_t *)path,
         sizeof(uint8_t) * SNFS_MAX_FILENAME_LENGTH);

  snfs_rep *reply;
  reply = send_request(&request, snfs_req_size(create));
  if (!reply) {
    return -ENOENT;
  }

  fi->fh = reply->content.create_rep.handle;

  nn_freemsg(reply);
  return 0;
}

/**
 * The FUSE unlink callback
 *
 * Remove a file
 */
int snfs_unlink(const char *path) {
  assert(path);

  fhandle handle;
  if (!lookup(path, &handle)) {
    return -ENOENT;
  }
  snfs_req request =
      make_request(REMOVE, .remove_args = {.fh = handle, .is_dir = 0});
  snfs_rep *reply = send_request(&request, snfs_req_size(remove));
  if (!reply) {
    return -ENOENT;
  }

  nn_freemsg(reply);
  return 0;
}

/**
 * The FUSE rename callback
 *
 * Rename a file
 */
int snfs_rename(const char *oldpath, const char *newpath) {
  verbose(STATE->options.verbose, "Renaming file from  %s to %s.\n", oldpath,
          newpath);
  fhandle handle;
  if (!lookup(oldpath, &handle)) {
    return -ENOENT;
  }

  // Prepare rename request and reply object
  snfs_req request = make_request(RENAME, /* fill in later */);
  snfs_rename_args *args = &request.content.rename_args;
  args->fh = handle;

  memset(args->filename, '\0', sizeof(uint8_t) * SNFS_MAX_FILENAME_BUF);
  memcpy(args->filename, (uint8_t *)newpath,
         sizeof(uint8_t) * SNFS_MAX_FILENAME_LENGTH);

  snfs_rep *reply;
  reply = send_request(&request, snfs_req_size(rename));
  if (!reply) {
    return -ENOENT;
  }

  nn_freemsg(reply);
  return 0;
}

/**
 * The FUSE release callback
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.	 It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 */
int snfs_release(const char *path, ffi *fi) {
  UNUSED(path);
  UNUSED(fi);

  return 0;
}

/**
 * The FUSE opendir callback
 *
 * Unless the 'default_permissions' mount option is given,
 * this method should check if opendir is permitted for this
 * directory. Optionally opendir may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to readdir, releasedir and fsyncdir.
 */
int snfs_opendir(const char *path, ffi *fi) {
  fhandle handle;
  if (!lookup(path, &handle)) {
    return -ENOENT;
  }
  fi->fh = handle;

  return 0;
}

/**
 * The FUSE mkdir callback
 *
 * Note that the mode argument may not have the type specification
 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
 * correct directory type bits use  mode|S_IFDIR
 */
int snfs_mkdir(const char *path, mode_t mode) {
  verbose(STATE->options.verbose, "Creating directory %s.\n", path);

  if (!strcmp(path, "/")) {
    print_err("Cannot mkdir a directory that is the mounted root handle");
    return -ENOENT;
  }

  // Prepare mkdir request and reply object
  snfs_req request = make_request(MKDIR, /* fill in later */);
  snfs_mkdir_args *args = &request.content.mkdir_args;
  args->mode = (mode_t)mode | S_IFDIR;
  memset(args->dirname, '\0', sizeof(uint8_t) * SNFS_MAX_FILENAME_BUF);
  memcpy(args->dirname, (uint8_t *)path,
         sizeof(uint8_t) * SNFS_MAX_FILENAME_LENGTH);

  snfs_rep *reply;
  reply = send_request(&request, snfs_req_size(mkdir));
  if (!reply) {
    return -ENOENT;
  }

  nn_freemsg(reply);
  return 0;
}

/**
 * The FUSE releasedir callback
 *
 * Release a directory
 */
int snfs_releasedir(const char *path, ffi *fi) {
  UNUSED(path);
  UNUSED(fi);

  return 0;
}

/**
 * The FUSE rmdir callback
 *
 * Remove a directory
 */
int snfs_rmdir(const char *path) {
  assert(path);

  fhandle handle;
  if (!lookup(path, &handle)) {
    return -ENOENT;
  }
  snfs_req request =
      make_request(REMOVE, .remove_args = {.fh = handle, .is_dir = 1});
  snfs_rep *reply = send_request(&request, snfs_req_size(remove));
  if (!reply) {
    return -ENOENT;
  }

  nn_freemsg(reply);
  return 0;
}