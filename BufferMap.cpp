#include "BufferMap.h"

#include <Engine/Systems/Memory/forge_memory_debugger.h>
#include <stdint.h>

static u32 AlignUp(u32 Value, u32 Alignment)
{
	return (Value + Alignment - 1) & ~(Alignment - 1);
}

static void* AlignPointer(void* Ptr, u32 Alignment)
{
	uintptr_t Addr = (uintptr_t)Ptr;
	uintptr_t AlignedAddr = (Addr + Alignment - 1) & ~(uintptr_t)(Alignment - 1);
	return (void*)AlignedAddr;
}

void Systems_PoolAllocator_Init(PoolAllocator* Allocator, u64 InitialCapacity, u32 DataSize, u32 Alignment)
{
	Allocator->FreeList = nullptr;
	Allocator->FreeCount = 0;
	Allocator->FreeCapacity = 0;
	Allocator->Count = 0;
	Allocator->capacity = InitialCapacity;
	Allocator->DataSize = DataSize;
	Allocator->Alignment = Alignment;
	
	u32 Stride = AlignUp(DataSize, Allocator->Alignment);
	u64 TotalSize = InitialCapacity * Stride + Allocator->Alignment - 1;
	Allocator->RawData = calloc(1, TotalSize);
	Allocator->Data = AlignPointer(Allocator->RawData, Allocator->Alignment);
}

void Systems_PoolAllocator_Free(PoolAllocator* Allocator, System_PoolAllocator_OnFreeDelegate OnFreeDelegate)
{
	if (OnFreeDelegate != nullptr && Allocator->Data != nullptr)
	{
		u32 Stride = AlignUp(Allocator->DataSize, Allocator->Alignment);
		
		for (u64 i = 0; i < Allocator->Count; ++i)
		{
			bool IsFree = false;
			for (u64 j = 0; j < Allocator->FreeCount; ++j)
			{
				if (Allocator->FreeList[j] == (u32)i)
				{
					IsFree = true;
					break;
				}
			}
			
			if (!IsFree)
			{
				void* DataPtr = (char*)Allocator->Data + i * Stride;
				OnFreeDelegate(DataPtr);
			}
		}
	}
	
	free(Allocator->RawData);
	free(Allocator->FreeList);
}

u32 Systems_PoolAllocator_PushData(PoolAllocator* Allocator, const void* Data)
{
	u32 Stride = AlignUp(Allocator->DataSize, Allocator->Alignment);
	
	if (Allocator->FreeCount > 0)
	{
		u32 Index = Allocator->FreeList[--Allocator->FreeCount];
		memcpy((char*)Allocator->Data + Index * Stride, Data, Allocator->DataSize);
		return Index;
	}

	if (Allocator->Count >= Allocator->capacity)
	{
		Allocator->capacity *= 2;
		u64 TotalSize = Allocator->capacity * Stride + Allocator->Alignment - 1;
		void* NewRawData = calloc(1, TotalSize);
		void* NewData = AlignPointer(NewRawData, Allocator->Alignment);
		
		// Copy existing data
		for (u64 i = 0; i < Allocator->Count; ++i)
		{
			memcpy((char*)NewData + i * Stride, (char*)Allocator->Data + i * Stride, Allocator->DataSize);
		}
		
		free(Allocator->RawData);
		Allocator->RawData = NewRawData;
		Allocator->Data = NewData;
	}

	memcpy((char*)Allocator->Data + Allocator->Count * Stride, Data, Allocator->DataSize);
	return (u32)(Allocator->Count++);
}

void Systems_PoolAllocator_GetData(PoolAllocator* Allocator, u32 Index, void* OutData)
{
	u32 Stride = AlignUp(Allocator->DataSize, Allocator->Alignment);
	memcpy(OutData, (char*)Allocator->Data + Index * Stride, Allocator->DataSize);
}

void Systems_PoolAllocator_FreeData(PoolAllocator* Allocator, u32 Index)
{
	if (Allocator->FreeCount >= Allocator->FreeCapacity)
	{
		Allocator->FreeCapacity = Allocator->FreeCapacity ? Allocator->FreeCapacity * 2 : 8;
		Allocator->FreeList = (u32*)realloc(Allocator->FreeList, Allocator->FreeCapacity * sizeof(u32));
	}
	Allocator->FreeList[Allocator->FreeCount++] = Index;
}

// -----------------------------------------------------------------------------
// BufferMap implementation

static u64 HashVkHandle(u64 VkHandle)
{
	VkHandle >>= 3; // Drop low bits if handles are aligned
	VkHandle ^= VkHandle >> 30; VkHandle *= UINT64_C(0xBF58476D1CE4E5B9);
	VkHandle ^= VkHandle >> 27; VkHandle *= UINT64_C(0x94D049BB133111EB);
	VkHandle ^= VkHandle >> 31;
	return VkHandle;
}

