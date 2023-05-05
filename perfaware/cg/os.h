#ifndef _OS_H
#define _OS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "string.h"

string_t read_entire_file(arena_t *arena, char *filename);

#endif // _OS_H
