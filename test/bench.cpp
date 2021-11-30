#include <vector>
#include <chrono>
#include <cassert>
#include "../pinned.h"

template <typename Vec>
auto bench(size_t initialCapacity, uint64_t iterations)
{
  auto start = std::chrono::high_resolution_clock::now();

  Vec v;
  v.reserve(initialCapacity);
  assert(v.capacity() == initialCapacity);

  for (uint64_t i = 0; i < iterations; i++)
  {
    v.push_back(uint32_t(i));
    if (v.size() == v.capacity())
    {
      size_t newCapacity = v.capacity() * 2;
      v.reserve(newCapacity);
      assert(v.capacity() == newCapacity);
    }
  }

  auto duration = std::chrono::high_resolution_clock::now() - start;
  return duration;
}

void benchMegabytes(size_t initialCapacity, uint32_t megabytes)
{
  constexpr uint64_t megabyte = 1024 * 1024;

  printf("# %u MiB\n", megabytes);
  printf("std::vector: %lld ms\n", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(bench<std::vector<uint32_t>>(initialCapacity, (megabyte * megabytes) / sizeof(uint32_t))).count());
  printf("pinned_vec:  %lld ms\n", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(bench<pinned_vec<uint32_t>>(initialCapacity, (megabyte * megabytes) / sizeof(uint32_t))).count());
  puts("");
}

void benchKilobytes(size_t initialCapacity, int32_t kilobytes)
{
  constexpr int32_t kilobyte = 1024;

  double stdVecVal = 0;
  double pinnedVecVal = 0;

  int32_t iterations = 20;
  for (int32_t i = 0; i < iterations; i++)
  {
    stdVecVal += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(bench<std::vector<uint32_t>>(initialCapacity, (kilobyte * kilobytes) / sizeof(uint32_t))).count();
    pinnedVecVal += (double)std::chrono::duration_cast<std::chrono::nanoseconds>(bench<pinned_vec<uint32_t>>(initialCapacity, (kilobyte * kilobytes) / sizeof(uint32_t))).count();
  }

  stdVecVal /= iterations;
  pinnedVecVal /= iterations;

  printf("# %dKiB\n", kilobytes);
  printf("std::vector: %lld ns\n", (long long)stdVecVal);
  printf("pinned_vec:  %lld ns\n", (long long)pinnedVecVal);
  puts("");
}

int main(int, char**)
{
  // pinned_vec capacity is always page-aligned, so use the same start for std::vector to be fair
  size_t initialCapacity = 0;
  {
    pinned_vec<uint32_t> temp;
    temp.reserve(512);
    initialCapacity = temp.capacity();
  }

  benchMegabytes(initialCapacity, 4096);
  benchMegabytes(initialCapacity, 1024);
  benchMegabytes(initialCapacity, 512);
  benchMegabytes(initialCapacity, 16);

  benchKilobytes(initialCapacity, 2048);
  benchKilobytes(initialCapacity, 1024);
  benchKilobytes(initialCapacity, 512);
  benchKilobytes(initialCapacity, 16);
  benchKilobytes(initialCapacity, 1);

  return 0;
}