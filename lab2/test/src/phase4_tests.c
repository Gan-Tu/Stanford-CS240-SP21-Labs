#define FUSE_USE_VERSION 26
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <fuse.h>

#include "test.h"
#include "mock.h"
#include "helpers.h"
#include "common.h"
#include "server.h"
#include "client.h"

static volatile int num_entries = 0;
#define MAX_FILES 30
static char *filenames[MAX_FILES];
static char *entries[MAX_FILES + 2] = { 0 };

int mfiller(void *buf, const char *name, const struct stat *stbuf, off_t off) {
  UNUSED(buf);
  UNUSED(off);
  UNUSED(stbuf);
  UNUSED(name);

  if (name && num_entries < MAX_FILES + 2) {
    entries[num_entries] = strdup(name);
  }

  num_entries++;
  return 0;
}

static void reset_dir_stats() {
  for (int i = 0; i < MAX_FILES; ++i) {
    free(entries[i]);
    entries[i] = NULL;
  }

  num_entries = 0;
}

static void server_cleanup() {
  reset_dir_stats();
  stop_server(true);
  teardown_client();
}

static bool test_empty() {
  // Make sure we can create and remove a database.
  check(start_server(true));
  check(setup_client());

  // We need it...
  struct fuse_file_info info;

  // Generate a bunch of empty directories
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 128);
    rand_string[(SNFS_MAX_FILENAME_LENGTH - 128) - 1] = '/';
    create_file_at_path(rand_string);

    int prev_count = num_entries;
    snfs_readdir(rand_string, NULL, mfiller, 0, &info);
    check_neq(prev_count, num_entries);
    check_eq(num_entries, 2); // '.' and '..'

    reset_dir_stats();
  }

  // Clean it allll up
  reset_dir_stats();
  check(stop_server(true));
  check(teardown_client());
  return true;
}

static bool test_names() {
  // Make sure we can create and remove a database.
  check(start_server(true));
  check(setup_client());

  // We need it...
  struct fuse_file_info info;

  // Now non-empty directories
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 25; ++i) {
    // generate a directory name
    gen_random_filename(rand_string, 64);
    rand_string[63] = '/';
    create_file_at_path(rand_string);

    // Put some random number of files [0, 30) in there
    int num_files = rand() % MAX_FILES;
    for (int j = 0; j < num_files; ++j) {
      gen_random_filename(rand_string + 64, 64);
      create_file_at_path(rand_string);
      filenames[j] = strdup(rand_string + 64);
    }

    // Make sure the count is correct
    rand_string[63] = '\0';
    int prev_count = num_entries;
    snfs_readdir(rand_string, NULL, mfiller, 0, &info);
    check_neq(prev_count, num_entries);
    check_eq(num_entries, num_files + 2); // '.' and '..'

    // Make sure all the filenames are there. Yes, it's bad.
    for (int j = 0; j < num_files; ++j) {
      char *filename = filenames[j];
      check(filename);

      bool found_it = false;
      for (int k = 0; k < num_entries; ++k) {
        char *entry = entries[k];
        check(entry);

        if (!strcmp(entry, "..") || !strcmp(entry, ".")) continue;
        found_it = !strcmp(filename, entry);
        if (found_it) break;
      }

      check(found_it);
    }

    reset_dir_stats();
  }

  // Clean it allll up
  reset_dir_stats();
  check(stop_server(true));
  check(teardown_client());
  return true;
}

/**
 * The phase 4 test suite. This function is declared via the BEGIN_TEST_SUITE
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
BEGIN_TEST_SUITE(phase4_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  clean_run_test(test_empty, server_cleanup);
  clean_run_test(test_names, server_cleanup);
}
