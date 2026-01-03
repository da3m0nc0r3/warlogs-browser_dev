#include <sqlite3.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

///////////////////////////////////////////////////////////////////////////////
/// Global vars & windows ////////////////////////////////////////////////////
sqlite3 *db;
sqlite3_stmt *result;

#define SEARCH_EXIT      0      // Search return value
#define SEARCH_OPEN_REC  1      // Search return value
#define SEARCH_NEW_QUERY 2      // Search return value
#define BORDER 2                // Border calc factor for calculating window sizes

unsigned int ch;
int record_nr = 1;
int records;
int last_search_rowid = 0;      // Store last search row ID
char current_search[256] = "";  // For the 'n' case

int content_pad_lines = 0;      // veriable for dynamic content pad
int content_scroll = 0;         // flag for scrollable window
int in_scroll = 0;              // flag for in scroll (for key handling)
int content_total_lines = 0;    // total number of lines in record



// Windows for different sections
WINDOW *win_header = NULL;     // top field
WINDOW *win_content = NULL;    // main document window
WINDOW *win_status = NULL;     // bottom field
WINDOW *win_input = NULL;      // search input
WINDOW *win_sresult = NULL;    // search results

// scrollable content pad
WINDOW *content_pad = NULL;

///////////////////////////////////////////////////////////////////////////////
/// function declarations
//
int showFullRecord(int rec_nr);
int browseSearchResults(const char *term);
int searchByText(const char *term, int start_after);

///////////////////////////////////////////////////////////////////////////////
/// initWindows() - Create window layout
//
void initWindows() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int HEADER_HEIGHT   = 3 + BORDER;
    int STATUS_HEIGHT   = 0 + BORDER;
    // int INPUT_HEIGHT    = 1 + BORDER;
    // int CONTENT_HEIGHT  = max_y - HEADER_HEIGHT - STATUS_HEIGHT;


    // Create windows
    win_header = newwin(HEADER_HEIGHT, max_x, 0, 0);
    win_content = newwin(max_y - HEADER_HEIGHT - STATUS_HEIGHT, max_x, HEADER_HEIGHT, 0);
    win_status = newwin(STATUS_HEIGHT, max_x, max_y - STATUS_HEIGHT, 0);
    
    // Enable scrolling for content window
    scrollok(win_content, TRUE);
    
    // Draw borders
    box(win_header, 0, 0);
    box(win_content, 0, 0);
    box(win_status, 0, 0);
    
    wrefresh(win_header);
    wrefresh(win_content);
    wrefresh(win_status);
}

///////////////////////////////////////////////////////////////////////////////
/// cleanupWindows()
//
void cleanupWindows() {
    if (win_header) delwin(win_header);
    if (win_content) delwin(win_content);
    if (win_status) delwin(win_status);
    if (win_input) delwin(win_input);
    if (win_sresult) delwin(win_sresult);
}

///////////////////////////////////////////////////////////////////////////////
/// updateHeader()
//
void updateHeader() {

    int header_line = 1;

    werase(win_header);
    box(win_header, 0, 0);

    int max_width = getmaxx(win_header) -2;         // width minus borders    
    // Header info
    mvwprintw(win_header, header_line ++, 2, "Record: %s / %d | %s |", 
        sqlite3_column_text(result, 0),
        records, 
        sqlite3_column_text(result, 1));

    mvwprintw(win_header, header_line ++, 2, "Lat: %s | Lon: %s |", 
        sqlite3_column_text(result, 2), 
        sqlite3_column_text(result, 3)); 

    mvwprintw(win_header, header_line ++, 2, "Unit: %s | Type: %s |",
        sqlite3_column_text(result, 4), 
        sqlite3_column_text(result, 5));

    mvwprintw(win_header, header_line ++, (max_width - 7) / 2, "[ %s ]",        // print in center / on border
        sqlite3_column_text(result, 6));

    header_line = 1;    // Reset line count to 1 for print in right part

    mvwprintw(win_header, header_line ++, max_width - 45, "Coalition :               WIA: %s, KIA: %s", 
        sqlite3_column_text(result, 7), 
        sqlite3_column_text(result, 8));

    mvwprintw(win_header, header_line ++, max_width - 45, "Host      :               WIA: %s, KIA: %s", 
        sqlite3_column_text(result, 9), 
        sqlite3_column_text(result, 10));

    mvwprintw(win_header, header_line ++, max_width - 45, "Enemy     :  Detained: %s, WIA: %s, KIA: %s", 
        sqlite3_column_text(result, 13), 
        sqlite3_column_text(result, 11), 
        sqlite3_column_text(result, 12));

    wrefresh(win_header);

    return;
}

