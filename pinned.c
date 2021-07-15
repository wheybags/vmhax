#include "pinned.h"
#include <assert.h>

_Static_assert(sizeof(void*) >= 8, "This ain't gonna work unless you have way more address space than you need");

static size_t align_size(size_t val, size_t block_size)
{
  size_t block_count = val / block_size;
  if (block_count * block_size < val)
    block_count++;
  return block_count * block_size;
}

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include "Windows.h"

#pragma comment(lib, "mincore")

int pinned_alloc(size_t size, size_t max_size, pinned_alloc_info* allocation)
{
  int err = 0;
  void* base_pointer = NULL;

  // Reserve (without committing) a huge region in virtual memory. Not committing means we don't use any physical ram, just address space
  base_pointer = VirtualAlloc2(NULL, NULL, max_size, MEM_RESERVE, PAGE_READWRITE, NULL, 0);
  if (!base_pointer)
  {
    err = (int)GetLastError();
    goto on_error;
  }

  allocation->data = base_pointer;
  allocation->size = 0;
  allocation->max_size = max_size;

  // commit only the region we need immediately
  err = pinned_realloc(size, allocation);
  if (err != 0)
    goto on_error;

  goto ok;

on_error:
  if (base_pointer)
  {
    BOOL success = VirtualFree(base_pointer, 0, MEM_RELEASE);
    assert(success);
  }

ok:
  return err;
}

int pinned_realloc(size_t new_size, pinned_alloc_info* allocation)
{
  if (new_size > allocation->max_size)
    return ERROR_INVALID_PARAMETER;

  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  size_t aligned_size = align_size(new_size, system_info.dwAllocationGranularity);

  if (aligned_size < allocation->size)
  {
    // Decommit pages when shrinking
    if (!VirtualFree(((char*)allocation->data) + aligned_size, allocation->size - aligned_size, MEM_DECOMMIT))
      return (int) GetLastError();
  }
  else if (aligned_size > 0)
  {
    // Commit pages when growing
    if (!VirtualAlloc2(NULL, allocation->data, aligned_size, MEM_COMMIT, PAGE_READWRITE, NULL, 0))
      return (int) GetLastError();
  }

  allocation->size = aligned_size;

  return 0;
}

void pinned_free(pinned_alloc_info* allocation)
{
  BOOL success = VirtualFree(allocation->data, 0, MEM_RELEASE);
  assert(success);
}

#endif // _WIN32