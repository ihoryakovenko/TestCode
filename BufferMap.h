#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include <Util/EngineTypes.h>

typedef void (*System_PoolAllocator_OnFreeDelegate)(void* data);

struct PoolAllocator
{
    void* Data;
    void* RawData;
    u32* FreeList;
    u64 FreeCount;
    u64 FreeCapacity;
    u64 Count;
    u64 capacity;
    u32 DataSize;
    u32 Alignment;
};

void Systems_PoolAllocator_Init(PoolAllocator* Allocator, u64 InitialCapacity, u32 DataSize, u32 Alignment = 1);
void Systems_PoolAllocator_Free(PoolAllocator* Allocator, System_PoolAllocator_OnFreeDelegate OnFreeDelegate);
u32 Systems_PoolAllocator_PushData(PoolAllocator* Allocator, const void* Data);
void Systems_PoolAllocator_GetData(PoolAllocator* Allocator, u32 Index, void* OutData);
void Systems_PoolAllocator_FreeData(PoolAllocator* Allocator, u32 Index);

struct SparceHashMap
{
    u64* Keys;
    u32* Indices;
    u8* ProbeDist;
    u8* Occupied;
    u64 Capacity;
    u64 Count;
};

void Systems_SparceHashMap_Init(SparceHashMap* Map, u64 InitialCapacity);
void Systems_SparceHashMap_Free(SparceHashMap* Map);
void Systems_SparceHashMap_Insert(SparceHashMap* Map, u64 Key, u32 Index);
bool Systems_SparceHashMap_Get(const SparceHashMap* Map, u64 Key, u32* OutIndex);
bool Systems_SparceHashMap_Remove(SparceHashMap* Map, u64 Key, u32* OutIndex);