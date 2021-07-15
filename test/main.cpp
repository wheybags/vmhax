#include <cstdio>
#include "../pinned.h"

#ifdef _MSC_VER
#define DEBUG_BREAK() __debugbreak()
#else
#include <csignal>
#define DEBUG_BREAK() raise(SIGTRAP)
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CHECK(X) \
  do \
  { \
    if (!(X)) \
    { \
      fprintf(stderr, "ASSERTION FAILED: (" #X ") in " __FILE__ ":" TOSTRING(__LINE__)); \
      DEBUG_BREAK(); \
    } \
  } while(false)


void test_pinned_basic()
{
  pinned_alloc_info allocation = {};
  CHECK(pinned_alloc(512, PINNED_MAXSIZE_HUGE, &allocation) == 0);
  CHECK(allocation.size >= 512);

  for (size_t i = 0; i < allocation.size; i++)
    ((char*)allocation.data)[i] = (char)(i % 256);

  pinned_free(&allocation);
}

void test_pinned_grow()
{
  pinned_alloc_info allocation = {};
  CHECK(pinned_alloc(512, PINNED_MAXSIZE_HUGE, &allocation) == 0);

  for (size_t i = 0; i < allocation.size; i++)
    ((char*)allocation.data)[i] = (char)(i % 256);

  size_t old_size = allocation.size;
  void* old_ptr = allocation.data;
  CHECK(pinned_realloc(allocation.size * 2, &allocation) == 0);
  CHECK(old_ptr == allocation.data);

  for (size_t i = 0; i < old_size; i++)
    CHECK(((char*)allocation.data)[i] == (char)(i % 256));

  pinned_free(&allocation);
}

void test_grow_from_empty()
{
  pinned_alloc_info allocation = {};
  CHECK(pinned_alloc(0, PINNED_MAXSIZE_HUGE, &allocation) == 0);

  CHECK(pinned_realloc(512, &allocation) == 0);
  CHECK(allocation.size >= 512);

  for (size_t i = 0; i < allocation.size; i++)
    ((char*)allocation.data)[i] = (char)(i % 256);

  pinned_free(&allocation);
}

void test_shrink()
{
  pinned_alloc_info allocation = {};
  CHECK(pinned_alloc(512, PINNED_MAXSIZE_HUGE, &allocation) == 0);
  CHECK(pinned_realloc(allocation.size * 2, &allocation) == 0);

  for (size_t i = 0; i < allocation.size; i++)
    ((char*)allocation.data)[i] = (char)(i % 256);

  CHECK(pinned_realloc(allocation.size / 2, &allocation) == 0);

  for (size_t i = 0; i < allocation.size; i++)
    CHECK(((char*)allocation.data)[i] == (char)(i % 256));

  pinned_free(&allocation);
}

int main()
{
  test_pinned_basic();
  test_pinned_grow();
  test_grow_from_empty();
  test_shrink();

  fputs("All tests passed!\n", stderr);
  return 0;
}
