#ifndef TYPES_H
#define TYPES_H

#include <sqlite3.h>
#include <ncurses.h>

#define BORDER 				2      // Border calc factor for calculating window sizes
#define SEARCH_EXIT      	0      // Search return value
#define SEARCH_OPEN_REC  	1      // Search return value
#define SEARCH_NEW_QUERY 	2      // Search return value

#define MAX_SEARCH_LEN		256

extern sqlite3 *db;
extern sqlite3_stmt *result;

extern WINDOW *win_header;
extern WINDOW *win_content;
extern WINDOW *win_status;
extern WINDOW *win_input;			// can stay NULL most of the time
extern WINDOW *content_pad;
extern WINDOW *win_sresult;

extern int record_nr;
extern int records;

#endif	