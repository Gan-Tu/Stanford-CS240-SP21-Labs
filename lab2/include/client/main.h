#ifndef SNFS_CLIENT_MAIN_H
#define SNFS_CLIENT_MAIN_H

#include <stdbool.h>

#include "common.h"

/*
 * A convenience macro to generate a snfs_req structure. The first parameter is
 * the message type, i.e. GETATTR, READ, etc., and the second parameter is the
 * corresponding request structure.
 *
 * The following invocation creates a GETATTR request:
 * snfs_req request = make_request(GETATTR, .getattr_args = {
 *   .fh = 0
 * });
 */
#define make_request(request_type, ...) \
  { \
    .type = request_type, \
    .content = { \
      __VA_ARGS__ \
    } \
  }

/*
 * A convenience macro to generate the size of a given message.
 */
#define snfs_req_size(msg_type) \
  (sizeof(snfs_msg_type) + sizeof(snfs_##msg_type##_args))

typedef struct client_options_struct {
  // Whether we should print verbose messages.
  bool verbose;

  // Whether to start fuse in debug mode.
  bool fuse_debug;
} client_options;

typedef struct client_state_struct {
  // URL to server
  const char *server_url;

  // Open socket to the serverk
  int server_sock;

  // The root file handle
  fhandle root_fhandle;

  // Command line options
  client_options options;
} client_state;

int client_main(int, char *[]);

#include "mock.h"

#ifndef STATE
#define STATE ((client_state *) \
    ((MOCK_STATE) ? MOCK_STATE : fuse_get_context()->private_data))
#endif

#endif
