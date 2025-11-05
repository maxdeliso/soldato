// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

// Game of Life constants
#ifndef GOL_TIMER_ID
#define GOL_TIMER_ID 2
#endif

#ifndef GOL_TIMER_RATE
#define GOL_TIMER_RATE 0
#endif

#ifndef GOL_MIN_CELL_SIZE
#define GOL_MIN_CELL_SIZE 3
#endif