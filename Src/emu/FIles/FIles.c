#include "Files.h"

/* Reads the entire binary contents of a file into a dynamically allocated buffer and returns the file size. */
size_t read_raw_data(const char* file_path, void** out_ptr) {
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        perror("Read Error");
        *out_ptr = NULL;
        return 0;
    }

    // Determine file size
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    // Allocate the "blank" memory
    void* buffer = malloc(size);
    if (!buffer) {
        perror("Allocation Error");
        fclose(file);
        *out_ptr = NULL;
        return 0;
    }

    // Read data and clean up
    fread(buffer, 1, size, file);
    fclose(file);

    // Set the pointer provided by the caller to our new buffer
    *out_ptr = buffer;
    
    return size; 
}

/* Writes a specific amount of binary data to a file, returning 0 on success. */
int write_raw_data(const char* file_path, const void* data, size_t size) {
    if (!data && size > 0) return -1;

    FILE* file = fopen(file_path, "wb");
    if (!file) {
        perror("Write Error");
        return -1;
    }

    size_t written = fwrite(data, 1, size, file);
    fclose(file);

    return (written == size) ? 0 : -1;
}