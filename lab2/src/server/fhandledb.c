/* #define DEBUG */

#include <ftw.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <db.h>

#include "common.h"
#include "server.h"

static const char *const DB_FILE = ".snfs.db";
static DB *DBP = NULL;

/**
 * Initializes the database if needed.
 *
 * @return false if it was initialized prior, true if this call initalized it
 */
bool init_db_if_needed() {
  debug("Initializing DB. DB existing? %p\n", DBP);
  if (DBP) return false;

  // Initialize the handle for the database.
  int cerr = db_create(&DBP, NULL, 0);
  if (cerr) {
    err_exit("Could not instantiate DB pointer: %s\n", db_strerror(cerr));
  }

  // Open an existing database, or create it if it doesn't exist.
  int oerr = DBP->open(DBP, NULL, DB_FILE, NULL, DB_BTREE, DB_CREATE, 0664);
  if (oerr) {
    err_exit("Error opening db file '%s': %s\n", DB_FILE, db_strerror(oerr));
  }

  return true;
}

/**
 * Closes the database. Deletes the entire database if requested. All keys and
 * values will be lost! If closing the database fails, it will not be deleted,
 * even if requested.
 *
 * @param delete whether to delete everything or not
 *
 * @return true if the deletion and close were successful, false otherwise
 */
bool destroy_db(bool delete) {
  // If the database is already gone, exit early.
  debug("Destroying database. Delete? %d\n", delete);
  if (!DBP) return true;

  // Try to close the database.
  int err = DBP->close(DBP, 0);
  if (err) {
    debug("Database failed to close: %s\n", db_strerror(err));
    return false;
  }

  // Set DBP to NULL to indicate its closure. Delete the database if requested.
  DBP = NULL;
  if (delete) {
    if ((err = remove(DB_FILE)) != 0) {
      debug("Error deleting the database: %s\n", strerror(err));
      return false;
    }
  }

  return true;
}

/**
 * Attempts to get the value for `key` with length `keylen` from the database.
 * If `vallen` is not NULL, it is set to the length, in bytes, of the value.
 * Returns a malloc()d pointer to the value if it is found. It is the caller's
 * responsibility to free it.
 *
 * @param key a pointer to the key whose value to search for
 * @param keylen the length of the key in bytes
 * @param[out] vallen a pointer which is set the length of value if it is found
 *
 * @return a malloc()d point to the value if it is found, NULL otherwise
 */
static char *find(const void *key, size_t keylen, size_t *vallen) {
  // Make sure we have a database.
  init_db_if_needed();

  // The key to fetch.
  DBT db_key = (DBT) {
    .data = (void *)key,
    .size = keylen
  };

  // Where the call will place the result.
  DBT db_value = (DBT) { .data = NULL }; // Really, zero initialize.

  // Some nice debug info.
  if_debug {
    printf("Fetching value for key: ");
    printbuf((void *)key, keylen);
  }

  // Do the get.
  int err = DBP->get(DBP, NULL, &db_key, &db_value, 0);
  if (err) {
    if (err != DB_NOTFOUND) {
      DBP->err(DBP, err, "Database get() failed!");
      err_exit("Database get failed unexpectedly!\n");
    }

    debug("Key was not found in the database.\n");
    return NULL;
  }

  if (vallen) *vallen = db_value.size;
  char *ret_str = (char *)malloc(db_value.size);
  memcpy(ret_str, db_value.data, db_value.size);
  return ret_str;
}

/**
 * Inserts the value `val` with size `vallen` for `key` of size `keylen` in the
 * database.
 *
 * @param key a pointer to the key
 * @param klen the size, in bytes, of the key
 * @param val a pointer to the value to set for the key
 * @param vlen the size of the value in bytes
 */
static void insert(const void *key, size_t klen, const void *val, size_t vlen) {
  if_debug {
    printf("Insert key: ");
    printbuf((void *)key, klen);
    printf("With value: ");
    printbuf((void *)val, vlen);
    printf("(%s)\n", (char *)val);
  }

  init_db_if_needed();

  DBT db_key = (DBT) {
    .data = (void *)key,
    .size = klen,
  };

  DBT db_value = (DBT) {
    .data = (void *)val,
    .size = vlen,
  };

  int err = DBP->put(DBP, NULL, &db_key, &db_value, 0);
  if (err) {
    DBP->err(DBP, err, "DB->put");
    err_exit("Database put failed.\n");
  }
}

/**
 * Finds an unused fhandle and returns it.
 *
 * @return fhandle a new, unused, random fhandle
 */
static fhandle generate_new_fhandle() {
  while (true) {
    fhandle handle;
    get_random(&handle, sizeof(fhandle));

    size_t rlen;
    char *result = find(&handle, sizeof(fhandle), &rlen);
    if (!result) {
      return handle;
    }

    free(result);
  };
}

/**
 * Generates a new fhandle and creates a mapping from the fhandle to the
 * filename and from the filename to fhandle.
 *
 * @param filename the filename to map the fhandle from and to
 *
 * @return the fhandle that was generated
 */
static fhandle new_fhandle(const char *filename) {
  fhandle handle = generate_new_fhandle();

  // TODO: Create a more encompassing structure to map fhandle to.
  insert(&handle, sizeof(fhandle), filename, strlen(filename) + 1);
  insert(filename, strlen(filename) + 1, &handle, sizeof(fhandle));

  return handle;
}

/**
 * Attempts to find the fhandle for `filename`. If it is not found, a new
 * fhandle is generated, and the proper mappings from/to the fhandle are set up.
 *
 * @param filename the name of the file
 *
 * @return the filename's fhandle
 */
fhandle name_find_or_insert(const char *filename) {
  UNUSED(new_fhandle);

  assert(filename);
  debug("n_f_o_i for %s\n", filename);

  // FIXME: Implement this function.

  return -1;
}

/**
 * Attempts to find the filename associated with the `handle`. A malloc()d
 * filename is returned if it is found. It is the caller's responsibility to
 * free it.
 *
 * @param handle the fhandle whose filename will be searched for
 *
 * @return a malloc()d filename if it is found, NULL otherwise
 */
const char *get_file(fhandle handle) {
  UNUSED(handle);

  // FIXME: Implement this function.

  return NULL;
}
