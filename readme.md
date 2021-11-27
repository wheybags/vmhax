# vmhax
Experiments with virtual memory.

## pinned.h
This is an allocator / vector class that retains the same base pointer when you reallocate.
### Example
```c
pinned_alloc_info allocation;
int ret = pinned_alloc(1024, PINNED_MAXSIZE_SMALL, &allocation);
assert(ret);
do_stuff_with_buffer(allocation.data, allocation.size);

void* pointer_inside_buffer = ((char*)allocation.data) + 20);

ret = pinned_realloc(&allocation, 8192);
assert(ret);

do_more_stuff_with_bigger_buffer(allocation.data, allocation.size);
do_more_stuff_with_old_pointer(pointer_inside_buffer); // valid, because pinned_realloc does not reallocate pointers

pinned_free(&allocation);
```

When using the header from c++, it also defines a `pinned_vec` template class, that works like `std::vector`
### C++ example
```c++
pinned_vec<int> v;
for (int i = 0; i < 10; i++)
  v.push_back(i);

int* p = &v[5];

for (int i = 0; i < 10; i++)
  v.push_back(i); // iterators / pointers *not* invalidated
  
do_something_with(p); // not an error
```