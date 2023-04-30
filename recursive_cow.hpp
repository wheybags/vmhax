#pragma once
#include <cstdint>
#include <windows.h>

void setupRecursiveCow(size_t mappingSize);
uint8_t* createNewGeneration(size_t generationSize, void* parentAddr = nullptr);
void destroyGeneration(void* address);

LONG recursiveCowExceptionFilter(_EXCEPTION_POINTERS * ExceptionInfo);
size_t getChunkSize();
size_t alignToChunkSize(size_t i);
int32_t getUsedMappingChunkCount();