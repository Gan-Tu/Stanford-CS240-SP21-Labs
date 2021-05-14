#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#include "test.h"
#include "mock.h"
#include "helpers.h"
#include "common.h"
#include "server.h"
#include "client.h"

static void server_cleanup() {
  clear_servedir();
  stop_server(true);
  teardown_client();
}

static bool test_lookup() {
  check(start_server(true));
  check(setup_client());

  fhandle root;
  check(lookup("/", &root));

  fhandle root1;
  check(lookup("/", &root1));

  // Root should be the same
  check_eq(root, root1);

  // Try a bunch of nonexistent paths
  fhandle bad_handle = 0;
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_path(rand_string, SNFS_MAX_FILENAME_LENGTH);
    check(!lookup(rand_string, &bad_handle));
    check_eq(bad_handle, 0);
  }

  // Now try a bunch paths that actually exist
  fhandle good_handle = 0;
  for (int i = 0; i < 100; ++i) {
    gen_random_path(rand_string, SNFS_MAX_FILENAME_LENGTH - 128);
    create_file_at_path(rand_string);
    check(lookup(rand_string, &good_handle));
    check_neq(good_handle, 0);
  }

  // Make sure the root hasn't changed
  fhandle handle;
  check(server_name_find_or_insert("/", &handle));
  check_eq(root, handle);

  // delete all those files
  check(teardown_client());
  check(stop_server(true));
  return true;
}

/**
 * The phase 2 test suite. This function is declared via the BEGIN_TEST_SUITE
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
BEGIN_TEST_SUITE(phase2_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  clean_run_test(test_lookup, server_cleanup);
}
