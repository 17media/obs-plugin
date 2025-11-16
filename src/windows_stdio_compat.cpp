// Windows stdio compatibility layer for MbedTLS with static runtime
// This provides the dynamic runtime symbols that MbedTLS expects when using static runtime

#ifdef _WIN32

#include <stdio.h>
#include <stdlib.h>

// Provide the __imp_* symbols that MbedTLS expects for dynamic runtime
// These redirect to the static runtime equivalents

extern "C" {

// stdio function compatibility
void __cdecl __imp_setbuf(FILE* stream, char* buffer) {
    setbuf(stream, buffer);
}

int __cdecl __imp_ferror(FILE* stream) {
    return ferror(stream);
}

int __cdecl __imp_remove(const char* filename) {
    return remove(filename);
}

// Additional stdio functions that might be needed
int __cdecl __imp_fclose(FILE* stream) {
    return fclose(stream);
}

FILE* __cdecl __imp_fopen(const char* filename, const char* mode) {
    return fopen(filename, mode);
}

size_t __cdecl __imp_fread(void* buffer, size_t size, size_t count, FILE* stream) {
    return fread(buffer, size, count, stream);
}

size_t __cdecl __imp_fwrite(const void* buffer, size_t size, size_t count, FILE* stream) {
    return fwrite(buffer, size, count, stream);
}

// Memory function compatibility
void* __cdecl __imp_calloc(size_t num, size_t size) {
    return calloc(num, size);
}

void __cdecl __imp_free(void* memblock) {
    free(memblock);
}

void* __cdecl __imp_malloc(size_t size) {
    return malloc(size);
}

void* __cdecl __imp_realloc(void* memblock, size_t size) {
    return realloc(memblock, size);
}

} // extern "C"

#endif // _WIN32