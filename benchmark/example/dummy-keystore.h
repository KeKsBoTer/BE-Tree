#ifndef __DUMMY_KEYSTORE_H__
#define __DUMMY_KEYSTORE_H__


// It's just a dummy, use integer as the database structure
#define db_t int

db_t *db_new();
int db_put(db_t *db_data, char *key, char *val);
char* db_get(db_t *db_data, char *key);
int db_free(db_t *db_data);

#endif