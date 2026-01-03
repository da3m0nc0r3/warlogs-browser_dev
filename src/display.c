// display.c

#include "types.h"
#include "db.h"
#include "ui.h"

int showFullRecord(int rec_nr) {
    record_nr = rec_nr;
    const char *sql =
        "SELECT _rowid_, DateOccurred, Latitude, Longitude, UnitName, "
        "TypeOfUnit, Classification, friendlywia, friendlykia, "
        "hostnationwia, hostnationkia, enemywia, enemykia, enemydetained, "
        "Title, Summary FROM afg WHERE _rowid_ = @id";
    
    if (sqlQuery((char *)sql) != 0) 
        return 1;
    
    int step = sqlite3_step(result);

    int content_height = getmaxy(win_content) - 2;  // height minus borders
    int header_width = getmaxx(win_header) - 2;     // width minus borders    

    // Clear windows
    werase(win_content);
    box(win_content, 0, 0);
    wrefresh(win_content);

    werase(win_header);
    box(win_header, 0, 0);

    if (step == SQLITE_ROW) {
        int header_line = 1;
        int content_line = 1;

        // Set up scrollable content pad
        int pad_width = getmaxx(win_content) - 4;
        int pad_height = 5000;

        if (content_pad)
            delwin(content_pad);

        content_pad = newpad(pad_height, pad_width);

        // Header info
        mvwprintw(win_header, header_line++, 2, "Record: %s / %d | %s |", 
                  sqlite3_column_text(result, 0),
                  records, 
                  sqlite3_column_text(result, 1));

        mvwprintw(win_header, header_line++, 2, "Lat: %s | Lon: %s |", 
                  sqlite3_column_text(result, 2), 
                  sqlite3_column_text(result, 3)); 

        mvwprintw(win_header, header_line++, 2, "Unit: %s | Type: %s |",
                  sqlite3_column_text(result, 4), 
                  sqlite3_column_text(result, 5));
        
        mvwprintw(win_header, header_line++, (header_width - 7) / 2, "[ %s ]",
                  sqlite3_column_text(result, 6));

        header_line = 1;  // Reset for right-aligned info

        mvwprintw(win_header, header_line++, header_width - 45, 
                  "Coalition :               WIA: %s, KIA: %s", 
                  sqlite3_column_text(result, 7), 
                  sqlite3_column_text(result, 8));

        mvwprintw(win_header, header_line++, header_width - 45, 
                  "Host      :               WIA: %s, KIA: %s", 
                  sqlite3_column_text(result, 9), 
                  sqlite3_column_text(result, 10));

        mvwprintw(win_header, header_line++, header_width - 45, 
                  "Enemy     :  Detained: %s, WIA: %s, KIA: %s", 
                  sqlite3_column_text(result, 13), 
                  sqlite3_column_text(result, 11), 
                  sqlite3_column_text(result, 12));

        content_line++;

        // Word wrap title
        const char *title = (const char *)sqlite3_column_text(result, 14);
        int content_max_width = getmaxx(win_content) - 6;
        print_wrapped(content_pad, &content_line, 4, content_max_width, title, current_search);
        content_line++;

        // Word wrap summary
        const char *summary = (const char *)sqlite3_column_text(result, 15);
        print_wrapped(content_pad, &content_line, 4, content_max_width, summary, current_search);

        content_total_lines = content_line;
        content_pad_lines = content_line;
        content_scroll = 0;

        // Status hint
        if (content_total_lines > content_height) {
            updateStatus("[ Up/Dn: scroll current document | <- prev | -> next | [ENTER] to end scroll mode | q quit browser ]");
        } else if (current_search[0] == '\0') {
            updateStatus("[ Up/Dn : pre/next record | <-/-> : -/+100 | PgDn/PgUp : +/-1000 | /,s: search | q quit browser ]");
        } else {
            updateStatus("[ Up/Dn : pre/next record | <-/-> : -/+100 | PgDn/PgUp : +/-1000 | /,s: search n : Next in search | q quit browser ]");
        }

    } else {
        mvwprintw(content_pad, 1, 2, "No record found.");
    }

    if (!content_pad)
        updateStatus("[ DEBUG: content_pad is NULL ]");

    refresh_content_pad();
    wrefresh(win_header);
    wrefresh(win_content);
    wrefresh(win_status);

    sqlite3_finalize(result);
    // Note: Do NOT close db here; handled by db_close() in main()

    return 0;
}
