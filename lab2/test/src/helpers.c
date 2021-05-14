#define _XOPEN_SOURCE 600
#include <ftw.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

#include "test.h"
#include "common.h"
#include "server.h"
#include "client.h"
#include "mock.h"
#include "helpers.h"

void gen_random(char *s, const int len, const char *chars, size_t nchars) {
  assert(len);

  for (int i = 0; i < len; ++i) {
    s[i] = chars[rand() % (nchars - 1)];
  }

  s[len] = '\0';
}

void gen_random_filename(char *s, const int len) {
  static const char pathchars[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  gen_random(s, len, pathchars, sizeof(pathchars));
}

void gen_random_path(char *s, const int len) {
  static const char pathchars[] = "/0123456789abcdefghijklmnopqrstuvwxyz";
  gen_random(s, len, pathchars, sizeof(pathchars));

  // Make the first character '/' half the time
  if (rand() % 2 == 0) {
    s[0] = '/';
  }
}

char *pushd(const char *path) {
  char *cwd = (char *)malloc(1024);
  if (!getcwd(cwd, 1023)) {
    perror("getcwd failed!\n");
    free(cwd);
    return NULL;
  }

  if (chdir(path) < 0) {
    print_err("Couldn't chdir to %s/%s: %s\n", cwd, path, strerror(errno));
    free(cwd);
    return NULL;
  }

  return cwd;
}

bool popd(char *prev_cwd) {
  if (chdir(prev_cwd) < 0) {
    perror("Couldn't chdir back\n");
    free(prev_cwd);
    return false;
  }

  free(prev_cwd);
  return true;
}


bool server_name_find_or_insert(const char *path, fhandle *handle) {
  // Stop the server first.
  bool stopped_server = false;
  if (server_pid) {
    check(stop_server(false));
    stopped_server = true;
  }

  // Switch into the server directory. Do the lookup.
  char *cwd = pushd(SERVE_DIR);
  if (!cwd) return false;
  *handle = name_find_or_insert(path);
  popd(cwd);

  // Restart the server.
  if (stopped_server) {
    check(start_server(false));
  }

  return *handle;
}

bool create_file_at_path(const char *path) {
  /* printf("Creating %s\n", path); */
  char tmp[SNFS_MAX_FILENAME_BUF];
  char *p = NULL;
  size_t len;
  bool is_a_dir = is_dir(path, strlen(path));

  snprintf(tmp, sizeof(tmp), "%s/%s", SERVE_DIR, path);
  len = strlen(tmp);

  if (tmp[len - 1] == '/') {
    tmp[len - 1] = '\0';
  }

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, S_IRWXU) && errno != EEXIST) {
        perror("mkdir failed");
        return false;
      }

      *p = '/';
    }
  }

  if (is_a_dir) {
    if (mkdir(tmp, S_IRWXU) && errno != EEXIST) {
      perror("mkdir failed");
      return false;
    }
  } else {
    int fd;
    if ((fd = creat(tmp, S_IRWXU)) < 0 && errno != EEXIST && errno != EISDIR) {
      perror("creat failed");
      return false;
    }

    close(fd);
  }

  return true;
}

bool is_dir(const char *path, size_t len) {
  bool answer = path[len - 1] == '/';
  return answer;
}

off_t write_rand_to(const char *path, size_t max, char **bufout) {
  char tmp[SNFS_MAX_FILENAME_BUF];
  snprintf(tmp, sizeof(tmp), "%s/%s", SERVE_DIR, path);
  int fd = open(tmp, O_RDWR);

  size_t size = rand() % max;

  char *buf = (char *)malloc(size);
  get_random(buf, size);
  ssize_t wrote = write(fd, buf, size);
  close(fd);

  if (bufout) *bufout = buf;
  else free(buf);

  if (wrote < 0) return 0;
  return wrote;
}

void get_stat(const char *path, struct stat *st) {
  char tmp[SNFS_MAX_FILENAME_BUF];
  snprintf(tmp, sizeof(tmp), "%s/%s", SERVE_DIR, path);
  stat(tmp, st);
}

static int unlink_cb(const char *fpath, const struct stat *sb,
    int typeflag, struct FTW *ftwbuf) {
  UNUSED(sb);
  UNUSED(typeflag);
  UNUSED(ftwbuf);

  int rv = remove(fpath);
  if (rv < 0) {
    print_err("Couldn't unlink '%s': %s\n", fpath, strerror(errno));
  }

  return rv;
}

bool clear_servedir() {
  mkdir(SERVE_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
  if (nftw(SERVE_DIR, unlink_cb, 256, FTW_DEPTH | FTW_PHYS) < 0) {
    print_err("Couldn't delete entire mock server directory!");
    return false;
  }


  mkdir(SERVE_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
  return true;
}

