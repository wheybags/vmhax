#include "test.h"

void test_c_pinned_basic()
{
  pinned_alloc_info allocation;
  CHECK(pinned_alloc(512, PINNED_MAXSIZE_HUGE, &allocation) == 0);
  CHECK(allocation.size >= 512);

  for (size_t i = 0; i < allocation.size; i++)
    ((char*)allocation.data)[i] = (char)(i % 256);

  pinned_free(&allocation);
}

void test_c_pinned_grow()
{
  pinned_alloc_info allocation;
  CHECK(pinned_alloc(512, PINNED_MAXSIZE_LARGE, &allocation) == 0);

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

void test_c_grow_from_empty()
{
  pinned_alloc_info allocation;
  CHECK(pinned_alloc(0, PINNED_MAXSIZE_NORMAL, &allocation) == 0);

  CHECK(pinned_realloc(512, &allocation) == 0);
  CHECK(allocation.size >= 512);

  for (size_t i = 0; i < allocation.size; i++)
    ((char*)allocation.data)[i] = (char)(i % 256);

  pinned_free(&allocation);
}

void test_c_shrink()
{
  pinned_alloc_info allocation;
  CHECK(pinned_alloc(512, PINNED_MAXSIZE_HUGE, &allocation) == 0);
  CHECK(pinned_realloc(allocation.size * 2, &allocation) == 0);

  for (size_t i = 0; i < allocation.size; i++)
    ((char*)allocation.data)[i] = (char)(i % 256);

  CHECK(pinned_realloc(allocation.size / 2, &allocation) == 0);

  for (size_t i = 0; i < allocation.size; i++)
    CHECK(((char*)allocation.data)[i] == (char)(i % 256));

  pinned_free(&allocation);
}

void run_c_tests()
{
  test_c_pinned_basic();
  test_c_pinned_grow();
  test_c_grow_from_empty();
  test_c_shrink();
}