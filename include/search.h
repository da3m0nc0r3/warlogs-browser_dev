// search.h

#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

// Search for a term in the database starting after 'start_after' record
int searchByText(const char *term, int start_after);

// Browse search results, returns the selected record number or 0 to exit
int browseSearchResults(const char *term);

#endif
