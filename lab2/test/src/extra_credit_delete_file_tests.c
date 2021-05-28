#define FUSE_USE_VERSION 26
#include <errno.h>
#include <fuse.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "client.h"
#include "common.h"
#include "helpers.h"
#include "mock.h"
#include "server.h"
#include "test.h"

// TODO(tugan): add tests

static void server_cleanup() {
  stop_server(true);
  teardown_client();
}

static bool test_delete() {
  // Make sure we can create and remove a database.
  check(start_server(true));
  check(setup_client());

  struct fuse_file_info fi;
  fi.fh = 1;      

  // As always, we start with nonexisting files
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    check_eq(snfs_open(rand_string, &fi), -ENOENT);
  }

  // Create some files, then delete them
  fhandle handle;
  for (int i = 0; i < 100; ++i) {
    // Create a file
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);

    // Open Success
    check(!snfs_open(rand_string, &fi));
    check(fi.fh > 0);

    // Delete the file
    check(!snfs_unlink(rand_string));
    
    // Handles cannot be looked up
    check(!lookup(rand_string, &handle));

    // Open failed
    check_eq(snfs_open(rand_string, &fi), -ENOENT);
  }

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

/**
 * The extra credit delete file test suite.
 * This function is declared via the BEGIN_TEST_SUITE macro for easy testing.
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
BEGIN_TEST_SUITE(extra_credit_delete_file_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  clean_run_test(test_delete, server_cleanup);
}
