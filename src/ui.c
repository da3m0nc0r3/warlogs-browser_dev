// ui.c
// #define _GNU_SOURCE

#include <string.h>
#include "types.h"

void cleanupWindows() {
    if (win_header) delwin(win_header);
    if (win_content) delwin(win_content);
    if (win_status) delwin(win_status);
    if (win_input) delwin(win_input);
    if (win_sresult) delwin(win_sresult);
}

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

void updateStatus(const char *message) {
    werase(win_status);
    box(win_status, 0, 0);
    mvwprintw(win_status, 1, 2, "%s", message);
    wrefresh(win_status);
}

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
