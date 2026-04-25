#ifndef PROJECT_ETHOS_FILES_H
#define PROJECT_ETHOS_FILES_H

#include <stdio.h>
#include <stdlib.h>

/* Reads the entire binary contents of a file into a dynamically allocated buffer and returns the file size. */
size_t read_raw_data(const char* file_path, void** out_ptr);
/* Writes a specific amount of binary data to a file, returning 0 on success. */
int write_raw_data(const char* file_path, const void* data, size_t size);

#endif