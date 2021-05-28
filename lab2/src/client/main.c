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

// The name the program was invoked with.
static const char *PROG_NAME;

// The initial STATE -- only used to pass cmd line arguments to the FUSE init()
static client_state *INIT_STATE;

/**
 * Prints the usage information for this program.
 */
static void usage() {
  fprintf(stderr,
          "Usage: %s [OPTION]... <url> <dir>\n"
          "  <url>  URL for the Simple NFS Server\n"
          "  <dir>  mount point for remote directory\n"
          "\nOptions:\n"
          "  -d     start FUSE in debug mode\n"
          "  -h     give this help message\n"
          "  -v     print verbose output\n",
          PROG_NAME);
}

/**
 * Parses the command line, setting options in `opts` as necessary.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments.
 * @param[out] opts The options as set by the user via the command line.
 *
 * @return index in argv of the first argv-element that is not an option
 */
static int parse_command_line(int argc, char *argv[], client_options *opts) {
  assert(argc);
  assert(argv);
  assert(opts);

  // Zero out the options structure.
  memset(opts, 0, sizeof(client_options));

  /*
   * Don't have getopt print an error message when it finds an unknown option.
   * We'll take care of that ourselves by looking for '?'.
   */
  opterr = 0;

  // Parse the command line.
  int opt = '\0';
  while ((opt = getopt(argc, argv, "dhv")) != -1) {
    switch (opt) {
      case 'd':
        opts->fuse_debug = true;
        break;
      case 'h':
        usage();
        exit(EXIT_SUCCESS);
      case 'v':
        opts->verbose = true;
        break;
      case '?':
      default:
        usage_msg_exit("%s: Unknown option '%c'\n", PROG_NAME, optopt);
    }
  }

  return optind;
}

/**
 * The FUSE init callback. Initializes the file system.
 *
 * FUSE does very strange things. One of those things makes this function be
 * necessarily implemented. Something happens between a socket opening in
 * `client_main` and a file system callback that somehow obliterates the open
 * socket, rendering it useless. However, if that socket is opened here,
 * everything is okay! Perhaps it does a vfork() of some sort. Who knows!
 *
 * In any case, this function is important. It initiates the connection to the
 * SNFS server. If the connection fails, then this function exits: there's no
 * file system without the server. If the connection succeeds, it sends a MOUNT
 * request to the server to retrieve the root file handle. It is stored in the
 * STATE as `root_fhandle` to be used by file system callbacks. The server's
 * socket is stored in STATE's `server_sock`.
 *
 * Note that you won't see these, or any print messages unless FUSE is launched
 * in debug mode. Pass '-d' as a flag to this binary to enable it.
 *
 * @param conn FUSE supplied information about supported file system features
 *
 * @return FUSE private data, accessible later via the STATE macro
 */
static void *snfs_init(struct fuse_conn_info *conn) {
  UNUSED(conn);

  // Open up the socket to the server and test the connection
  INIT_STATE->server_sock = nn_socket(AF_SP, NN_REQ);
  test_connection(INIT_STATE->server_sock, INIT_STATE->server_url);

  // Send a MOUNT request to the server to get the root fhandle
  fhandle root;
  if (!server_mount(INIT_STATE->server_sock, INIT_STATE->server_url, &root)) {
    err_exit("Server mount failed. Check public/private keys.\n");
  }

  // Save it in the client's state
  INIT_STATE->root_fhandle = root;
  verbose(INIT_STATE->options.verbose, "Mounted! Root handle is %" PRIu64 "\n",
          root);
  printf("Connected to server at '%s'.", INIT_STATE->server_url);

  // Return the pointer which can later be accessed using STATE
  return INIT_STATE;
}

/**
 * The FUSE destroy callback. Cleans up FUSE private data.
 *
 * This function is called by FUSE when the file system is unmounted or quits
 * for some reason. Frees the resources used by the private_data allocated in
 * `init`.
 *
 * @param private_data the data returned by the `init` callback
 */
static void snfs_destroy(void *private_data) {
  client_state *state = (client_state *)private_data;

  // Free the memory used by STATE (initially INIT_STATE)
  if (state) {
    nn_close(state->server_sock);
    free((void *)state->server_url);
    free(state);
  }
}

/**
 * The fuse_operations structure containing the list of file system callbacks.
 */
static struct fuse_operations snfs_client_oper = {
    .getattr = snfs_getattr,
    .readdir = snfs_readdir,
    .open = snfs_open,
    .read = snfs_read,
    .write = snfs_write,
    .truncate = snfs_truncate,
    .chmod = snfs_chmod,
    .chown = snfs_chown,
    .utimens = snfs_utimens,
    .init = snfs_init,
    // Extra Credit below
    .destroy = snfs_destroy,
    .create = snfs_create,
    .unlink = snfs_unlink,
    .rename = snfs_rename,
    .opendir = snfs_opendir,
    .mkdir = snfs_mkdir,
    .releasedir = snfs_releasedir,
    .rmdir = snfs_rmdir,
};

/**
 * The client's main() function.
 *
 * Parses the command line arguments, determining the mount point and server
 * URL. Then, calls FUSE's main function to initialize the file system.
 *
 * @param argc number of parameters in argv
 * @param argv the command line parameters
 */
int client_main(int argc, char *argv[]) {
  PROG_NAME = argv[0];
  INIT_STATE = (client_state *)malloc(sizeof(client_state));
  assert_malloc(INIT_STATE);

  // Parse the command line and check that a string lives in argv
  int url_index = parse_command_line(argc, argv, &INIT_STATE->options);
  if (argc <= url_index) {
    usage_msg_exit("%s: <url> argument is required\n", PROG_NAME);
  }

  // Grab a copy of the server's URL
  INIT_STATE->server_url = strdup(argv[url_index]);

  // Check that there's a directory parameter, too
  int dir_index = url_index + 1;
  if (argc <= dir_index) {
    usage_msg_exit("%s: <dir> argument is required\n", PROG_NAME);
  }

  // Check that the string corresponds to a directory
  const char *dir_path = (const char *)argv[dir_index];
  DIR *mount_dir = opendir(dir_path);
  if (!mount_dir) {
    if (errno == ENOENT || errno == ENOTDIR) {
      usage_msg_exit("%s: '%s' is not a directory\n", PROG_NAME, dir_path);
    } else {
      usage_msg_exit("%s: <dir> error: %s\n", PROG_NAME, strerror(errno));
    }
  }

  // Okay, it exists. Close it.
  closedir(mount_dir);

  // Set up the fuse argc and argv
  int fargc = 5;
  const char *fargv[7] = {
      PROG_NAME, dir_path, "-s", "-o", "default_permissions", NULL, NULL};

  // Enable FUSE's debug mode if requested.
  if (INIT_STATE->options.fuse_debug) {
    fargv[fargc] = "-d";
    fargc += 1;
  }

  // Start up fuse!
  printf("SNFS mounting at '%s'.\n", dir_path);
  fflush(stdout);

  return fuse_main(fargc, (char **)fargv, &snfs_client_oper, INIT_STATE);
}
