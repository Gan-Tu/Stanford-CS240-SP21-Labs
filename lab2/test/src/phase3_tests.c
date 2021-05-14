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

static bool test_getattr() {
  // Make sure we can create and remove a database.
  check(start_server(true));
  check(setup_client());

  // Our mighty stat structure
  struct stat st;

  // Make sure we get -ENOENT for nonexistent paths
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_path(rand_string, SNFS_MAX_FILENAME_LENGTH);
    check_eq(snfs_getattr(rand_string, &st), -ENOENT);
  }

  // Try a bunch of empty and nonempty files and directories
  struct stat real_st;
  for (int i = 0; i < 100; ++i) {
    gen_random_path(rand_string, SNFS_MAX_FILENAME_LENGTH - 128);
    bool is_a_dir = is_dir(rand_string, SNFS_MAX_FILENAME_LENGTH - 128);
    create_file_at_path(rand_string);

    bool wrote = false;
    off_t size = 0;
    if (i % 2 == 0 && !is_a_dir) {
      size = write_rand_to(rand_string, 1024, NULL);
      wrote = true;
    }

    get_stat(rand_string, &real_st);
    check(!snfs_getattr(rand_string, &st));
    check_eq(st.st_size, real_st.st_size);
    if (wrote) check_eq(st.st_size, size);
    check_eq(st.st_mode, real_st.st_mode);
    check_eq(st.st_nlink, real_st.st_nlink);
    check_eq(st.st_ino, real_st.st_ino);
  }

  // Stop the server, but keep the client going. Make sure we get an error.
  check(stop_server(true));

  // Receive should time out, rendering an error
  gen_random_path(rand_string, SNFS_MAX_FILENAME_LENGTH - 128);
  create_file_at_path(rand_string);
  check(snfs_getattr(rand_string, &st) < 0);

  clear_servedir();
  check(teardown_client());
  return true;
}

/**
 * The phase 3 test suite. This function is declared via the BEGIN_TEST_SUITE
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
BEGIN_TEST_SUITE(phase3_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  clean_run_test(test_getattr, server_cleanup);
}