///////////////////////////////////////////////////////////////////////////////
/// updateStatus()
//
void updateStatus(const char *message) {
    werase(win_status);
    box(win_status, 0, 0);
    mvwprintw(win_status, 1, 2, "%s", message);
    wrefresh(win_status);
}

///////////////////////////////////////////////////////////////////////////////
/// getTextInput() - Free text input with ncurses
//
int getTextInput(const char *prompt, char *buffer, int max_len) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    // Create input window in center
    int win_width = max_x - 10;
    int win_height = 5;
    win_input = newwin(win_height, win_width, 
                       (max_y - win_height) / 2, 
                       (max_x - win_width) / 2);
    
    box(win_input, 0, 0);
    mvwprintw(win_input, 1, 2, "%s", prompt);
    mvwprintw(win_input, 3, 2, "> ");
    wrefresh(win_input);
    
    // Enable echo and move cursor
    echo();
    curs_set(1);
    
    // Get input
    wmove(win_input, 3, 4);
    wgetnstr(win_input, buffer, max_len - 1);
    
    // Restore settings
    noecho();
    curs_set(0);
    
    delwin(win_input);
    win_input = NULL;
    
    // Redraw main windows
    touchwin(stdscr);
    touchwin(win_header);
    touchwin(win_content);
    touchwin(win_status);
    wrefresh(win_header);
    wrefresh(win_content);
    wrefresh(win_status);
    refresh();
    
    return strlen(buffer);
}

///////////////////////////////////////////////////////////////////////////////
/// db helper functions
//
int db_open(void) {
    return sqlite3_open("../afg.db", &db) == SQLITE_OK;
}

void db_close(void) {
    sqlite3_close(db);
}

///////////////////////////////////////////////////////////////////////////////
/// sqlQuery()
// //
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

///////////////////////////////////////////////////////////////////////////////
/// countRecords()
//
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

///////////////////////////////////////////////////////////////////////////////
/// Helper function to print highlighted text
//
void print_highlighted(WINDOW *win, int y, int x, const char *text, const char *term) {

        if (!term || !*term) {
            mvwprintw(win, y, x, "%s", text);

            return;
        }
        const char *p = text;
        int term_len = strlen(term);

        while (*p) {
            const char *match = strcasestr(p, term);

            if (!match) {
                wprintw(win, "%s", p);

                break;
            }

            wprintw(win, "%.*s", (int)(match - p), p);
            wattron(win, COLOR_PAIR(1) | A_BOLD);
            wprintw(win, "%.*s", term_len, match);
            wattroff(win, COLOR_PAIR(1) | A_BOLD);
            p = match + term_len;
        }
    }

///////////////////////////////////////////////////////////////////////////////
/// Word wrap function
//
void print_wrapped(WINDOW *win, int *line, int start_x, int max_width,
                                const char *text, const char *highlight) {    
    int col = 0;
    const char *p = text;

    wmove(win, *line, start_x);

    while (*p) {
        /* Handle explicit newlines */
        if (*p == '\n') {
            (*line) ++;
            col = 0;
            wmove(win, *line, start_x);
            p++;

            continue;
        }

        /* Extract one word */
        const char *word_start = p;
        int word_len = 0;

        while (p[word_len] && p[word_len] != ' ' && p[word_len] != '\n')
            word_len ++;

        /* overflow prevention */
        if (word_len > max_width) {
            word_len = max_width - 3;               // -3 to fix overflow bug
        }

        /* Move to next line if word won't fit */
        if (col + word_len > max_width - 3) {       // -3 to fix overflow bug
            (*line) ++;
            col = 0;
            wmove(win, *line, start_x);
        }

        /* Print the word (with optional highlight) */
        if (highlight && *highlight) {
            char word[512];
            strncpy(word, word_start, word_len);
            word[word_len] = '\0';
            print_highlighted(win, *line, start_x + col, word, highlight);

        } else {
            wprintw(win, "%.*s", word_len, word_start);
        }

        col += word_len;
        p += word_len;

        /* Print following space */
        if (*p == ' ') {
            if (col + 1 > max_width -3) {           // -3 to fix overflow bug
                (*line) ++;
                col = 0;
                wmove(win, *line, start_x);
            
            } else {            
                waddch(win, ' ');
                col ++;
            }
            p ++;
        }
    }

    (*line) ++;
}

