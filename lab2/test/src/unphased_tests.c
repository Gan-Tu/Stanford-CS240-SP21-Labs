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

static const int MAX_BYTES = 4096;

static void server_cleanup() {
  stop_server(true);
  teardown_client();
}

static bool test_truncate() {
  check(start_server(true));
  check(setup_client());

  // First the simple "there's no file" case
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 30; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    check_eq(snfs_truncate(rand_string, rand()), -ENOENT);
  }

  // Now let's make sure truncating works
  struct stat real_st;
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);

    off_t size = rand() % MAX_BYTES;
    check(!snfs_truncate(rand_string, size));

    get_stat(rand_string, &real_st);
    check_eq(real_st.st_size, size);
  }

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

static bool test_chmod() {
  check(start_server(true));
  check(setup_client());

  // First the simple "there's no file" case
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 30; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    check_eq(snfs_chmod(rand_string, rand()), -ENOENT);
  }

  // Now let's make sure the op works
  struct stat real_st;
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);

    mode_t mode = rand() % 0777;
    check(!snfs_chmod(rand_string, mode));

    get_stat(rand_string, &real_st);
    check_eq(real_st.st_mode & 0777, mode);
  }

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

static bool test_chown() {
  check(start_server(true));
  check(setup_client());

  // First the simple "there's no file" case
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 30; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    check_eq(snfs_chown(rand_string, rand(), rand()), -ENOENT);
  }

  // Now let's make sure the op works
  struct stat real_st;
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);

    uid_t uid = rand();
    gid_t gid = rand();
    check(!snfs_chown(rand_string, uid, gid));

    get_stat(rand_string, &real_st);
    check_eq(real_st.st_uid, uid);
    check_eq(real_st.st_gid, gid);
  }

  // Make sure -1 values are ignored
  gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
  create_file_at_path(rand_string);
  get_stat(rand_string, &real_st);

  uid_t uid = rand();
  gid_t prev_gid = real_st.st_gid;
  check(!snfs_chown(rand_string, uid, -1));
  get_stat(rand_string, &real_st);
  check_eq(real_st.st_uid, uid);
  check_eq(real_st.st_gid, prev_gid);

  gid_t gid = rand();
  uid_t prev_uid = real_st.st_uid;
  check(!snfs_chown(rand_string, -1, gid));
  get_stat(rand_string, &real_st);
  check_eq(real_st.st_gid, gid);
  check_eq(real_st.st_uid, prev_uid);

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

static bool test_utimens() {
  check(start_server(true));
  check(setup_client());

  struct timespec tv[2];

  // First the simple "there's no file" case
  char rand_string[SNFS_MAX_FILENAME_BUF];
  for (int i = 0; i < 30; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    check_eq(snfs_utimens(rand_string, tv), -ENOENT);
  }

  // Now let's make sure the op works
  struct stat real_st;
  for (int i = 0; i < 100; ++i) {
    gen_random_filename(rand_string, SNFS_MAX_FILENAME_LENGTH - 64);
    create_file_at_path(rand_string);

    tv[0].tv_sec = rand();
    tv[0].tv_nsec = rand();
    tv[1].tv_sec = rand();
    tv[1].tv_nsec = rand();
    check(!snfs_utimens(rand_string, tv));

    get_stat(rand_string, &real_st);
#if defined(__APPLE__)
    check_eq(real_st.st_atimespec.tv_sec, tv[0].tv_sec);
    check_eq(real_st.st_mtimespec.tv_sec, tv[1].tv_sec);
#elif defined(__linux__)
    check_eq(real_st.st_atime, tv[0].tv_sec);
    check_eq(real_st.st_mtime, tv[1].tv_sec);
#endif
  }

  // Clean up
  check(stop_server(true));
  check(teardown_client());
  return true;
}

/**
 * The unphased test suite. This function is declared via the BEGIN_TEST_SUITE
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
BEGIN_TEST_SUITE(unphased_tests) {
  // Get some randomness in there
  srand(current_ms());

  // Run the tests
  clean_run_test(test_truncate, server_cleanup);
  clean_run_test(test_chmod, server_cleanup);
  clean_run_test(test_chown, server_cleanup);
  clean_run_test(test_utimens, server_cleanup);
}
