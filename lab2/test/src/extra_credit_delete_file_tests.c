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

static bool test_noop() {
  // setup client sends two NOOPS
  check(start_server(true));
  // will call err_exit (failing the test) if server doesn't reply to NOOPs
  setup_client();

  // Okay, done
  check(teardown_client());
  check(stop_server(true));
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
  clean_run_test(test_noop, server_cleanup);
}
