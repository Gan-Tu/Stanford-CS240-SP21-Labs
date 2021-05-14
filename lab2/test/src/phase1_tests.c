/* #define DEBUG */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#include "test.h"
#include "mock.h"
#include "helpers.h"
#include "common.h"
#include "server.h"
#include "client.h"

static char *cwd = NULL;

static void db_cleanup() {
  if (cwd) popd(cwd);
  destroy_db(true);
}

static void server_cleanup() {
  stop_server(true);
  teardown_client();
}

static bool test_db() {
  // Change into the serve directory
  cwd = pushd(SERVE_DIR);
  check(cwd);

  // Make sure we can create and remove a database.
  check(init_db_if_needed());
  check(destroy_db(true));

  // Okay, try to put a filename and get it back
  check(init_db_if_needed());

  const char *first_name = "/empty/file";
  const char *second_name = "/empty/file "; // notice the space at the end...

  // Get it back once, twice, three times
  fhandle first = name_find_or_insert(first_name);
  const char *name = get_file(first);
  const char *name2 = get_file(first);
  const char *name3 = get_file(first);
  check_eq_str(name, first_name);
  check_eq_str(name2, first_name);
  check_eq_str(name3, first_name);

  // Now check the second name
  fhandle second = name_find_or_insert(second_name);
  const char *name_second = get_file(second);
  check_eq_str(name_second, second_name);

  // Make sure the first one is still there
  const char *name4 = get_file(first);
  check_eq_str(name4, first_name);

  // Make sure we get the same handle for both of the names
  fhandle first_take_2 = name_find_or_insert(first_name);
  check_eq(first_take_2, first);
  fhandle second_take_2 = name_find_or_insert(second_name);
  check_eq(second_take_2, second);

  // Okay, let's generate a bunch of random names and make sure we can get/put
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 1e3; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH);

    fhandle handle1 = name_find_or_insert(rand_string);
    const char *filename = get_file(handle1);
    check_eq_str(rand_string, filename);

    fhandle handle2 = name_find_or_insert(rand_string);
    check_eq(handle1, handle2);
    free((void *)filename);
  }

  // Free them all!
  free((void *)name);
  free((void *)name2);
  free((void *)name3);
  free((void *)name_second);
  free((void *)name4);

  check(destroy_db(true));
  check(popd(cwd));
  return true;
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

static bool test_client_mount() {
  check(start_server(true));
  check(setup_client());

  // Ensure we got SOME file handle back.
  check_neq(MOCK_STATE->root_fhandle, 0);

  // If we did, ensure it's either the dummy value or some real file handle.
  fhandle handle;
  check(server_name_find_or_insert("/", &handle));
  if (handle != MOCK_STATE->root_fhandle) {
    check_eq(MOCK_STATE->root_fhandle, SNFS_DUMMY_FH);
  } else {
    check_eq(handle, MOCK_STATE->root_fhandle);
  }

  // Okay, done
  check(teardown_client());
  check(stop_server(true));
  return true;
}

static bool test_mount() {
  check(start_server(true));
  check(setup_client());

  // The client's mounted. Let's see what it thinks the root is.
  fhandle root;
  check(lookup("/", &root));

  // Ensure the server thinks the same thing.
  fhandle handle;
  check(server_name_find_or_insert("/", &handle));
  check_eq(root, handle);

  // Once more for good measure
  check(lookup("/", &root));
  check_eq(root, handle);

  // Teardown everything, clearing the database.
  check(teardown_client());
  check(stop_server(true));

  // Let's make sure the handles are randomly generated
  check(start_server(true));
  check(setup_client());

  fhandle root2;
  check(lookup("/", &root2));

  // Ensure the server thinks the same thing.
  fhandle handle2;
  check(server_name_find_or_insert("/", &handle2));
  check_eq(root, handle);

  // Make sure both root handles are different
  check_neq(root, root2);

  // Okay, done
  check(teardown_client());
  check(stop_server(true));
  return true;
}


/**
 * The phase 1 test suite. This function is declared via the BEGIN_TEST_SUITE
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
BEGIN_TEST_SUITE(phase1_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  run_test(test_noop);
  clean_run_test(test_db, db_cleanup);
  clean_run_test(test_client_mount, server_cleanup);
  clean_run_test(test_mount, server_cleanup);
}
