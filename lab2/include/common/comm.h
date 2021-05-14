#ifndef SNFS_COMMON_COMM_H
#define SNFS_COMMON_COMM_H

#include <stdlib.h>

long long current_ms();
void get_random(void *buf, size_t bytes);
void *receive_data(int sock, size_t *size, int flags);
int send_data(int sock, void *data, size_t size, int flags);

#endif
