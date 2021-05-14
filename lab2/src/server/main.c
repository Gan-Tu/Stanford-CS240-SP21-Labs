/* #define DEBUG */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>

#ifdef __linux__
#include <bsd/stdlib.h>
#endif

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include "common.h"
#include "server.h"

// The name the program was invoked with.
static const char *PROG_NAME;

// The default TCP port to serve on.
static const int DEFAULT_PORT = 2048;

// Server options structure.
static server_options OPTIONS;

// Set to 1 when SIGTERM was received
static volatile sig_atomic_t terminated = 0;

/**
 * Prints the usage information for this program.
 */
static void usage() {
  fprintf(stderr,
    "Usage: %s [OPTION]... <dir>\n"
    "  <dir>  path to directory to serve via simple NFS\n"
    "\nOptions:\n"
    "  -h         give this help message\n"
    "  -p [port]  the TCP port to run on (defaults to 2048)\n"
    "  -v         print verbose output\n",
    PROG_NAME);
}

/**
 * Parses a number from a string that starts with a number. Returns 1 if that
 * number is the only text the string contains. Returns 0 if a number was parsed
 * but there is other text in the string. Returns -1 if no number could be
 * parsed.
 *
 * @param string The string to parse a number from.
 * @param number A pointer to where to store the parsed number, if any.
 *
 * @return 1 if only number was parsed, 0 if number plus other, -1 on no parse
 */
