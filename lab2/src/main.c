/*
 * This file contains the main() function for both the server and the client
 * binaries. The Makefile sets either the SNFS_IS_CLIENT or SNFS_IS_SERVER
 * preprocessor macros which causes either the client_main or server_main
 * function, respectively, to be called by main(). This lets us call the client
 * and server's main function directly from testing harnesses if needed.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "common.h"
#include "client.h"
#include "server.h"

/**
 * The client's main function's caller.
 *
 * @param argc number of cmd line arguments
 * @param argv the cmd line arguments
 *
 * @return 0 on successful exit, nonzero otherwise
 */
#ifdef SNFS_IS_CLIENT
int main(int argc, char *argv[]) {
  return client_main(argc, argv);
}
#endif

/**
 * The server's main function's caller.
 *
 * @param argc number of cmd line arguments
 * @param argv the cmd line arguments
 *
 * @return 0 on successful exit, nonzero otherwise
 */
#ifdef SNFS_IS_SERVER
int main(int argc, char *argv[]) {
  return server_main(argc, argv);
}
#endif