static inline u64 GetIndexFromHandleMask(u64 h, u64 Capacity)
{
	assert((Capacity & (Capacity - 1)) == 0); // Power of two required
	u64 Mask = Capacity - 1;
	return HashVkHandle(h) & Mask;
}

static inline u64 NextIndex(u64 i, u64 Mask)
{
	return (i + 1) & Mask;
}

static void SparceHashMap_Resize(SparceHashMap* Map)
{
	u64 OldCapacity = Map->Capacity;
	u64* OldKeys = Map->Keys;
	u32* OldIndices = Map->Indices;
	u8* OldProbe = Map->ProbeDist;
	u8* OldOccupied = Map->Occupied;

	u64 NewCapacity = OldCapacity * 2;
	Systems_SparceHashMap_Init(Map, NewCapacity);

	for (u64 i = 0; i < OldCapacity; ++i)
	{
		if (OldOccupied[i])
		{
			Systems_SparceHashMap_Insert(Map, OldKeys[i], OldIndices[i]);
		}
	}

	free(OldKeys);
	free(OldIndices);
	free(OldProbe);
	free(OldOccupied);
}

void Systems_SparceHashMap_Init(SparceHashMap* Map, u64 InitialCapacity)
{
	assert((InitialCapacity & (InitialCapacity - 1)) == 0);
	Map->Capacity = InitialCapacity;
	Map->Count = 0;
	Map->Keys = (u64*)calloc(InitialCapacity, sizeof(u64));
	Map->Indices = (u32*)calloc(InitialCapacity, sizeof(u32));
	Map->ProbeDist = (u8*)calloc(InitialCapacity, sizeof(u8));
	Map->Occupied = (u8*)calloc(InitialCapacity, sizeof(u8));
}

void Systems_SparceHashMap_Free(SparceHashMap* Map)
{
	free(Map->Keys);
	free(Map->Indices);
	free(Map->ProbeDist);
	free(Map->Occupied);
}

void Systems_SparceHashMap_Insert(SparceHashMap* Map, u64 Key, u32 Index)
{
	const float LoadFactor = 0.8f;
	if (Map->Count + 1 > (u64)(Map->Capacity * LoadFactor))
	{
		SparceHashMap_Resize(Map);
	}

	u64 Mask = Map->Capacity - 1;
	u64 i = GetIndexFromHandleMask(Key, Map->Capacity);
	u8 Dist = 0;

	while (true)
	{
		if (!Map->Occupied[i])
		{
			Map->Keys[i] = Key;
			Map->Indices[i] = Index;
			Map->ProbeDist[i] = Dist;
			Map->Occupied[i] = 1;
			Map->Count++;
			return;
		}

		if (Map->Keys[i] == Key)
		{
			Map->Indices[i] = Index;
			return;
		}

		if (Map->ProbeDist[i] < Dist)
		{
			u64 tmp_Key = Map->Keys[i];
			u32 tmp_Index = Map->Indices[i];
			u8 tmp_Dist = Map->ProbeDist[i];

			Map->Keys[i] = Key;
			Map->Indices[i] = Index;
			Map->ProbeDist[i] = Dist;

			Key = tmp_Key;
			Index = tmp_Index;
			Dist = tmp_Dist;
		}

		i = NextIndex(i, Mask);
		Dist++;
	}
}

bool Systems_SparceHashMap_Get(const SparceHashMap* Map, u64 Key, u32* OutIndex)
{
	u64 Mask = Map->Capacity - 1;
	u64 i = GetIndexFromHandleMask(Key, Map->Capacity);
	u8 Dist = 0;

	while (true)
	{
		if (!Map->Occupied[i] || Map->ProbeDist[i] < Dist)
		{
			return false;
		}

		if (Map->Keys[i] == Key)
		{
			*OutIndex = Map->Indices[i];
			return true;
		}

		i = NextIndex(i, Mask);
		Dist++;
	}
}

bool Systems_SparceHashMap_Remove(SparceHashMap* Map, u64 Key, u32* OutIndex)
{
	u64 Mask = Map->Capacity - 1;
	u64 i = GetIndexFromHandleMask(Key, Map->Capacity);
	u8 Dist = 0;

	while (true)
	{
		if (!Map->Occupied[i] || Map->ProbeDist[i] < Dist)
		{
			return false;
		}

		if (Map->Keys[i] == Key)
		{
			*OutIndex = Map->Indices[i];
			Map->Occupied[i] = 0;

			// Backward-shift deletion
			u64 j = NextIndex(i, Mask);
			while (Map->Occupied[j] && Map->ProbeDist[j] > 0)
			{
				Map->Keys[i] = Map->Keys[j];
				Map->Indices[i] = Map->Indices[j];
				Map->ProbeDist[i] = Map->ProbeDist[j] - 1;
				Map->Occupied[i] = 1;

				Map->Occupied[j] = 0;
				i = j;
				j = NextIndex(j, Mask);
			}

			Map->Count--;
			return true;
		}

		i = NextIndex(i, Mask);
		Dist++;
	}
}