///////////////////////////////////////////////////////////////////////////////
/// refresh_content_pad()
//
void refresh_content_pad(void) {
    int wy, wx;
    int h = getmaxy(win_content) - 2;
    int w = getmaxx(win_content) - 2;

    getbegyx(win_content, wy, wx);

    prefresh(
        content_pad,
        content_scroll, 0,          // pad start
        wy + 1, wx + 1,             // screen start
        wy + h, wx + w              // screen end
    );
}

///////////////////////////////////////////////////////////////////////////////
/// scroll_content()
//
void scroll_content(int delta) {
    int content_height = getmaxy(win_content) - 2;

    int max_scroll = content_total_lines - content_height;
    if (max_scroll < 0) max_scroll = 0;

    int new_scroll = content_scroll + delta;

    if (new_scroll < 0) new_scroll = 0;
    if (new_scroll > max_scroll) new_scroll = max_scroll;

    int diff = new_scroll - content_scroll;

    if (diff != 0) {
        wscrl(win_content, diff);
        content_scroll = new_scroll;
        wrefresh(win_content);
    }
}

///////////////////////////////////////////////////////////////////////////////
/// showFullRecord()
//
int showFullRecord(int rec_nr) {
    record_nr = rec_nr;
    char *sql = "SELECT _rowid_, DateOccurred, Latitude, Longitude, UnitName, "
                "TypeOfUnit, Classification, friendlywia, friendlykia, "
                "hostnationwia, hostnationkia, enemywia, enemykia, enemydetained, "
                "Title, Summary FROM afg WHERE _rowid_ = @id";
    
    if (sqlQuery(sql) != 0) 
        return 1;
    
    int step = sqlite3_step(result);

    // int content_scroll = 0;
    // int content_line = 0;

    // Calculate how much fits on one screen
    int content_height = getmaxy(win_content) - 2;  // height minus borders
    int max_width = getmaxx(win_header) -2;         // width minus borders    

    // Clear content window
    werase(win_content);
    box(win_content, 0, 0);
    wrefresh(win_content);

    // Clear header window
    werase(win_header);
    box(win_header, 0, 0);
    
    if (step == SQLITE_ROW) {

       int header_line = 1;
       int content_line = 1;

    // Setting up the pad ////////////////////////////////////////////////////////////////////
    //
       int pad_width = getmaxx(win_content) - 4;    // screen width minus borders minus margs
       int pad_height = 5000;                       // lines are cheap in ncurses

        if (content_pad)
            delwin(content_pad);

        content_pad = newpad(pad_height, pad_width);
    //
    /////////////////////////////////////////////////////////////////////////////////////////

        // Header info
        mvwprintw(win_header, header_line ++, 2, "Record: %s / %d | %s |", 
            sqlite3_column_text(result, 0),
            records, 
            sqlite3_column_text(result, 1));

        mvwprintw(win_header, header_line ++, 2, "Lat: %s | Lon: %s |", 
            sqlite3_column_text(result, 2), 
            sqlite3_column_text(result, 3)); 

        mvwprintw(win_header, header_line ++, 2, "Unit: %s | Type: %s |",
            sqlite3_column_text(result, 4), 
            sqlite3_column_text(result, 5));
        
        mvwprintw(win_header, header_line ++, (max_width - 7) / 2, "[ %s ]",
            sqlite3_column_text(result, 6));

        header_line = 1;
        
        // Casualties
//        mvwprintw(win_header, header_line +1,    max_width - 50, "CAS:");

        mvwprintw(win_header, header_line ++, max_width - 45, "Coalition :               WIA: %s, KIA: %s", 
            sqlite3_column_text(result, 7), 
            sqlite3_column_text(result, 8));

        mvwprintw(win_header, header_line ++, max_width - 45, "Host      :               WIA: %s, KIA: %s", 
            sqlite3_column_text(result, 9), 
            sqlite3_column_text(result, 10));

        mvwprintw(win_header, header_line ++, max_width - 45, "Enemy     :  Detained: %s, WIA: %s, KIA: %s", 
            sqlite3_column_text(result, 13), 
            sqlite3_column_text(result, 11), 
            sqlite3_column_text(result, 12));

        content_line ++;
        
        // Word wrap the title
        const char *title = (const char *)sqlite3_column_text(result, 14);
        int max_width = getmaxx(win_content) - 6;
        print_wrapped(content_pad, &content_line, 4, max_width, title, current_search);

        content_line ++;

        // Word wrap the summary
        const char *summary = (const char *)sqlite3_column_text(result, 15);
        print_wrapped(content_pad, &content_line, 4, max_width, summary, current_search);

        content_total_lines = content_line;
        content_pad_lines = content_line;
        content_scroll = 0;

        // Status hint for scrollable window
        if (content_total_lines > content_height) {
            updateStatus("[ Up/Dn: scroll current document | <- prev | -> next | [ENTER] to end scroll mode | q quit browser ]");

       }else if (current_search[0] == '\0') {
                updateStatus("[ Up/Dn : pre/next record | <-/-> : -/+100 | PgDn/PgUp : +/-1000 | /,s: search | q quit browser ]");

            }else{
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

//    updateHeader();
    
    sqlite3_finalize(result);
    sqlite3_close(db);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
/// searchCollect()  Search query that returns multiple rows
//
typedef struct {
    int rowid;
    char class[16];         // record classification
    char date[16];
    char title[128];
    char summary[500];      // summary preview size
} SearchResult;

int searchCollect(const char *term, SearchResult *out, int max_results) {
    const char *sql =
        "SELECT _rowid_, Classification, DateOccurred, Title, Summary "
        "FROM afg "
        "WHERE Title LIKE ? OR Summary LIKE ? "
        "ORDER BY _rowid_ "
        "LIMIT ?";

    char search_string[300];
    snprintf(search_string, sizeof(search_string), "%%%s%%", term);

    if (sqlite3_open("../afg.db", &db) != SQLITE_OK)

        return 0;

    if (sqlite3_prepare_v2(db, sql, -1, &result, NULL) != SQLITE_OK) {
        sqlite3_close(db);

        return 0;
    }

    sqlite3_bind_text(result, 1, search_string, -1, SQLITE_TRANSIENT);      // classification (doesn't work)           
    sqlite3_bind_text(result, 2, search_string, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(result,  3, max_results);

    int count = 0;

    while (sqlite3_step(result) == SQLITE_ROW && count < max_results) {
        out[count].rowid = sqlite3_column_int(result, 0);                   // rowid

        snprintf(out[count].class, sizeof(out[count].class), "%s",          // classification (doesn't work)
                 sqlite3_column_text(result, 1)); 

        snprintf(out[count].date, sizeof(out[count].date), "%s",            // date / time
                 sqlite3_column_text(result, 2));

        snprintf(out[count].title, sizeof(out[count].title), "%s",          // title
                 sqlite3_column_text(result, 3));

        snprintf(out[count].summary, sizeof(out[count].summary), "%s",      // summary
                 sqlite3_column_text(result, 4));
       

        count ++;
    }

    sqlite3_finalize(result);
    sqlite3_close(db);

    return count;
}

///////////////////////////////////////////////////////////////////////////////
/// searchByText() - Search in Title and Summary
//
int searchByText(const char *term, int start_after) {
    const char *sql =
        "SELECT _rowid_ FROM afg "
        "WHERE (Title LIKE ? OR Summary LIKE ?) "
        "AND _rowid_ > ? "
        "ORDER BY _rowid_ LIMIT 1";

    char search_string[300];
    snprintf(search_string, sizeof(search_string), "%%%s%%", term);

    if (sqlite3_open("../afg.db", &db) != SQLITE_OK)

        return -1;

    if (sqlite3_prepare_v2(db, sql, -1, &result, NULL) != SQLITE_OK) {
        sqlite3_close(db);

        return -1;
    }

    sqlite3_bind_text(result, 1, search_string, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(result, 2, search_string, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(result,  3, start_after);

    int found = -1;

    if (sqlite3_step(result) == SQLITE_ROW)
        found = sqlite3_column_int(result, 0);

    sqlite3_finalize(result);
    sqlite3_close(db);

    return found;
}

///////////////////////////////////////////////////////////////////////////////
/// Search results browser -> win_sresult 
//
int browseSearchResults(const char *term) {
    SearchResult results[10000];
    int total = searchCollect(term, results, 10000);
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    if (win_sresult) {
        delwin(win_sresult);
        win_sresult = NULL;
    }

    if (total == 0) {
        updateStatus("[ No results found ]");

        return 0;
    }

    // Size the popup window
    int win_width = max_x - 20;                     // Dynamic width of the popup
    int win_height = max_y - 10;                    // Dynamic height of the popup
    // Create the popup window
    win_sresult = newwin(win_height, win_width, 
                         (max_y - win_height) / 2,  // Center screen
                         (max_x - win_width) / 2);  // Center screen

    box(win_sresult, 0, 0);                         // Draw the popup window border
    wrefresh(win_sresult);                          // Refresh the popup window

    curs_set(1);                                    // Show cursor for search navigation

    // Enable keypad input for win_sresult to capture arrow keys
    keypad(win_sresult, TRUE);

    int selected = 0;                               // Index of currently selected result
    int current_page = 0;                           // Current page of results
    int results_per_page = win_height - 15;         // How many results to show per page
    int total_pages = (total + results_per_page - 1) / results_per_page;    // Total pages

    while (1) {
        werase(win_sresult);                        // Clear the popup window
        box(win_sresult, 0, 0);                     // Redraw the border

        // Display the search results in the top part of the window
        mvwprintw(win_sresult, 1, 2, "Search results for: \"%s\" (%d)", term, total);
        mvwprintw(win_sresult, 2, 2, "Page %d / %d Record %d / %d", current_page + 1, total_pages, selected + 1, total);  // Page info

        int start_idx = current_page * results_per_page;
        int end_idx = start_idx + results_per_page;

        if (end_idx > total) end_idx = total;

        int line = 4;                               // Start from the 4th line for the search results

        for (int i = start_idx; i < end_idx; i ++) {
            int page_index = i - start_idx;         // 0..results_per_page-1

            if (page_index == selected)
                wattron(win_sresult, A_REVERSE);

            mvwprintw(win_sresult, line ++, 2,
                      "[%d] %s  %s %.*s",           // print * characters from the db title column
                      results[i].rowid,
                      results[i].class,
                      results[i].date,
                      win_width - 40,               // dim title dynamically
                      results[i].title);

            if (page_index == selected)
                wattroff(win_sresult, A_REVERSE);
        }

        // Display the preview in the bottom part of the window
        mvwprintw(win_sresult, win_height - 10, 2, "Summary preview: ");  // Preview at bottom of window in the 2nd pos

        // Wrap and display the selected summary
        int wrap_line = win_height - 8;             // Line for wrapped text, 1 line after "Summary preview:"
        const char *summary = results[selected].summary;
        
        // Use print_wrapped() to wrap the summary text
        int max_width = getmaxx(win_sresult) - 6;   // Width of the window minus some padding
        print_wrapped(win_sresult, &wrap_line, 4, max_width, summary, term);

        wrefresh(win_sresult);                      // Refresh the popup window

        updateStatus("[ Up/Dn: select | Enter: open | PgUp/PgDn prev/next page | q cancel ]");

        int ch = wgetch(win_sresult);

        // Handle key presses in the search results popup window
        if (ch == 'q' || ch == 27) {                // Q or ESC to exit
            keypad(win_sresult, FALSE);
            endwin();
            wrefresh(win_content);                  // Refresh the popup window
            keypad(win_content, TRUE);  

            return 0;
        }

        int items_on_page = end_idx - start_idx;

        if (ch == KEY_UP) {
            if (selected > 0) {
                selected --;

            } else if (current_page > 0) {
                current_page --;
                selected = results_per_page - 1;
            }
        }

        else if (ch == KEY_DOWN) {

            if (selected < items_on_page - 1) {
                selected ++;

            } else if (current_page < total_pages - 1) {
                current_page ++;
                selected = 0;
            }
        }

        else if (ch == KEY_PPAGE && current_page > 0) {
            current_page --;
            selected = 0;
        }

        else if (ch == KEY_NPAGE && current_page < total_pages - 1) {
            current_page ++;
            selected = 0;
        }

        else if (ch == '\n') {                                          // Enter to select a result
            record_nr = results[selected + current_page * results_per_page].rowid;  // Adjust for current page
            strcpy(current_search, term);
            last_search_rowid = record_nr;
            showFullRecord(record_nr);                                  // Show the full record of the selected result

            return 1;
        }

        else if (ch == 's' || ch == '/') {
            char buf[256] = "";

            if (getTextInput("Search (Title/Summary):", buf, sizeof(buf)) > 0) {
                strcpy(current_search, buf);
                last_search_rowid = 0;

                /* Clean up this search window before leaving */
                delwin(win_sresult);
                win_sresult = NULL;

                /* Tell caller to start a new search */
                strcpy((char *)term, buf);  // or store globally

                return SEARCH_NEW_QUERY;
            }
        }
    }
    curs_set(0);                            // Hide cursor after exiting
}

///////////////////////////////////////////////////////////////////////////////
/// getRecordByNr()
//
int getRecordByNr() {
    char input[32];
    snprintf(input, sizeof(input), "%d", record_nr);
    
    if (getTextInput("Enter record number:", input, sizeof(input)) > 0) {
        int new_rec = atoi(input);
        if (new_rec >= 1 && new_rec <= records) {
            showFullRecord(new_rec);
            updateStatus("[ Press '?' for help ]");

        } else {
            updateStatus("[ Invalid record number ]");
        }
    }
    
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
/// browse()
//
int browse() {
    in_scroll = 0;
    updateStatus("[ Press '?' for help | 'q' to quit ]");
//    updateHeader();

    while ((ch = getch()) != 'q') {
        int old_record = record_nr;
        
        switch(ch) {
            case KEY_HOME:
                record_nr = 1;
                break;
                
            case KEY_END:
                record_nr = records;
                break;

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  CONSTRUCTION ZONE 

            case KEY_DOWN:

                if (content_scroll + (getmaxy(win_content) - 2) < content_pad_lines) {
                    content_scroll += 5;
                    in_scroll = 1;
                    refresh_content_pad();

                } else if (record_nr < records && content_scroll <= 0) {      // no record change in scrollable 
                    in_scroll = 0;
                    record_nr ++;
                    showFullRecord(record_nr);
                }
                break;

            case KEY_UP:

                if (content_scroll > 0) {
                    content_scroll -= 5;
                    if (content_scroll < 0) content_scroll = 0;
                    refresh_content_pad();

                } else if (record_nr > 1 && in_scroll == 0) {           // no record change in scrollable
                    record_nr --;
                    showFullRecord(record_nr);
                }
                break;

            case KEY_LEFT:

                if (content_scroll > 0 && in_scroll == 1) {             // KEY LEFT = previous in scroll (not optimal)
                    record_nr --;

                    }else{
                        record_nr = (record_nr > 100) ? record_nr - 100 : 1;
                }
                break;
                
            case KEY_RIGHT:

                if (content_scroll > 0) {                               // KEY RIGHT = next in scroll
                    record_nr ++;

                    }else{
                        record_nr = (record_nr + 100 <= records) ? record_nr + 100 : records;
                }
                break;
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////
                
            case KEY_PPAGE:
                record_nr = (record_nr > 1000) ? record_nr - 1000 : 1;
                break;
                
            case KEY_NPAGE:
                record_nr = (record_nr + 1000 <= records) ? record_nr + 1000 : records;
                break;
                
            case '/':  // Search
            case 's':  
                {
                    char buf[256] = "";
                    if (getTextInput("Search (Title/Summary):", buf, sizeof(buf)) > 0) {
                        strcpy(current_search, buf);
                        last_search_rowid = 0;
                        int rc = browseSearchResults(buf);

                        if (rc == SEARCH_NEW_QUERY) {
                            browseSearchResults(current_search);
                        }
                    }
                    break;
                }

            case '#':
            case 'g':  // Go to record
                getRecordByNr();
                break;
            
            case 'n':  // Next result after search
                {
                    if (current_search[0] == '\0') {
                        updateStatus("[ No active search ]");
                        break;
                    }

                    int found = searchByText(current_search, last_search_rowid);

                    if (found > 0) {
                        last_search_rowid = found;
                        record_nr = found;
                        showFullRecord(record_nr);
                        updateStatus("[ n: Next result ]");

                    }else{
                        updateStatus("[ No more results ]");
                    }
                    break;
                }

            case ' ':
            case '\n':
                // Space or Enter - go to next record
                if (record_nr < records) {
                    in_scroll = 0;
                    record_nr ++;
                    showFullRecord(record_nr);
                }
                break;
                
            default:
                updateStatus("[ Invalid key. Press '?' for help ]");
                break;

            case '?':  // Help
            case 'h':
                {
                    werase(win_content);
                    box(win_content, 0, 0);
                    int line = 1;
                    mvwprintw(content_pad, line ++, 2, "KEYBOARD SHORTCUTS:");
                    line ++;
                    mvwprintw(content_pad, line ++, 4, "Navigation:");
                    mvwprintw(content_pad, line ++, 6, "↑/↓       : Previous/Next record");
                    mvwprintw(content_pad, line ++, 6, "←/→       : -100/+100 records");
                    mvwprintw(content_pad, line ++, 6, "PgUp/PgDn : -1000/+1000 records");
                    mvwprintw(content_pad, line ++, 6, "Home/End  : First/Last record");
                    line ++;
                    mvwprintw(content_pad, line ++, 4, "Actions:");
                    mvwprintw(content_pad, line ++, 6, "/ or s    : Search text");
                    mvwprintw(content_pad, line ++, 6, "# or g    : Go to record number");
                    mvwprintw(content_pad, line ++, 6, "? or h    : Show this help");
                    mvwprintw(content_pad, line ++, 6, "n         : Next result (after search)");
                    mvwprintw(content_pad, line ++, 6, "q         : Quit");
                    line ++;
                    mvwprintw(content_pad, line ++, 2, "Note: Use 'n' to navigate through multiple search results.");
                    wrefresh(win_content);
                    updateStatus("[ Press any key to return to browser ]");
                    getch();
                    break;
                }    
        }
        
        // Only update content if record changed
        if (record_nr != old_record) {
            showFullRecord(record_nr);
        }
    }
    
    // Cleanup
    cleanupWindows();
    endwin();
    
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
/// main()
//
int main() {

    // Initialize ncurses
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    
    // Initialize windows
    initWindows();
    
    // Initiate colors
    start_color();
    use_default_colors();
    init_pair(1, COLOR_YELLOW, -1);     // highlight

    // Count records
    if (countRecords() != 0) {
        endwin();
        fprintf(stderr, "Failed to count records\n");

        return 1;
    }
    
    // Show first record
    if (records > 0) {
        showFullRecord(1);

    } else {

        updateStatus("[ No records found in database ]");
    }
    
    // Start browse mode
    record_nr = 0;
    browse();

    db_close();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  CONSTRUCTION ZONE 
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////
