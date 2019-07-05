#include <stdlib.h>
#include <stdio.h>

void* safe_malloc(int size, void* parent)
{
    return malloc(size);
    int total_size = sizeof(void*) + size;
    void** base_ptr = (void**)malloc(total_size);
    *base_ptr = parent;
    void* ptr = (void*)(base_ptr + 1);
    return ptr;
}

void safe_free(void* ptr, void* parent)
{
    free(ptr);
    return;
    void** base_ptr = (void**)ptr;
    base_ptr--;
    if (*base_ptr != parent)
    {
        printf("Error! Allocation pointers do not match.");
        exit(1);
    }
    free(base_ptr);
}
