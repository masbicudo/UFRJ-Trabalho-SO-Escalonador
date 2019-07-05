#ifndef _SAFE_ALLOC_H
#define _SAFE_ALLOC_H

void* safe_malloc(int size, void* parent);
void* safe_free(void* ptr, void* parent);

#endif