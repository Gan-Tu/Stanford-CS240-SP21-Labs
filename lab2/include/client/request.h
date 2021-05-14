#ifndef SNFS_CLIENT_REQUEST_H
#define SNFS_CLIENT_REQUEST_H

#include <stdlib.h>
#include <stdbool.h>

#include "common.h"

bool server_mount(int sock, const char *url, fhandle *root);
void test_connection(int sock, const char *url);

snfs_rep *snfs_req_rep_f(int, const char *, snfs_req *, size_t, int);
snfs_rep *snfs_req_rep(int, const char *, snfs_req *request, size_t size);

#endif
