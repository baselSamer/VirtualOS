#ifndef PROJECT_ETHOS_FILES_H
#define PROJECT_ETHOS_FILES_H

#include <stdio.h>
#include <stdlib.h>

size_t read_raw_data(const char* file_path, void** out_ptr);
int write_raw_data(const char* file_path, const void* data, size_t size);

#endif