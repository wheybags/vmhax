#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pinned_alloc_info
{
  void* data;
  size_t size;
  size_t max_size;
} pinned_alloc_info;

// You must pick a maximum size for your allocation, which will also determine how many allocations you can create.
// Virtual memory is big, but it is not infinite, and it's probably not the full 64 bits you might expect either.
// For example, on 64-bit windows the available virtual address space is only 128 TiB, instead of the 16 exabytes
// of a full 64-bit address space.
// Below are a few good options:

// 2^42 (4 TiB), you can probably only have tens of allocations with this max size
# define PINNED_MAXSIZE_HUGE    0x0000040000000000LL

// 2^37 (128 GiB), you can probably have hundreds of allocations with this max size
# define PINNED_MAXSIZE_MEDIUM  0x0000002000000000LL

// 2^34 (16 GiB), you can probably have thousands of allocations with this max size
# define PINNED_MAXSIZE_SMALL   0x0000000400000000LL

int pinned_alloc(size_t size, size_t max_size, pinned_alloc_info* allocation);
int pinned_realloc(size_t new_size, pinned_alloc_info* allocation);
void pinned_free(pinned_alloc_info* allocation);

// Example use:
//
// pinned_alloc_info allocation;
// int ret = pinned_alloc(1024, PINNED_MAXSIZE_SMALL, &allocation);
// assert(ret);
// do_stuff_with_buffer(allocation.data, allocation.size);
//
// size_t needed_size = calc_size_needed();
// if (needed_size > allocation->size)
//{
//  ret = pinned_realloc(&allocation, needed_size);
//  assert(ret);
//}
// do_more_stuff_with_bigger_buffer(allocation.data, allocation.size);
//
// pinned_free(&allocation);

#ifdef __cplusplus
};
#endif