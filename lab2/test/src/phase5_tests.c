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

static void server_cleanup() {
  stop_server(true);
  teardown_client();
}

static bool test_open() {
  // Make sure we can create and remove a database.
  check(start_server(true));
  check(setup_client());

  struct fuse_file_info fi1;
  fi1.fh = 1;

  struct fuse_file_info fi2;
  fi2.fh = 2;

  // As always, we start with nonexisting files
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_path(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    check_eq(snfs_open(rand_string, &fi1), -ENOENT);
    check_eq(fi1.fh, 1);
  }

  // Now check that things that exist get opened correctly
  for (int i = 0; i < 100; ++i) {
    gen_random_path(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);

    // Once...
    check(!snfs_open(rand_string, &fi1));
    check(fi1.fh > 0);
    check_neq(fi1.fh, fi2.fh);

    // Twice...
    check(!snfs_open(rand_string, &fi2));
    check(fi2.fh > 0);
    check_eq(fi1.fh, fi2.fh);
  }

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

/**
 * The phase 5 test suite. This function is declared via the BEGIN_TEST_SUITE
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
BEGIN_TEST_SUITE(phase5_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  clean_run_test(test_open, server_cleanup);
}
