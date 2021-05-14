#ifndef SNFS_SERVER_HANDLERS_H
#define SNFS_SERVER_HANDLERS_H

#include "common.h"

void handle_noop(int sock);
void handle_mount(int sock);
void handle_getattr(int sock, snfs_getattr_args *args);
void handle_readdir(int sock, snfs_readdir_args *args);
void handle_lookup(int sock, snfs_lookup_args *args);
void handle_read(int sock, snfs_read_args *args);
void handle_write(int sock, snfs_write_args *args);
void handle_setattr(int sock, snfs_setattr_args *args);
void handle_error(int sock, snfs_error error);
void handle_unimplemented(int sock, snfs_msg_type msg_type);

#endif
