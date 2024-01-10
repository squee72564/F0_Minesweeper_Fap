#include "mine_sweeper_storage.h"


static Storage* mine_sweeper_profiler_open_storage() {
    return furi_record_open(RECORD_STORAGE);
}

static void mine_sweeper_profiler_close_storage() {
    furi_record_close(RECORD_STORAGE);
}


Stream* open_profiling_file() {

    Storage* storage = mine_sweeper_profiler_open_storage();
	
	
    // Overwrite wont work, so delete first
    if(storage_file_exists(storage, PROFILING_FILE_SAVE_PATH)) {
        storage_simply_remove(storage, PROFILING_FILE_SAVE_PATH);
    }

    // Open File, create if not exists
    if(!storage_common_stat(storage, PROFILING_FILE_SAVE_PATH, NULL) == FSE_OK) {
        FURI_LOG_I(TAG, "Config file %s is not found. Will create new.", PROFILING_FILE_SAVE_PATH);
        if(storage_common_stat(storage, PROFILING_FILE_DIRECTORY_PATH, NULL) == FSE_NOT_EXIST) {
            FURI_LOG_I(
                TAG,
                "Directory %s doesn't exist. Will create new.",
                PROFILING_FILE_DIRECTORY_PATH);
            if(!storage_simply_mkdir(storage, PROFILING_FILE_DIRECTORY_PATH)) {
                FURI_LOG_E(TAG, "Error creating directory %s", PROFILING_FILE_DIRECTORY_PATH);
            }
        }
    }

    Stream* file_stream = file_stream_alloc(storage);

    FS_Error err;

    if (!file_stream_open(file_stream, PROFILING_FILE_SAVE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND) ) {
        err = file_stream_get_error(file_stream);
        FURI_LOG_E(TAG_PROFILER, "Error opening file %s: %s",PROFILING_FILE_SAVE_PATH, filesystem_api_error_get_desc(err));
        file_stream_close(file_stream);
        return NULL;
    }

    if (file_stream_get_error(file_stream) != FSE_OK) {
        err = file_stream_get_error(file_stream);
        FURI_LOG_E(TAG_PROFILER, "Error opening file: %s", filesystem_api_error_get_desc(err));
        file_stream_close(file_stream);
        return NULL;
    }

    mine_sweeper_profiler_close_storage();

    return file_stream;
}

bool append_to_profiling_file(Stream* fs, const char* format, ...) {
    furi_assert(fs);
    furi_assert(format);

    size_t size;
    va_list args;
    va_start(args, format);
    size = stream_write_vaformat(fs, format, args);
    va_end(args);

    return size;
}

void close_profiling_file(Stream* fs) {
    furi_assert(fs);

    if (!file_stream_close(fs) ) {
        FS_Error err = file_stream_get_error(fs);
        FURI_LOG_E(TAG_PROFILER, "Error closing file: %s", filesystem_api_error_get_desc(err));
    }

    return;
}