int parse_number(char *string, long *number) {
    char *end;
    long result = strtol(string, &end, 0);
    if (result == 0 && string == end) {
      // No number was parsed.
      return -1;
    } else if (errno == ERANGE && (result == LONG_MAX || result == LONG_MIN)) {
      // Number was out of range.
      return -1;
    }

    // We have a parse!
    *number = result;

    // Check if there is text after the number.
    if (*end) {
      return 0;
    }

    return 1;
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
static int parse_command_line(int argc, char *argv[], server_options *opts) {
  assert(argc);
  assert(argv);
  assert(opts);

  // Zero out the options structure.
  memset(opts, 0, sizeof(server_options));

  // Set the defaults.
  opts->port = DEFAULT_PORT;

  /*
   * Don't have getopt print an error message when it finds an unknown option.
   * We'll take care of that ourselves by looking for '?'.
   */
  opterr = 0;

  // Parse the command line.
  int opt = '\0';
  while ((opt = getopt(argc, argv, "hvp:")) != -1) {
    switch (opt) {
      case 'h':
        usage();
        exit(EXIT_SUCCESS);
      case 'v':
        opts->verbose = true;
        break;
      case 'p':
        if (parse_number(optarg, &opts->port) != 1) {
          usage_msg_exit("Error: Invalid [port] argument. Must be a number.");
        }

        break;
      case '?':
      default:
        if (optopt == 'p') {
          usage_msg_exit("Error: Option -p requires an argument.");
        }

        usage_msg_exit("%s: Unknown option '%c'\n", PROG_NAME, optopt);
    }

  }

  return optind;
}

/**
 * The main server loop.
 *
 * Waits for connections from clients, determines the type of request the client
 * is making, and dispatches the request to respective handlers, passing it the
 * request's parameters.
 *
 * @param url the nanomsg formatted URL the server should listen at
 */
void serve_loop(const char *url) {
  int sock = nn_socket(AF_SP, NN_REP);
  if (sock < 0) {
    err_exit("Failed to open socket: %s\n", strerror(errno));
  }

  assert(url);
  if (nn_bind(sock, url) < 0) {
    err_exit("Failed to bind with url '%s': %s\n", url, strerror(errno));
  }

  // okay, it all checks out. Let's loop, waiting for a message.
  verbose(OPTIONS.verbose, "SNFS serving '%s' on %s...\n", MOUNT_PATH, url);

  snfs_req *req = NULL;
  while (!terminated) {
    int bytes = nn_recv(sock, &req, NN_MSG, 0);
    if (bytes < 0) break;

    snfs_msg_type msg_type = ERROR;
    if ((size_t) bytes >= sizeof(snfs_msg_type)) {
      msg_type = req->type;
    }

    debug("Received '%s' request.\n", strmsgtype(msg_type));
    switch (req->type) {
      case NOOP:
        handle_noop(sock);
        break;
      case MOUNT:
        handle_mount(sock);
        break;
      case GETATTR:
        handle_getattr(sock, &req->content.getattr_args);
        break;
      case READDIR:
        handle_readdir(sock, &req->content.readdir_args);
        break;
      case LOOKUP:
        handle_lookup(sock, &req->content.lookup_args);
        break;
      case READ:
        handle_read(sock, &req->content.read_args);
        break;
      case WRITE:
        handle_write(sock, &req->content.write_args);
        break;
      case SETATTR:
        handle_setattr(sock, &req->content.setattr_args);
        break;
      default:
        handle_unimplemented(sock, msg_type);
        break;
    }

    nn_freemsg(req);
    nn_shutdown(sock, 0);
  }

  // Check if we were terminated or simply failed
  if (terminated) {
    debug("Terminating...\n");
    verbose(OPTIONS.verbose, "Recevied SIGTERM. Terminating.\n");
    fflush(stdout);
  } else {
    print_err("Server failed to recv()!\n");
    fflush(stderr);
  }

  // Cleanup
  nn_shutdown(sock, 0);
  nn_close(sock);
  destroy_db(false);
  exit(0);
}

/**
 * The SIGTERM signal handler. Simply sets the 'terminated' variable to 1 to
 * inform the serve loop it should exit.
 *
 * @param sig the signal received
 */
static void sigterm_handler(int sig) {
  UNUSED(sig);
  terminated = 1;
  nn_term();
}

/*
 * The server's main() function.
 *
 * Parses the command line arguments to determine the path to the directory
 * being served over SNFS. Once the path has been determined to be valid, main()
 * chroots into the directory for safe processing of path strings in the future.
 *
 * @param argc number of cmd line arguments
 * @param argv the cmd line arguments
 *
 * @return 0 on successful exit, nonzero otherwise
 */
int server_main(int argc, char *argv[]) {
  PROG_NAME = argv[0];

  // Parse the command line and check that a string lives in argv
  int dir_index = parse_command_line(argc, argv, &OPTIONS);
  if (argc <= dir_index) {
      usage_msg_exit("%s: <dir> argument is required\n", PROG_NAME);
  }

  // Grab the directory path from argv
  MOUNT_PATH = (const char *)argv[dir_index];

  // Check that the string corresponds to a directory
  MOUNT_DIR = opendir(MOUNT_PATH);
  if (!MOUNT_DIR) {
    if (errno == ENOENT || errno == ENOTDIR) {
      usage_msg_exit("%s: '%s' is not a directory\n", PROG_NAME, MOUNT_PATH);
    } else {
      usage_msg_exit("%s: <dir> error: %s\n", PROG_NAME, strerror(errno));
    }
  }

  // Stir up the randomness before jailing
  arc4random_stir();

  // jailing the server to the directory being served
  char full_path[PATH_MAX];
  if (!realpath(MOUNT_PATH, full_path)) {
    err_exit("Couldn't resolve '%s': %s", MOUNT_PATH, strerror(errno));
  }

  if (chdir(full_path) < 0) {
    err_exit("Couldn't chdir to '%s': %s", full_path, strerror(errno));
  }

  if (chroot(full_path)) {
    err_exit("Could not jail server to %s: %s\n", full_path, strerror(errno));
  }

  // Attempt to initialize the fhandle database
  init_db_if_needed();

  // Make sure we catch SIGTERM to clean up
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = sigterm_handler;
  sigaction(SIGTERM, &act, NULL);

  // tcp://*: has 8 chars, ceil(log_10(2^64)) == 20, + 1 ('\0')
  char tcp_url[29];

  // Set the URL according to the options, and start serving
  snprintf(tcp_url, 29, "tcp://*:%ld", OPTIONS.port);
  serve_loop(tcp_url);

  return 0;
}
