#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdbool.h>
#include <sys/stat.h>

#include "common.h"

bool server_name_find_or_insert(const char *path, fhandle *handle);
bool create_file_at_path(const char *path);
bool is_dir(const char *path, size_t len);
bool clear_servedir();
off_t write_rand_to(const char *path, size_t max, char **bufout);
void get_stat(const char *path, struct stat *st);

char *pushd(const char *path);
bool popd(char *prev_cwd);

void gen_random_filename(char *s, const int len);
void gen_random_path(char *s, const int len);

#endif
