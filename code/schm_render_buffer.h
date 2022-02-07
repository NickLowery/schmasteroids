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
    // TODO: Don't need the whole object. Could go back to using a pointer to an object and just
    // get the data needed for the draw call
    object Object;
    light_source Light;
} render_entry_object;

typedef struct {
    render_entry_type Type;
    //TODO: We don't need the whole particle struct for sure
    particle Particle;
    light_source Light;
} render_entry_particle;

// NOTE: I'm trying having this be followed IN THE BUFFER by an array of v2s and an array of ZDistSq
typedef struct {
    render_entry_type Type;
    float H;
    float S;
    float C_L; //TODO: Store this as a light_source? Think about it.
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
    Entry->Object = *Object;
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
    Entry->Particle = *Particle;
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


internal void
Render(render_buffer *RenderBuffer, platform_framebuffer *TargetBuffer, memory_arena *TransientArena) 
{
    temporary_memory Temp = BeginTemporaryMemory(TransientArena);
    rgb_block_buffer SwizzledBuffer;
    SwizzledBuffer.Width = TargetBuffer->Width / 2;
    SwizzledBuffer.Height = TargetBuffer->Height / 2;
    SwizzledBuffer.Pitch = SwizzledBuffer.Width * sizeof(rgba_4);
    SwizzledBuffer.MemorySize = SwizzledBuffer.Pitch * SwizzledBuffer.Height;
    Assert(SwizzledBuffer.MemorySize == TargetBuffer->MemorySize);
    SwizzledBuffer.Memory = (u8*)PushSizeAligned(TransientArena, SwizzledBuffer.MemorySize, 16);
    ZeroSize(SwizzledBuffer.Memory, SwizzledBuffer.MemorySize);

    float LightScale = CalculateLightScale(TargetBuffer);

    for (u8 *NextEntry = RenderBuffer->BaseMemory;
         NextEntry - RenderBuffer->BaseMemory < RenderBuffer->Size;)
    {
        render_entry_type *EntryType = (render_entry_type*)NextEntry;
        switch(*EntryType) {
            case RenderEntryType_render_entry_glyph: {
                render_entry_glyph* Entry = (render_entry_glyph*) EntryType;
                NextEntry += sizeof(render_entry_glyph);

                glyph ProjectedGlyph = ProjectGlyphIntoRect(Entry->Glyph, Entry->Rect);

                for(int SegIndex = 0; SegIndex < ProjectedGlyph.SegCount; ++SegIndex) {
                    ProjectedGlyph.Segs[SegIndex].First = MapGameSpaceToWindowSpace(ProjectedGlyph.Segs[SegIndex].First, TargetBuffer);
                    ProjectedGlyph.Segs[SegIndex].Second = MapGameSpaceToWindowSpace(ProjectedGlyph.Segs[SegIndex].Second, TargetBuffer);
                }
                Entry->Light = ScaleLightSource(&Entry->Light, LightScale);
                DrawGlyphSwizzled(&SwizzledBuffer, &ProjectedGlyph, Entry->Light, TransientArena);
            } break;
            case RenderEntryType_render_entry_object: {
                render_entry_object* Entry = (render_entry_object*) EntryType;
                NextEntry += sizeof(render_entry_object);

                for(int VIndex = 0; VIndex < Entry->Object.VerticesCount; VIndex++) {
                    v2 MappedVertex = MapGameSpaceToWindowSpace(MapFromObjectPosition(Entry->Object.Vertices[VIndex], Entry->Object), TargetBuffer);
                    Entry->Object.Vertices[VIndex] = MappedVertex;
                }
                Entry->Light = ScaleLightSource(&Entry->Light, LightScale);
                DrawObjectSwizzled(&SwizzledBuffer, &Entry->Object, Entry->Light, TransientArena);
            } break;
            case RenderEntryType_render_entry_particle: {
                render_entry_particle* Entry = (render_entry_particle*) EntryType;
                NextEntry += sizeof(render_entry_particle);

                Entry->Particle.Position = MapGameSpaceToWindowSpace(Entry->Particle.Position, TargetBuffer);
                Entry->Light = ScaleLightSource(&Entry->Light, LightScale);
                DrawParticleSwizzled(Entry->Particle.Position, &SwizzledBuffer, Entry->Light, TransientArena);
            } break;
            case RenderEntryType_render_entry_particle_group: {
                //TODO: Try having an l_buffer that uses u8s or u16s? Could get me back to the look of separate buffers
                //and have better performance?
                render_entry_particle_group* Entry = (render_entry_particle_group*) EntryType;
                  particle_data_sizes DataSizeInfo = GetParticleDataSizes(Entry->ParticleCount);
                NextEntry += sizeof(render_entry_particle_group);
                v2 *Positions = (v2*)NextEntry;
                float *ZDistSqs = (float*)(NextEntry + DataSizeInfo.Position);

                NextEntry += DataSizeInfo.Total;
                light_source Light;
                Light.H = Entry->H;
                Light.S = Entry->S;
                Light.C_L = Entry->C_L * LightScale;
#if 1
                for (i32 PIndex = 0;
                     PIndex < Entry->ParticleCount;
                     ++PIndex)
                {
                    v2 Position = MapGameSpaceToWindowSpace(Positions[PIndex], TargetBuffer);
                    Light.ZDistSq = ZDistSqs[PIndex] * LightScale;
                    DrawParticleSwizzled(Position, &SwizzledBuffer, Light, TransientArena);
                }
#else

                rect BoundingRect;
                BoundingRect.Min = V2(FLT_MAX, FLT_MAX);
                BoundingRect.Max = V2(FLT_MIN, FLT_MIN);
                i32 TotalParticles = 0;
                for(i32 PIndex = 0;
                    PIndex < Entry->ParticleCount;
                    ++PIndex)
                {
                    Positions[PIndex] = MapGameSpaceToWindowSpace(Positions[PIndex], TargetBuffer);
                    BoundingRect = ExpandRectToContainPoint(BoundingRect, Positions[PIndex]);
                    ZDistSqs[PIndex] *= LightScale;
                }
                
                Light.ZDistSq = Entry->MinZDistSq;
                float LightMargin = CalculateLightMargin(&Light);
                BoundingRect = ExpandRectByRadius(BoundingRect, V2(LightMargin, LightMargin));

                temporary_memory ParticleGroupTemp = BeginTemporaryMemory(TransientArena);
                l_buffer LBuffer = AllocateLBufferForBoundingRect(BoundingRect, TransientArena);
                v2_4 *PositionsAsV2_4 = PushArrayAligned(TransientArena, Entry->ParticleCount, v2_4, 16);

                for(i32 PIndex = 0;
                    PIndex < Entry->ParticleCount;
                    ++PIndex)
                {
                    PositionsAsV2_4[PIndex] = V2_4FromV2(Positions[PIndex] - LBuffer.MinPoint);
                }

                float4 *OutBlock = LBuffer.Values;
                for(i32 BlockY = 0; BlockY < LBuffer.BHeight; ++BlockY) {
                    for(i32 BlockX = 0; BlockX < LBuffer.BWidth; ++BlockX) {
                        v2_4 Pixel = BlockCoordsToPixelCoords(BlockX, BlockY);
                        for(i32 PIndex = 0; PIndex < Entry->ParticleCount; ++PIndex)
                        {
                            v2_4 Source = PositionsAsV2_4[PIndex];
                            Light.ZDistSq = ZDistSqs[PIndex];
                            ApplyPointSourceToLBlockAdditive(OutBlock, Pixel, Source, Light);
                        }
                        ++OutBlock;
                    }
                }

                DrawLBufferToRGBBlockBuffer(LBuffer, &SwizzledBuffer, Light.H, Light.S);
                EndTemporaryMemory(ParticleGroupTemp);
#endif
            } break;
#if DEBUG_BUILD
            case RenderEntryType_render_entry_pixel_rect: {
                render_entry_pixel_rect* Entry = (render_entry_pixel_rect*) EntryType;
                NextEntry += sizeof(render_entry_pixel_rect);

                DrawRectangle(TargetBuffer, Entry->Color, Entry->Rect);
            } break;
            case RenderEntryType_render_entry_pixel_line: {
                render_entry_pixel_line* Entry = (render_entry_pixel_line*) EntryType;
                NextEntry += sizeof(render_entry_pixel_line);

                DrawLineClipped(TargetBuffer, Entry->Color, Entry->First, Entry->Second);
            } break;
            case RenderEntryType_render_entry_pixel_glyph: {
                render_entry_pixel_glyph* Entry = (render_entry_pixel_glyph*) EntryType;
                NextEntry += sizeof(render_entry_pixel_glyph);

                glyph ProjectedGlyph = ProjectGlyphIntoRect(Entry->Glyph, Entry->Rect);
                DrawGlyph(TargetBuffer, Entry->Color, &ProjectedGlyph);
            } break;
#endif
            case RenderEntryType_render_entry_clear: {
                render_entry_clear* Entry = (render_entry_clear*) EntryType;
                NextEntry += sizeof(render_entry_clear);

                Assert(TargetBuffer->MemorySize % sizeof(__m128) == 0);
                __m128i Color4 = _mm_set1_epi32(Entry->Color.Full);
                for (__m128i *Pixel4 = (__m128i*)TargetBuffer->Memory;
                     Pixel4 < (__m128i*)(TargetBuffer->Memory + TargetBuffer->MemorySize);
                     ++Pixel4)
                {
                    *Pixel4 = Color4;
                }
            } break;
            InvalidDefault;
        }
    }

        BENCH_START_COUNTING_CYCLES(SwizzleFrameBuffer)
        UnswizzleRGBBlockBufferToFrameBuffer(&SwizzledBuffer, TargetBuffer);
        BENCH_STOP_COUNTING_CYCLES(SwizzleFrameBuffer)

        EndTemporaryMemory(Temp);
}


#endif
