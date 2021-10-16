#pragma once
#include <stddef.h>

// This header defines two apis: one low-level C interface that just provides memory, and one higher level C++ template class
// designed to resemble std::vector (it can probably be used as a drop-in replacement), layered on top of the low level API.
// In both cases, the whole point is that pointers/iterators into the buffer will not be invalidated on reallocation.
// This means, for example, you can do something like this:
//
//  pinned_vec<int> vec;
//  vec.push_back(1);
//  int* first_int = &vec[0];
//  vec.push_back(2);
//  do_something_with(first_int); // first_int is *not* invalidated, so this is a valid usage
//
// To be clear, this means that the numeric value of the pointer returned by vec.data() *does not change* when you resize the
// vector.

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
# define PINNED_MAXSIZE_LARGE   0x0000002000000000LL

// 2^34 (16 GiB), you can probably have thousands of allocations with this max size
# define PINNED_MAXSIZE_NORMAL  0x0000000400000000LL

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
// void* pointer_inside_buffer = ((char*)allocation.data) + 20);
//
// size_t needed_size = calc_size_needed();
// if (needed_size > allocation->size)
//{
//  ret = pinned_realloc(&allocation, needed_size);
//  assert(ret);
//}
// do_more_stuff_with_bigger_buffer(allocation.data, allocation.size);
// do_more_stuff_with_old_pointer(pointer_inside_buffer); // valid, because pinned_realloc does not reallocate pointers
//
// pinned_free(&allocation);

#ifdef __cplusplus
};

#include <new>
#include <iterator>
#include <stdexcept>

template <typename T>
class pinned_vec
{
public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = T&;
  using const_reference = const T&;
  using pointer = T*;
  using const_pointer = const T*;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  explicit pinned_vec()
  {
    if (pinned_alloc(0, PINNED_MAXSIZE_NORMAL, &allocation) != 0)
      throw std::bad_alloc();
  }

  explicit pinned_vec(size_t count, size_t max_size = PINNED_MAXSIZE_NORMAL)
  {
    if (pinned_alloc(count * sizeof(T), max_size, &allocation) != 0)
      throw std::bad_alloc();

    for (size_t i = 0; i < count; i++)
      new (&data()[i]) T();
    this->count = count;
  }

  pinned_vec(size_type count, const T& value, size_t max_size = PINNED_MAXSIZE_NORMAL)
  {
    if (pinned_alloc(count * sizeof(T), max_size, &allocation) != 0)
      throw std::bad_alloc();

    for (size_t i = 0; i < count; i++)
      new (&data()[i]) T(value);
    this->count = count;
  }

  ~pinned_vec()
  {
    resize(0);
    pinned_free(&allocation);
  }

  reference at(size_type pos)
  {
    if (pos >= size())
      throw std::out_of_range("out of range");
    return data()[pos];
  }

  const_reference at(size_type pos) const
  {
    if (pos >= size())
      throw std::out_of_range("out of range");
    return data()[pos];
  }

  reference operator[](size_type pos) { return data()[pos]; }
  const_reference operator[](size_type pos) const { return data()[pos]; }

  reference front() { return data()[0]; }
  const_reference front() const { return data()[0]; }

  reference back() { return data()[count-1]; }
  const_reference back() const { return data()[count-1]; }

  pointer data() noexcept { return reinterpret_cast<T*>(allocation.data); }
  const_pointer data() const noexcept { return reinterpret_cast<T*>(allocation.data); }

  iterator begin() noexcept { return &data()[0]; }
  const_iterator begin() const noexcept { return &data()[0]; }
  const_iterator cbegin() const noexcept { return &data()[0]; }

  iterator end() noexcept { return &data()[count]; }
  const_iterator end() const noexcept { return &data()[count]; }
  const_iterator cend() const noexcept { return &data()[count]; }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
  const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

  bool empty() const noexcept { return count == 0; }
  size_type size() const noexcept { return count; }
  size_type max_size() const noexcept { return allocation.max_size / sizeof(T); }
  size_type capacity() const noexcept { return allocation.size / sizeof(T); }

  void reserve(size_type new_cap)
  {
    if (new_cap <= capacity())
      return;

    if (pinned_realloc(new_cap * sizeof(T), &allocation) != 0)
      throw std::bad_alloc();
  }

  void shrink_to_fit()
  {
    resize(count);
    if (capacity() != count)
    {
      if (pinned_realloc(count * sizeof(T), &allocation) != 0)
        throw std::bad_alloc();
    }
  }

  void clear() noexcept
  {
    resize(0);
  }

  template<class... Args>
  reference emplace_back(Args&&... args)
  {
    if (count == capacity())
      reserve(size() == 0 ? 1 : size() * 2);

    new (&data()[count]) T(std::forward<Args>(args) ...);
    reference retval = data()[count];
    count++;
    return retval;
  }

  void push_back(const T& value)
  {
    if (count == capacity())
      reserve(size() == 0 ? 1 : size() * 2);

    new (&data()[count]) T(value);
    count++;
  }

  void push_back(const T&& value)
  {
    if (count == capacity())
      reserve(size() == 0 ? 1 : size() * 2);

    new (&data()[count]) T(std::move(value));
    count++;
  }

  void pop_back()
  {
    data()[count-1].~T();
    count--;
  }

  void resize(size_type new_count)
  {
    if (new_count > count)
    {
      for (size_type i = count; i < new_count; i++)
        emplace_back();
    }
    else if (new_count < count)
    {
      for (size_type i = new_count; i < count; i++)
        data()[i].~T();
      count = new_count;
    }
  }

  void resize(size_type new_count, const value_type& value)
  {
    if (new_count > count)
    {
      for (size_type i = count; i < new_count; i++)
        emplace_back(value);
    }
    else if (new_count < count)
    {
      for (size_type i = new_count; i < count; i++)
        data()[i].~T();
      count = new_count;
    }
  }

private:
  pinned_alloc_info allocation = {};
  size_t count = 0;
};
#endif