#include <thread>
#include "test.h"
#include "../recursive_cow.hpp"

void testBasic()
{
  size_t size = getChunkSize() * 4;

  uint8_t* gen1 = createNewGeneration(size);

  CHECK(getUsedMappingChunkCount() == alignToChunkSize(size) / getChunkSize());

  for (size_t i = 0; i < size; i++)
    gen1[i] = 0xFE;

  for (size_t i = 0; i < size; i++)
    CHECK(gen1[i] == 0xFE);

  uint8_t* gen2 = createNewGeneration(size, gen1);

  CHECK(getUsedMappingChunkCount() == alignToChunkSize(size) / getChunkSize());

  for (size_t i = 0; i < size; i++)
    CHECK(gen1[i] == 0xFE);

  for (size_t i = 0; i < size; i++)
    CHECK(gen2[i] == 0xFE);


  // write to gen2
  for (size_t i = size/2; i < size; i++)
    gen2[i] = 0xFF;

  CHECK(getUsedMappingChunkCount() == alignToChunkSize(size / 2) / getChunkSize() + alignToChunkSize(size) / getChunkSize());


  for (size_t i = 0; i < size; i++)
    CHECK(gen1[i] == 0xFE);

  for (size_t i = 0; i < size; i++)
  {
    if (i < size/2)
      CHECK(gen2[i] == 0xFE);
    else
      CHECK(gen2[i] == 0xFF);
  }


  // write to gen1
  for (size_t i = 0; i < size/2; i++)
    gen1[i] = 0x10;

  CHECK(getUsedMappingChunkCount() == alignToChunkSize(size)*2 / getChunkSize());


  for (size_t i = 0; i < size; i++)
  {
    if (i < size/2)
      CHECK(gen1[i] == 0x10);
    else
      CHECK(gen1[i] == 0xFE);
  }

  for (size_t i = 0; i < size; i++)
  {
    if (i < size / 2)
      CHECK(gen2[i] == 0xFE);
    else
      CHECK(gen2[i] == 0xFF);
  }

  destroyGeneration(gen1);

  CHECK(getUsedMappingChunkCount() == alignToChunkSize(size) / getChunkSize());

  for (size_t i = 0; i < size; i++)
  {
    if (i < size / 2)
      CHECK(gen2[i] == 0xFE);
    else
      CHECK(gen2[i] == 0xFF);
  }

  for (size_t i = 0; i < size; i++)
    gen2[i] = 0x11;

  for (size_t i = 0; i < size; i++)
    CHECK(gen2[i] == 0x11);

  CHECK(getUsedMappingChunkCount() == alignToChunkSize(size) / getChunkSize());

  destroyGeneration(gen2);
  CHECK(getUsedMappingChunkCount() == 0);
}

void testMultithread()
{
  size_t size = getChunkSize() * 4096;

  uint8_t* gen1 = createNewGeneration(size);

  CHECK(getUsedMappingChunkCount() == alignToChunkSize(size) / getChunkSize());

  for (size_t i = 0; i < size; i++)
    gen1[i] = 0xFE;

  volatile long threadState = 0;

  uint8_t* gen2 = nullptr;
  std::thread t2([&]()
  {
    __try
    {
      gen2 = createNewGeneration(size, gen1);

      InterlockedIncrement(&threadState);
      while (threadState != 2)
      {}

      // write to gen2
      for (size_t i = size / 2; i < size; i++)
        gen2[i] = 0xFF;
    }
     __except (recursiveCowExceptionFilter(GetExceptionInformation())) {}
  });


  while(threadState != 1) {}
  InterlockedIncrement(&threadState);

  // write to gen1
  for (size_t i = 0; i < size/2; i++)
    gen1[i] = 0x10;

  t2.join();

  CHECK(getUsedMappingChunkCount() == alignToChunkSize(size)*2 / getChunkSize());

  for (size_t i = 0; i < size; i++)
  {
    if (i < size/2)
      CHECK(gen1[i] == 0x10);
    else
      CHECK(gen1[i] == 0xFE);
  }

  for (size_t i = 0; i < size; i++)
  {
    if (i < size / 2)
      CHECK(gen2[i] == 0xFE);
    else
      CHECK(gen2[i] == 0xFF);
  }
}

int main()
{
  setupRecursiveCow(1024ULL * 1024ULL * 1024ULL * 5ULL);
  __try
  {
    testBasic();
    testMultithread();
  }
   __except (recursiveCowExceptionFilter(GetExceptionInformation())) {}
}