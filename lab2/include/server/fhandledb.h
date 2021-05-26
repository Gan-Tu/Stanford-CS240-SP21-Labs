#ifndef SNFS_FHANDLEDB_H
#define SNFS_FHANDLEDB_H

#include <stdbool.h>

#include "common.h"

bool init_db_if_needed();
bool destroy_db(bool);
const char *get_file(fhandle handle);
fhandle name_find_or_insert(const char *filename);
bool name_remove(const char *filename);

#endif
