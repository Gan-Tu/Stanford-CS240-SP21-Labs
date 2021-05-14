/* #define DEBUG */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#ifdef __linux__
#include <bsd/stdlib.h>
#endif

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include "common.h"

/**
 * Returns the current time in milliseconds.
 *
 * @return the current time in milliseconds
 */
long long current_ms() {
  struct timeval te;
  gettimeofday(&te, NULL);
  return (long long) te.tv_sec * 1000 + te.tv_usec / 1000;
}

/**
 * Fills a buffer `buf` with `bytes` number of random bytes.
 *
 * @param buf the buffer to fill
 * @param bytes the number of bytes to fill the buffer with
 */
void get_random(void *buf, size_t bytes) {
  assert(buf);
  arc4random_buf(buf, bytes);
}

/**
 * Sends `size` bytes from the buffer of `data` to the machine referred to by
 * `sock`. If `flags` == NN_DONTWAIT, this call blocks for at most about 500ms.
 *
 * @param sock the socket endpoint
 * @param data the data to send
 * @param size the number of bytes in `data`
 * @param flags if NN_DONTWAIT, blocks for at most ~500ms, else blocks until
 *              message is sent.
 *
 * @return number of bytes sent on success, < 0 on error
 */
int send_data(int sock, void *data, size_t size, int flags) {
  debug("Sending (flags: %d) %zu bytes of data (%p):\n", flags, size, data);
  if_debug { printbuf(data, size); }

  // Try for half a second to send the data.
  int bytes = 0;
  long long end_time = current_ms() + 550;

  do {
    if ((bytes = nn_send(sock, data, size, flags)) >= 0) break;

    switch (errno) {
      case EFAULT:
      case EBADF:
      case ENOTSUP:
      case ETERM:
        print_err("Send failed: '%s'\n", strerror(errno));
        return bytes;
      default:
        usleep(300);
    }
  } while (current_ms() < end_time);

  // Check that the second was successful
  if (bytes != (int) size) {
    print_err("Send failed: incorrect byte count.\n");
    return -1;
  }

  return bytes;
}

/**
 * Receives data from the machine referred to by `sock`. If `flags` ==
 * NN_DONTWAIT, this call blocks for at most about 1.25s. Returns a malloc()d
 * reply if it was received. It is the callers responsibility to free it.
 *
 * @param[out] size size in bytes of the received data
 * @param flags if NN_DONTWAIT, blocks for at most ~1.25s, else blocks until
 *              message is received
 *
 * @return malloc()d reply is it was received, NULL otherwise
 */
void *receive_data(int sock, size_t *size, int flags) {
  debug("Attempting to receive data (flags: %d)\n", flags);

  // Try for a little over a second to receive data
  int bytes;
  long long end_time = current_ms() + 1250;
  void *data = NULL;

  do {
    if ((bytes = nn_recv(sock, &data, NN_MSG, flags)) >= 0) break;

    if (errno == EBADF || errno == ENOTSUP || errno == ETERM) {
      print_err("Receive failed: '%s'\n", strerror(errno));
      return NULL;
    }

    usleep(250);
  } while (current_ms() < end_time);

  // If response is NULL, we didn't get a successful receive
  if (!data) {
    print_err("Receive failed: timed out (%d bytes).\n", bytes);
    return NULL;
  }

  // All is well. Set `size` if it was passed in.
  if (size) *size = bytes;
  debug("Received %d bytes of data:\n", bytes);
  if_debug { printbuf(data, bytes); }

  return data;
}
