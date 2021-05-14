/* #define DEBUG */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <bsd/stdlib.h>
#endif

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include "test.h"
#include "common.h"
#include "server.h"
#include "client.h"
#include "mock.h"
#include "helpers.h"

// const char *MOUNT_DIR = "./test/mock/mount/";
pid_t server_pid = 0;

// Mocking the client state
client_state *MOCK_STATE = NULL;

static const char *URL = "tcp://localhost:2048";
static const char *SERVER_PORT = "2048";

bool start_server(bool fresh_db) {
  if (fresh_db) {
    clear_servedir();
  }

  fflush(stdout);
  fflush(stderr);
  if (!(server_pid = fork())) {
    int argc = 4;
    const char *argv[] = {
      "mock_snfs", "-p", (char *)SERVER_PORT, SERVE_DIR, NULL
    };

    server_main(argc, (char **)argv);
    exit(0);
  }

  debug("Launched the server: is %d and I'm %d\n", server_pid, getpid());
  return true;
}

bool stop_server(bool kill_db) {
  if (server_pid) {
    debug("Stopping [KILL] server...\n");

    int status;
    if (kill(server_pid, SIGTERM) < 0) {
      return false;
    }

    if (server_pid != waitpid(server_pid, &status, 0)) {
      return false;
    }

    while (waitpid(0, NULL, WNOHANG) >= 0) { /* just a loop */ };
    server_pid = 0;
  } else {
    debug("No server PID?\n");
  }

  // Get rid of it all!
  if (kill_db) {
    char *cwd = pushd(SERVE_DIR);
    destroy_db(true);
    popd(cwd);
    clear_servedir();
  }

  return true;
}

bool setup_client() {
  MOCK_STATE = (client_state *)malloc(sizeof(client_state));
  memset(MOCK_STATE, 0, sizeof(*MOCK_STATE));

  MOCK_STATE->server_url = strdup(URL);
  MOCK_STATE->options = (client_options) {
    .verbose = false,
    .fuse_debug = false
  };

  MOCK_STATE->server_sock = nn_socket(AF_SP, NN_REQ);
  test_connection(MOCK_STATE->server_sock, MOCK_STATE->server_url);

  // Send a MOUNT request to the server to get the root fhandle
  fhandle root = 0;
  if (!server_mount(MOCK_STATE->server_sock, MOCK_STATE->server_url, &root)) {
    return false;
  }

  // Save it in the client's state
  MOCK_STATE->root_fhandle = root;
  return true;
}

bool teardown_client() {
  if (MOCK_STATE) {
    nn_close(MOCK_STATE->server_sock);
    free((void *)MOCK_STATE->server_url);
    free(MOCK_STATE);
    return true;
  }

  return false;
}

bool start_mock_server() {
  return false;
}

bool stop_mock_server() {
  return false;
}

bool start_mock_client() {
  return false;
}

bool stop_mock_client() {
  return false;
}
