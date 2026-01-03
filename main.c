//main.c

#include <sqlite3.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"
#include "ui.h"
#include "db.h"
#include "display.h"



unsigned int ch;
// int record_nr = 1;
// int records;
int last_search_rowid = 0;      // Store last search row ID
char current_search[256] = "";  // For the 'n' case



///////////////////////////////////////////////////////////////////////////////
/// function declarations
//
int showFullRecord(int rec_nr);
int browseSearchResults(const char *term);
int searchByText(const char *term, int start_after);



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
                char buf[MAX_SEARCH_LEN] = "";

                if (getTextInput("Search (Title/Summary):", buf, sizeof(buf)) > 0) {
                    strcpy(current_search, buf);
                    last_search_rowid = 0;

                    int num_matches = searchByText(current_search, 0);

                    if (num_matches > 0) {
                        int selected_row = browseSearchResults(current_search);

                        if (selected_row > 0) {
                            record_nr = selected_row;
                            last_search_rowid = record_nr;
                            showFullRecord(record_nr);

                            if (current_search[0] != '\0') {
                                char status_buf[256];
                                snprintf(status_buf, sizeof(status_buf),
                                         "[ Record %d / %d | Search: \"%s\" | n: next match ]",
                                         record_nr, records, current_search);
                                updateStatus(status_buf);
                            } else {
                                updateStatus("[ Press '?' for help | 'q' to quit ]");
                            }
                        }
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
                } else {
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
