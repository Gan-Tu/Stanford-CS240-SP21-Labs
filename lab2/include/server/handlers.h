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

// Extra Credit
void handle_create(int sock, snfs_create_args *args);
void handle_remove(int sock, snfs_remove_args *args);
void handle_rename(int sock, snfs_rename_args *args);
void handle_mkdir(int sock, snfs_mkdir_args *args);
void handle_rmdir(int sock, snfs_rmdir_args *args);

#endif
