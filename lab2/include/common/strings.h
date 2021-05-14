#ifndef SNFS_COMMON_STRINGS_H
#define SNFS_COMMON_STRINGS_H

#include <stdlib.h>

#include "ftypes.h"

const char *strmsgtype(snfs_msg_type);
const char *strsnfserror(snfs_error);
void printbuf(void *, size_t n);

#endif
