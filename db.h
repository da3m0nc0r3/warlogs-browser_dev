#ifndef DB_H
#define DB_H

#include "types.h"

int db_open(void);
void db_close(void);
int sqlQuery(char *sql);
int countRecords();


#endif