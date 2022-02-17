#ifndef SCHM_RENDER_BUFFER_H
#define SCHM_RENDER_BUFFER_H


enum render_entry_type {
    RenderEntryType_Invalid = 0,
    RenderEntryType_render_entry_glyph,
    RenderEntryType_render_entry_object,
    RenderEntryType_render_entry_particle,
    RenderEntryType_render_entry_particle_group,
    RenderEntryType_render_entry_pixel_rect,
    RenderEntryType_render_entry_pixel_line,
    RenderEntryType_render_entry_pixel_glyph,
    RenderEntryType_render_entry_clear,
};
typedef struct {
    render_entry_type Type;
    glyph* Glyph;
    light_source Light;
    rect Rect;
} render_entry_glyph;

typedef struct {
    render_entry_type Type;
    v2 Vertices[MAX_VERTICES];
    u32 VerticesCount;
    light_source Light;
} render_entry_object;

typedef struct {
    render_entry_type Type;
    v2 Position;
    light_source Light;
} render_entry_particle;

// NOTE: I'm trying having this be followed IN THE BUFFER by an array of v2s and an array of ZDistSq
typedef struct {
    render_entry_type Type;
    float H;
    float S;
    float C_L; //NOTE: Store this as a light_source? If we are not varying the parameters for different
                // particles at least.
    //i32 BlockCount;
    i32 ParticleCount;
    float MinZDistSq;
} render_entry_particle_group;

#if DEBUG_BUILD
typedef struct {
    render_entry_type Type;
    color Color;
    rect Rect;
} render_entry_pixel_rect;

typedef struct {
    render_entry_type Type;
    color Color;
    v2 First;
    v2 Second;
} render_entry_pixel_line;

typedef struct {
    render_entry_type Type;
    color Color;
    glyph *Glyph;
    rect Rect;
} render_entry_pixel_glyph;
#endif

typedef struct {
    render_entry_type Type;
    color Color;
} render_entry_clear;

typedef struct {
    u8* BaseMemory;
    u32 Size;
    u32 MaxSize;
} render_buffer;

internal render_buffer 
AllocateRenderBuffer(memory_arena *Arena, u32 Size)
{
    render_buffer Result;
    Result.BaseMemory = (u8*)PushSize(Arena, Size);
    Result.MaxSize = Size;
    Result.Size = 0;
    return Result;
}

internal render_entry_type* 
PushRenderEntry_(render_buffer *RenderBuffer, u32 EntrySize, render_entry_type Type) {
    Assert(RenderBuffer->Size + EntrySize <= RenderBuffer->MaxSize);
    render_entry_type *Result = (render_entry_type*)(RenderBuffer->BaseMemory + RenderBuffer->Size);
    RenderBuffer->Size += EntrySize;
    *Result = Type;
    return Result;
}
#define PushRenderEntry(RenderBuffer, type) (type*)PushRenderEntry_(RenderBuffer, sizeof(type), RenderEntryType_##type)


internal void 
PushObject(render_buffer *RenderBuffer, object *Object, float Alpha = 1.0f)
{
    render_entry_object *Entry = PushRenderEntry(RenderBuffer, render_entry_object);
    Entry->VerticesCount = Object->VerticesCount;
    for(int VIndex = 0; VIndex < Object->VerticesCount; VIndex++) {
        v2 MappedVertex = MapFromObjectPosition(Object->Vertices[VIndex], *Object);
        Entry->Vertices[VIndex] = MappedVertex;
    }

    Entry->Light = Object->Light;
    Entry->Light.C_L *= Alpha;
}

internal void 
PushGlyph(render_buffer *RenderBuffer, glyph* Glyph, light_source Light, rect Rect, float Alpha = 1.0f)
{
    render_entry_glyph *Entry = PushRenderEntry(RenderBuffer, render_entry_glyph);
    Entry->Glyph = Glyph;
    Entry->Light = Light;
    Entry->Light.C_L *= Alpha;
    Entry->Rect = Rect;
}

internal void 
PushParticle(render_buffer *RenderBuffer, particle* Particle, float Alpha = 1.0f)
{
    render_entry_particle *Entry = PushRenderEntry(RenderBuffer, render_entry_particle);
    Entry->Position = Particle->Position;
    Entry->Light = Particle->Light;
    Entry->Light.C_L *= Alpha;
}

typedef struct {
    u32 Position;
    u32 ZDistSq;
    u32 Total;
} particle_data_sizes;

inline particle_data_sizes
GetParticleDataSizes(i32 ParticleCount)
{
    particle_data_sizes Result;
    Result.Position = ParticleCount * sizeof(v2);
    Result.ZDistSq = ParticleCount * sizeof(float);
    Result.Total = Result.Position + Result.ZDistSq;
    return Result;
}

internal void
PushParticleGroup(render_buffer *RenderBuffer, particle_group* Group, float C_L, float Alpha = 1.0f)
{
    render_entry_particle_group *Entry = PushRenderEntry(RenderBuffer, render_entry_particle_group);
    Entry->H = Group->H;
    Entry->S = Group->S;
    Entry->C_L = C_L * Alpha;
    Entry->ParticleCount = Group->TotalParticles;

    particle_data_sizes DataSizeInfo = GetParticleDataSizes(Entry->ParticleCount);

    Assert(RenderBuffer->Size + DataSizeInfo.Total <= RenderBuffer->MaxSize);
    v2 *Position = (v2*)(RenderBuffer->BaseMemory + RenderBuffer->Size);
    float *ZDistSq = (float*)((u8*)Position + DataSizeInfo.Position);
    RenderBuffer->Size += DataSizeInfo.Total;

    Entry->MinZDistSq = FLT_MAX;


    for(particle_block *Block = Group->FirstBlock;
        Block;
        Block = Block->NextBlock)
    {
        for (i32 PIndex = 0;
             PIndex < Block->Count;
             ++PIndex)
        {
            *Position++ = Block->Position[PIndex];
            *ZDistSq++ = Block->ZDistSq[PIndex];
            Entry->MinZDistSq = Min(Entry->MinZDistSq, Block->ZDistSq[PIndex]);
        }
    }
}

internal void 
PushClear(render_buffer *RenderBuffer, color Color)
{
    render_entry_clear *Entry = PushRenderEntry(RenderBuffer, render_entry_clear);
    Entry->Color = Color;
}

#if DEBUG_BUILD
internal void
PushPixelRect(render_buffer *RenderBuffer, color Color, rect Rect)
{
    render_entry_pixel_rect *Entry = PushRenderEntry(RenderBuffer, render_entry_pixel_rect);
    Entry->Rect = Rect;
    Entry->Color = Color;
}

internal void
PushPixelLine(render_buffer *RenderBuffer, color Color, v2 First, v2 Second)
{
    render_entry_pixel_line *Entry = PushRenderEntry(RenderBuffer, render_entry_pixel_line);
    Entry->Color = Color;
    Entry->First = First;
    Entry->Second = Second;
}
internal void
PushPixelGlyph(render_buffer *RenderBuffer, color Color, glyph *Glyph, rect Rect)
{
    render_entry_pixel_glyph *Entry = PushRenderEntry(RenderBuffer, render_entry_pixel_glyph);
    Entry->Color = Color;
    Entry->Glyph = Glyph;
    Entry->Rect = Rect;
}
#include "schm_pixelateddraw.h"
#endif



#endif
