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

static bool test_write_nothing() {
  check(start_server(true));
  check(setup_client());

  char buf[MAX_BYTES];
  struct fuse_file_info fi;
  fi.fh = 0;

  // Lets make a bunch of empty files and read (hopefully zero bytes) from them
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);

    check(!snfs_open(rand_string, &fi));
    check(fi.fh > 0);

    check_eq(snfs_write(rand_string, buf, 0, 0, &fi), 0);
    fi.fh = 0;
  }

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

static bool test_write_something() {
  // Make sure we can create and remove a database.
  check(start_server(true));
  check(setup_client());

  char readbuf[MAX_BYTES];
  char buf[MAX_BYTES];
  struct fuse_file_info fi;
  fi.fh = 0;

  // Now actually write to the file.
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);
    size_t size = rand() % MAX_BYTES;
    get_random(buf, size);

    check(!snfs_open(rand_string, &fi));
    check(fi.fh > 0);

    check_eq(snfs_write(rand_string, buf, size, 0, &fi), (int) size);
    check_eq(snfs_read(rand_string, readbuf, size, 0, &fi), (int) size);
    check(!memcmp(readbuf, buf, size));
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

  char buf[MAX_BYTES];
  char rand_string[SNFS_MAX_FILENAME_BUF];
  gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
  check_eq(snfs_write(rand_string, buf, 1024, 0, &fi), -EIO);

  check(teardown_client());
  return true;
}


/**
 * The phase 7 test suite. This function is declared via the BEGIN_TEST_SUITE
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
BEGIN_TEST_SUITE(phase7_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  clean_run_test(test_write_nothing, server_cleanup);
  clean_run_test(test_write_something, server_cleanup);
  clean_run_test(test_no_server, client_cleanup);
}
