/* #define DEBUG */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include "common.h"
#include "client.h"

/**
 * Sends a request and waits for a reply. If flags is zero, we wait forever for
 * a reply. If flags == NN_DONTWAIT, we wait about a second and then return
 * NULL. Returns the reply if there was one and it wasn't an ERROR.
 *
 * @param sock the socket to send the request to
 * @param url the url to send the request to
 * @param request the request to send
 * @param size the size of the request
 * @param flags flags == NN_DONTWAIT, we wait about a second for a response,
 *              otherwise, the wait could be forever
 *
 * @return reply is successful, NULL otherwise
 */
snfs_rep *snfs_req_rep_f(int sock, const char *url, snfs_req *request,
    size_t size, int flags) {
  assert(sock >= 0);
  assert(url);
  assert(request);

  debug("Sending request '%s' to '%s'\n", strmsgtype(request->type), url);

  // Connect the socket
  int endpoint = nn_connect(sock, url);
  if (endpoint < 0) {
    print_err("Socket connection to '%s' failed: %s\n", url, strerror(errno));
    return NULL;
  }

  // Send the request. If it fails, close the connection and return NULL.
  if (send_data(sock, request, size, flags) < 0) {
    nn_shutdown(sock, endpoint);
    return NULL;
  }

  // Wait for a reply. Close the connection regardless of success.
  snfs_rep *reply = receive_data(sock, NULL, flags);
  nn_shutdown(sock, endpoint);

  // No reply? Well, okay. Return NULL.
  if (!reply) {
    return NULL;
  }

  // Check if it's an error and return NULL if it is
  if (reply->type == ERROR) {
    snfs_error error = reply->content.error_rep.error;
    debug("Server Returned Error: %s\n", strsnfserror(error));
    nn_freemsg(reply);
    return NULL;
  }

  return reply;
}

/**
 * The same as snfs_req_rep, but always waits forever for a response.
 *
 * @param sock the socket to send the request to
 * @param url the url to send the request to
 * @param req the request to send
 * @param size the size of the request
 *
 * @return reply is successful, NULL otherwise
 */
snfs_rep *snfs_req_rep(int sock, const char *url, snfs_req *req, size_t size) {
  return snfs_req_rep_f(sock, url, req, size, 0);
}

/**
 * Tests the connection to the SNFS server by sending two NOOP requests and
 * waiting for the responses.
 *
 * @param sock the socket to send the request to
 * @param url the url to send the request to
 */
void test_connection(int sock, const char *url) {
  snfs_rep *rep;
  snfs_req noop = make_request(NOOP,);

  debug("Sending test packet...\n");
  rep = snfs_req_rep_f(sock, url, &noop, snfs_req_size(noop), NN_DONTWAIT);
  if (!rep) {
    err_exit("Could not connect to server at '%s'.\n", url);
  }

  nn_freemsg(rep);
  debug("Reply received. Sending second test packet...\n");
  if (!(rep = snfs_req_rep(sock, url, &noop, snfs_req_size(noop)))) {
    err_exit("Could not connect to server at '%s'.\n", url);
  }

  // Cleanup
  nn_freemsg(rep);
  debug("Reply received.\n");
}

/**
 * Sends a MOUNT request to the server. If the server responds successfully,
 * *root is set to the root file handle if `root` is not NULL.
 *
 * @param sock the socket to send the request to
 * @param url the url to send the request to
 * @param[out] root a pointer to set to the root file handle
 *
 * @return true if the root file handle was retrieved, false otherwise
 */
bool server_mount(int sock, const char *url, fhandle *root) {
  UNUSED(sock);
  UNUSED(url);
  UNUSED(root);

  // FIXME: Create, send, and wait for reply indefinitely for MOUNT request.

  return false;
}
