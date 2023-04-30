#include <cstdio>
#include "recursive_cow.hpp"

#pragma comment(lib, "onecore.lib")

#define release_assert(X) do { if (!(X)) { if (IsDebuggerPresent()) DebugBreak(); abort();} } while(false)

struct Generation
{
  Generation* parent;
  Generation* child;
  BYTE* base;
  size_t size;
  CRITICAL_SECTION lock;
  size_t chunkIndices[1]; // variable size
};

static size_t chunkSize = 0;

static constexpr size_t MAX_GENERATION_COUNT = 256;
static Generation* generations[MAX_GENERATION_COUNT] = {};
static SRWLOCK generationTableLock = SRWLOCK_INIT;

static HANDLE mapping = nullptr;
static LARGE_INTEGER mappingSize = {};
static volatile long* mappingPagesRefcounts = nullptr;

size_t getChunkSize()
{
  return chunkSize;
}

size_t alignToChunkSize(size_t i)
{
  size_t chunks = i / chunkSize;
  if (chunks * chunkSize < i)
    chunks++;
  return chunks * chunkSize;
}

static size_t getNewChunkFromMapping(size_t startSearchAtIndex = 0)
{
  size_t mappingChunkIndex = startSearchAtIndex;

  bool found = false;
  while (true)
  {
    if (mappingPagesRefcounts[mappingChunkIndex] == 0)
    {
      if (InterlockedCompareExchange(&mappingPagesRefcounts[mappingChunkIndex], 1, 0) == 0)
      {
        found = true;
        break;
      }
    }
    mappingChunkIndex++;
  }
  release_assert(found);

  return mappingChunkIndex;
}


static LPTOP_LEVEL_EXCEPTION_FILTER previous = nullptr;
LONG recursiveCowExceptionFilter(_EXCEPTION_POINTERS * ExceptionInfo)
{
  if (ExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION && ExceptionInfo->ExceptionRecord->ExceptionInformation[0] == 1)
  {
    ULONG_PTR address = ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
    ULONG_PTR chunkAddress = (address / chunkSize) * chunkSize;

    AcquireSRWLockShared(&generationTableLock);

    Generation* generation = nullptr;
    for (int i = 0; i < MAX_GENERATION_COUNT; i++)
    {
      if (!generations[i])
        continue;

      ULONG_PTR generationBase = (ULONG_PTR)generations[i]->base;
      if (chunkAddress >= generationBase && chunkAddress < generationBase + generations[i]->size)
      {
        generation = generations[i];
        break;
      }
    }

    if (!generation)
    {
      ReleaseSRWLockShared(&generationTableLock);
      if (previous)
        return previous(ExceptionInfo);
      return EXCEPTION_EXECUTE_HANDLER;
    }


    Generation* firstGen = nullptr;
    for (Generation* it = generation; it != nullptr; it = it->parent)
    {
      if (it->parent == nullptr)
      {
        firstGen = it;
        break;
      }
    }

    EnterCriticalSection(&firstGen->lock);
    ReleaseSRWLockShared(&generationTableLock);

    BYTE* generationChunk = (BYTE*)chunkAddress;
    size_t generationChunkIndex = (chunkAddress - ((ULONG_PTR)generation->base)) / chunkSize;

    size_t shareCount = mappingPagesRefcounts[generation->chunkIndices[generationChunkIndex]];

    if (shareCount == 1)
    {
      DWORD oldProtect = {};
      release_assert(VirtualProtect(generation->base + (generationChunkIndex * chunkSize), chunkSize, PAGE_READWRITE, &oldProtect));
    }
    else
    {
      size_t newChunkIndex = getNewChunkFromMapping();

      // temporarily map the new chunk somewhere and copy the old data in
      BYTE* tempMapping = (BYTE*)MapViewOfFile3(mapping, nullptr, nullptr, newChunkIndex * chunkSize, chunkSize, 0, PAGE_READWRITE, nullptr, 0);
      memcpy(tempMapping, generationChunk, chunkSize);
      release_assert(UnmapViewOfFile(tempMapping));

      // remap the new chunk into our generation + update bookkeeping
      InterlockedDecrement(&mappingPagesRefcounts[generation->chunkIndices[generationChunkIndex]]);
      generation->chunkIndices[generationChunkIndex] = newChunkIndex;
      release_assert(UnmapViewOfFile2(GetCurrentProcess(), generation->base + (generationChunkIndex * chunkSize), MEM_PRESERVE_PLACEHOLDER));
      release_assert(MapViewOfFile3(mapping, nullptr, generationChunk, newChunkIndex * chunkSize, chunkSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0) == generationChunk);
    }

    LeaveCriticalSection(&firstGen->lock);

    return EXCEPTION_CONTINUE_EXECUTION;
  }

  if (previous)
    return previous(ExceptionInfo);
  return EXCEPTION_EXECUTE_HANDLER;
}

void setupRecursiveCow(size_t _mappingSize)
{
  SYSTEM_INFO systemInfo = {};
  GetSystemInfo(&systemInfo);
  chunkSize = systemInfo.dwAllocationGranularity;

  uint64_t chunks = _mappingSize / chunkSize;
  if (chunks * chunkSize < size_t(mappingSize.QuadPart))
    chunks++;
  mappingSize.QuadPart = chunks * chunkSize;

  mappingPagesRefcounts = (long*)calloc(chunks, sizeof(long));

  mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, mappingSize.HighPart, mappingSize.LowPart, nullptr);
  release_assert(mapping);

  previous = SetUnhandledExceptionFilter(recursiveCowExceptionFilter);
}


