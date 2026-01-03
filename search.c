// search.c

#include "types.h"
#include "db.h"
#include "ui.h"
#include "display.h"
#include <string.h>
#include <stdlib.h>

// Global search state (can later move to types.h if needed)
static int search_results[5000];  // store matching record numbers
static int num_results = 0;

// Perform a text search in the database
int searchByText(const char *term, int start_after) {
    num_results = 0;

    char sql[1024];
    snprintf(sql, sizeof(sql),
        "SELECT _rowid_ FROM afg WHERE Title LIKE '%%%s%%' OR Summary LIKE '%%%s%%' ORDER BY _rowid_ ASC",
        term, term);

    if (sqlQuery(sql) != 0) {
        updateStatus("[ ERROR: search failed ]");
        return 0;
    }

    int step;
    while ((step = sqlite3_step(result)) == SQLITE_ROW) {
        int rowid = sqlite3_column_int(result, 0);
        if (rowid > start_after) {
            if (num_results < 5000)
                search_results[num_results++] = rowid;
        }
    }

    sqlite3_finalize(result);

    if (num_results == 0) {
        updateStatus("[ No matches found ]");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "[ %d matches found ]", num_results);
        updateStatus(msg);
    }

    return num_results;
}

// Browse through search results
int browseSearchResults(const char *term) {
    if (num_results == 0) return SEARCH_EXIT;

    int current = 0;

    while (1) {
        showFullRecord(search_results[current]);

        int ch = getch();

        switch (ch) {
            case 'q':
                return SEARCH_EXIT;
            case KEY_DOWN:
            case 'j':
                if (current + 1 < num_results) current++;
                break;
            case KEY_UP:
            case 'k':
                if (current > 0) current--;
                break;
            case '\n':
                return search_results[current];  // return selected record
            default:
                break;
        }
    }

    return SEARCH_EXIT;
}
