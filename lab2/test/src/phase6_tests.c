#define FUSE_USE_VERSION 26
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <fuse.h>

#include "test.h"
#include "mock.h"
#include "helpers.h"
#include "common.h"
#include "server.h"
#include "client.h"

static const int MAX_BYTES = 1024;

static void server_cleanup() {
  stop_server(true);
  teardown_client();
}

static void client_cleanup() {
  teardown_client();
}

static bool test_read_empty() {
  check(start_server(true));
  check(setup_client());

  char readbuf[MAX_BYTES];
  struct fuse_file_info fi;
  fi.fh = 0;

  // Lets make a bunch of empty files and read (hopefully zero bytes) from them
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);

    check(!snfs_open(rand_string, &fi));
    check(fi.fh > 0);

    size_t asked = rand() % MAX_BYTES;
    check_eq(snfs_read(rand_string, readbuf, asked, 0, &fi), 0);
    fi.fh = 0;
  }

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

static bool test_read_something() {
  // Make sure we can create and remove a database.
  check(start_server(true));
  check(setup_client());

  char *written;
  char readbuf[MAX_BYTES];
  struct fuse_file_info fi;
  fi.fh = 0;

  // Now actually write to the file.
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);
    off_t size = write_rand_to(rand_string, MAX_BYTES, &written);

    check(!snfs_open(rand_string, &fi));
    check(fi.fh > 0);

    check_eq(snfs_read(rand_string, readbuf, size, 0, &fi), size);
    check(!memcmp(readbuf, written, size));
    free(written);
    fi.fh = 0;
  }

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

static bool test_no_server() {
  // Start the server, then kill it.
  check(start_server(true));
  check(setup_client());
  check(stop_server(true));

  struct fuse_file_info fi;
  fi.fh = rand();

  char readbuf[MAX_BYTES];
  char rand_string[SNFS_MAX_FILENAME_BUF];
  gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
  check_eq(snfs_read(rand_string, readbuf, 1024, 0, &fi), -EIO);

  check(teardown_client());
  return true;
}


/**
 * The phase 6 test suite. This function is declared via the BEGIN_TEST_SUITE
 * macro for easy testing.
 *
 * A test suite function takes three parameters as pointers: the result, the
 * number of tests, and the number of tests passed. The test suite function
 * should increment the number of tests each time a test is run and the number
 * of tests passed when a test passes. If all tests pass, result should be set
 * to true. If any of the tests fail, result should be set to false.
 *
 * The helper macro/function run_test can manage the pointers' state when the
 * suite function is declared using the appropriate parameter names as is done
 * by BEGIN_TEST_SUITE.
 *
 * @param[out] _result Set to true if all tests pass, false otherwise.
 * @param[out] _num_tests Set to the number of tests run by this suite.
 * @param[out] _num_passed Set to the number of tests passed during this suite.
 */
BEGIN_TEST_SUITE(phase6_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  clean_run_test(test_read_empty, server_cleanup);
  clean_run_test(test_read_something, server_cleanup);
  clean_run_test(test_no_server, client_cleanup);
}