uint8_t* createNewGeneration(size_t generationSize, void* parentAddr)
{
  AcquireSRWLockExclusive(&generationTableLock);

  // align generationSize
  {
    size_t chunks = generationSize / chunkSize;
    if (chunks * chunkSize < generationSize)
      chunks++;
    generationSize = chunks * chunkSize;
  }

  Generation* firstGen = nullptr;
  Generation* parent = nullptr;
  if (parentAddr)
  {
    Generation* generation = nullptr;
    for (int i = 0; i < MAX_GENERATION_COUNT; i++)
    {
      if (generations[i] && generations[i]->base == parentAddr)
      {
        parent = generations[i];
        break;
      }
    }

    release_assert(parent && !parent->child && generationSize == parent->size);

    for (Generation* gen = parent; gen != nullptr; gen = gen->parent)
    {
      if (gen->parent == nullptr)
      {
        firstGen = gen;
        break;
      }
    }
  }

  if (firstGen)
    EnterCriticalSection(&firstGen->lock);

  Generation* generation = (Generation*)calloc(sizeof(Generation) + ((generationSize / chunkSize) - 1) * sizeof(size_t), 1);
  generation->parent = parent;
  generation->child = nullptr;
  generation->base = nullptr;
  generation->size = generationSize;
  InitializeCriticalSectionAndSpinCount(&generation->lock, 1000);

  for (int i = 0; i < MAX_GENERATION_COUNT; i++)
  {
    if (generations[i] == nullptr)
    {
      generations[i] = generation;
      break;
    }
  }

  if (parent)
    parent->child = generation;

  ReleaseSRWLockExclusive(&generationTableLock);

  generation->base = (BYTE*)VirtualAlloc2(nullptr, nullptr, generationSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
  release_assert(generation->base);

  size_t generationChunkCount = generationSize / chunkSize;

  if (parent)
  {
    for (size_t generationChunkIndex = 0; generationChunkIndex < generationChunkCount; generationChunkIndex++)
    {
      BYTE* generationChunk = generation->base + generationChunkIndex * chunkSize;
      if (!generation->base)
        generation->base = generationChunk;

      if (generationChunkIndex != generationChunkCount - 1)
        release_assert(VirtualFree(generationChunk, chunkSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER));

      size_t mappingChunkIndex = parent->chunkIndices[generationChunkIndex];
      generation->chunkIndices[generationChunkIndex] = mappingChunkIndex;
      InterlockedIncrement(&mappingPagesRefcounts[mappingChunkIndex]);

      release_assert(MapViewOfFile3(mapping, nullptr, generationChunk, mappingChunkIndex * chunkSize, chunkSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0) == generationChunk);

      for (Generation* gen = generation; gen != nullptr; gen = gen->parent)
      {
        if (gen->chunkIndices[generationChunkIndex] == mappingChunkIndex)
        {
          BYTE* chunk = gen->base + generationChunkIndex * chunkSize;
          DWORD oldProtect = {};
          release_assert(VirtualProtect(chunk, chunkSize, PAGE_READONLY, &oldProtect));
        }
      }
    }
  }
  else
  {
    size_t mappingChunkIndex = 0;
    for (size_t generationChunkIndex = 0; generationChunkIndex < generationChunkCount; generationChunkIndex++)
    {
      BYTE* generationChunk = generation->base + generationChunkIndex * chunkSize;
      if (!generation->base)
        generation->base = generationChunk;

      if (generationChunkIndex != generationChunkCount - 1)
        release_assert(VirtualFree(generationChunk, chunkSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER));

      mappingChunkIndex = getNewChunkFromMapping(mappingChunkIndex);
      generation->chunkIndices[generationChunkIndex] = mappingChunkIndex;

      release_assert(MapViewOfFile3(mapping, nullptr, generationChunk, mappingChunkIndex * chunkSize, chunkSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0) == generationChunk);

      DWORD oldProtect = {};
      release_assert(VirtualProtect(generationChunk, chunkSize, PAGE_READWRITE, &oldProtect));
    }
  }

  if (firstGen)
    LeaveCriticalSection(&firstGen->lock);

  return generation->base;
}

void destroyGeneration(void* address)
{
  AcquireSRWLockExclusive(&generationTableLock);

  Generation* generation = nullptr;
  int generationIndex = -1;

  for (int i = 0; i < MAX_GENERATION_COUNT; i++)
  {
    if (generations[i] && generations[i]->base == address)
    {
      generationIndex = i;
      generation = generations[i];
      break;
    }
  }
  release_assert(generation);


  Generation* firstGen = nullptr;
  for (Generation* it = generation; it != nullptr; it = it->parent)
  {
    if (it->parent == nullptr)
    {
      firstGen = it;
      break;
    }
  }

  EnterCriticalSection(&firstGen->lock);

  if (generation->parent)
    generation->parent->child = generation->child;
  if (generation->child)
    generation->child->parent = generation->parent;

  generations[generationIndex] = nullptr;

  LeaveCriticalSection(&firstGen->lock);
  ReleaseSRWLockExclusive(&generationTableLock);

  size_t generationChunkCount = generation->size / chunkSize;
  for (size_t generationChunkIndex = 0; generationChunkIndex < generationChunkCount; generationChunkIndex++)
  {
    BYTE* generationChunk = generation->base + generationChunkIndex * chunkSize;
    release_assert(UnmapViewOfFile(generationChunk));

    size_t mappingIndex = generation->chunkIndices[generationChunkIndex];
    InterlockedDecrement(&mappingPagesRefcounts[mappingIndex]);
  }

  DeleteCriticalSection(&generation->lock);
  free(generation);
}

int32_t getUsedMappingChunkCount()
{
  uint64_t chunkCount = mappingSize.QuadPart / chunkSize;

  int32_t usedCount = 0;
  for (int32_t i = 0; i < chunkCount; i++)
  {
    if (mappingPagesRefcounts[i])
      usedCount++;
  }

  return usedCount;
}
