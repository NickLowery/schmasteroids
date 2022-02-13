#ifndef SCHM_MAIN_H

#define internal static

#include <math.h>
#include <stdio.h>
#include <float.h>
#include <stdlib.h>
#include <stdint.h>
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef i32 bool32;


#define PI 3.141592f
#define Kilobytes(n) (n*1024LL)
#define Megabytes(n) (Kilobytes(n)*1024LL)
#define Gigabytes(n) (Megabytes(n)*1024LL)
#define Terabytes(n) (Gigabytes(n)*1024LL)

#if ASSERTIONS
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else 
#define Assert(Expression) 
#endif
#define InvalidCodePath Assert(!"Code should be unreachable")
#define InvalidDefault default: Assert(!"Default should not be reached")

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

// TODO: Eventually calculate this based on what we actually need.
#define PERMANENT_STORE_SIZE Megabytes(64)
#define TRANSIENT_STORE_SIZE Megabytes(64)
#define GAME_MEMORY_SIZE (PERMANENT_STORE_SIZE + TRANSIENT_STORE_SIZE)







typedef struct _memory_arena {
    size_t Size;
    size_t Occupied;
    u8* Base;
    u8 TempCount;
} memory_arena;

inline void
InitializeArena(memory_arena *Arena, size_t Size, void *Base) 
{
    Arena->Size = Size;
    Arena->Base = (u8*)Base;
    Arena->Occupied = 0;
    Arena->TempCount = 0;
}

inline void
ResetArena(memory_arena *Arena)
{
    Arena->Occupied = 0;
    Assert(Arena->TempCount == 0);
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
#define PushSize(Arena, size) PushSize_(Arena, size)
inline void*
PushSize_(memory_arena *Arena, size_t Size) 
{
    void *Result;
    Assert(Arena->Occupied + Size <= Arena->Size);
    if (Arena->Occupied + Size <= Arena->Size) {
        Result = Arena->Base + Arena->Occupied;
        Arena->Occupied += Size;
    } else {
        InvalidCodePath;
        Result = 0;
    }

    return Result;
}

#define PushStructAligned(Arena, type, Align) (type *)PushSizeAligned_(Arena, sizeof(type), Align)
#define PushArrayAligned(Arena, Count, type, Align) (type *)PushSizeAligned_(Arena, (Count)*sizeof(type), Align)
#define PushSizeAligned(Arena, size, Align) PushSizeAligned_(Arena, size, Align)
inline void*
PushSizeAligned_(memory_arena *Arena, size_t Size, u32 Align)
{
    void *Result;
    size_t SizeToSkip = 0;
    u32 Mod = ((size_t)(Arena->Base) + Arena->Occupied) % Align;
    if (Mod) {
        SizeToSkip = (Align - Mod);
    }
    Assert(((u64)(Arena->Base) + Arena->Occupied + SizeToSkip) % Align == 0);
    if (Arena->Occupied + SizeToSkip + Size <= Arena->Size) { 
        Result = Arena->Base + Arena->Occupied + SizeToSkip;
        Arena->Occupied += SizeToSkip + Size;
    } else {
        InvalidCodePath;
        Result = 0;
    }

    return Result;
}

inline void
AssertAligned(void *Memory, u32 Align)
{
    Assert((size_t)Memory % Align == 0);
}

typedef struct {
    memory_arena *Arena;
    size_t Occupied;
} temporary_memory;

inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena) 
{
    temporary_memory Result;
    Result.Arena = Arena;
    Result.Occupied = Arena->Occupied;
    Arena->TempCount++;
    return Result;
}

inline void
EndTemporaryMemory(temporary_memory Temp)
{
    Assert(Temp.Arena->Occupied >= Temp.Occupied);
    Temp.Arena->Occupied = Temp.Occupied;
    Assert(Temp.Arena->TempCount > 0);
    Temp.Arena->TempCount--;
}

inline void
CheckArenaNotTemporary(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

#define ZeroArray(Array, type) ZeroSize_(Array, ArrayCount(Array)*sizeof(type))
#define ZeroSize(StartAddress, Size) ZeroSize_(StartAddress, Size)
inline void ZeroSize_(void* StartAddress, size_t Size)
{
    memset(StartAddress, 0, Size);
}

inline bool32
UpdateAndCheckTimer(float *Timer, float SecondsElapsed)
{
    *Timer -= SecondsElapsed;
    bool32 TimeUp = (*Timer <= 0.0f);
    return TimeUp;
}

#define SCHM_MAIN_H 1
#endif
