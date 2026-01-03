#ifndef UI_H
#define UI_H

#include "types.h"

void initWindows(void);
void cleanupWindows(void);
void updateHeader(void);
void updateStatus(const char *message);
int getTextInput(const char *prompt, char *buffer, int max_len);

void refresh_content_pad(void);
void scroll_content(int delta);

// Highlight & wrap helpers (used by display too)
void print_highlighted(WINDOW *win, int y, int x, 
				const char *text, const char *term);
void print_wrapped(WINDOW *win, int *line, int start_x,
				int max_width, const char *text, const char *highlight);

#endif
