#pragma once

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>

#include "../minesweeper.h"

#define MINESWEEPER_SETTINGS_FILE_VERSION 1
#define PROFILING_FILE_DIRECTORY_PATH "apps_data/f0_mine_sweeper/f0_mine_sweeper_profiling.txt"

#define TAG_PROFILER "Mine Sweeper Profiler"

Stream* open_profiling_file();
bool append_to_profiling_file(Stream* fs, const char* format, ...);
void close_profiling_file(Stream* fs);
