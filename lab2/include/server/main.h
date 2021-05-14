#ifndef SNFS_SERVER_MAIN_H
#define SNFS_SERVER_MAIN_H

#include <dirent.h>
#include <stdbool.h>

/*
 * A convenience macro to generate a snfs_rep structure. The first parameter is
 * the message type, i.e. GETATTR, ERROR, etc., and the second parameter is the
 * corresponding reply structure.
 *
 * The following invocation creates an error response:
 * snfs_rep reply = make_reply(ERROR, .error_rep = {
 *   .error = ENOTIMPL
 * });
 */
#define make_reply(reply_type, ...) \
  { \
    .type = reply_type, \
    .content = { \
      __VA_ARGS__ \
    } \
  }

/*
 * A convenience macro to generate the size of a given message.
 */
#define snfs_rep_size(msg_type) \
  (sizeof(snfs_msg_type) + sizeof(snfs_##msg_type##_rep))

typedef struct server_options_struct {
  // Whether we should print verbose messages.
  bool verbose;

  // The port to run on. Defaults to 2048.
  long port;
} server_options;

int server_main(int, char *[]);
void serve_loop(const char *url);

/*
 * Global variables defined below.
 */

// The directory being served
const char *MOUNT_PATH;
DIR *MOUNT_DIR;

/*
 * A value to use for dummy file handles when needed.
 */
#define SNFS_DUMMY_FH 0xDEADBEEF

#endif
