#include "db.h"
#include "ui.h"         // for updateStatus()


int db_open(void) {
    return sqlite3_open("../afg.db", &db) == SQLITE_OK;
}

void db_close(void) {
    sqlite3_close(db);
}

int sqlQuery(char *sql) {
    int rc = sqlite3_open("../afg.db", &db);
    
    if (rc != SQLITE_OK) {
        updateStatus("[ ERROR: Cannot open database ]");
        sqlite3_close(db);

        return 1;
    }
    
    rc = sqlite3_prepare_v2(db, sql, -1, &result, NULL);
    
    if (rc == SQLITE_OK) {
        int idx = sqlite3_bind_parameter_index(result, "@id");

        if (idx > 0) {
            sqlite3_bind_int(result, idx, record_nr);
        }

    } else {
        updateStatus("[ ERROR: Failed to execute statement ]");

        return 1;
    }
    
    return 0;
}

int countRecords() {
    char *sql = "SELECT COUNT(*) from afg";
    
    if (sqlQuery(sql) != 0) return 1;
    
    int step = sqlite3_step(result);
    
    if (step == SQLITE_ROW) {
        records = sqlite3_column_int(result, 0);
    }
    
    sqlite3_finalize(result);
    sqlite3_close(db);
    
    return 0;
}
