#ifndef SCHMASTEROIDS_HSLDRAW_H
#define SCHMASTEROIDS_HSLDRAW_H


// TODO: Take another pass at this when possible... make sure that the swizzling actually makes sense to do?
// I suspect that my test in October 2021 was not fair to the unswizzled version. Based on watching Casey's simd
// optimization, it seems quite possible that getting __m128 values out of structs could have
// performance impact too.

inline float CalculateLightMarginSq(light_source *Light, float LMin = 1.0f/255.0f)
{
    float MaxDistanceSqTotalFromLine = Light->C_L / LMin;
    float MaxDistanceSqXYFromLine = MaxDistanceSqTotalFromLine - Light->ZDistSq;
    float Result = Max(MaxDistanceSqXYFromLine, 0.0f);
    return Result;
}

inline float CalculateLightMargin(light_source *Light, float LMin = 1.0f/255.0f)
{
    float Result = Sqrt(CalculateLightMarginSq(Light, LMin));
    return Result;
}

inline void DrawPixelHSL(platform_framebuffer *Backbuffer, i32 PixelX, i32 PixelY, float H, float S, float L);

#define RGB_BLOCK_DIM 2
v2_4 BlockCoordsToPixelCoords(i32 BlockX, i32 BlockY)
{
    __m128 PixelX = _mm_add_ps(_mm_set_ps(0.0f, 1.0f, 0.0f, 1.0f),
                                _mm_set_ps1((float)BlockX*2.0f));
    __m128 PixelY = _mm_add_ps(_mm_set_ps(0.0f, 0.0f, 1.0f, 1.0f),
                                _mm_set_ps1((float)BlockY*2.0f));
    v2_4 Result = v2_4{PixelX, PixelY};
    return Result;
}

// NOTE: l_buffers are stored in blocks of 2x2 pixels, i.e. swizzled, and can be rendered to RGB to a swizzled
// frame buffer, or unswizzled at the same time by code that uses them.
typedef struct {
    i32 BWidth;
    i32 BHeight;
    i32 BMinX;
    i32 BMinY;
    v2 MinPoint;
    float4 *Values;
} l_buffer;

// NOTE: I think that I can say that the size of a block is constant. I'm not planning on having an AVX 
// version of this for example. So a block will always be 4 pixels.
typedef struct {
    i32 Width; // NOTE: Width and height are in blocks
    i32 Height;
    i32 Pitch; // NOTE: Pitch is in bytes
    i32 MemorySize;
    u8 *Memory;
} rgb_block_buffer;

inline v2 MapGameSpaceToWindowSpace(v2 In, rgb_block_buffer *SwizzledBuffer)
{
    v2 Result;
    Result.X = ((SwizzledBuffer->Width * 2 / SCREEN_WIDTH) * In.X) - 0.5f;
    Result.Y = ((SwizzledBuffer->Height  * 2 / SCREEN_HEIGHT) * In.Y) - 0.5f;
    return Result;
}



__m128 NegOne = _mm_set_ps1(-1.0f);
__m128 Zero = _mm_set_ps1(0.0f);
__m128 One = _mm_set_ps1(1.0f);
__m128 Three = _mm_set_ps1(3.0f);
__m128 Four = _mm_set_ps1(4.0f);
__m128 Eight = _mm_set_ps1(8.0f);
__m128 Nine = _mm_set_ps1(9.0f);
__m128 Twelve = _mm_set_ps1(12.0f);
__m128 UCharMax = _mm_set_ps1(255.0f);

inline __m128 EuclideanDivide(__m128 Val, __m128 Divisor)
{
    // NOTE: This only works with positive numbers!
    __m128i ResultInt = _mm_cvttps_epi32(_mm_div_ps(Val, Divisor));
    __m128 Result = _mm_cvtepi32_ps(ResultInt);
    return Result;
}

inline __m128 Mod(__m128 Val, __m128 Modulus)
{
    // NOTE: This only works with positive numbers!
    __m128 Result = _mm_sub_ps(Val, _mm_mul_ps(Modulus, EuclideanDivide(Val, Modulus)));
    return Result;
}

inline __m128 SingleColorF(__m128 N, __m128 HTimesTwelve, __m128 S, __m128 L)
{
    __m128 K = Mod(_mm_add_ps(N, HTimesTwelve), Twelve);
    __m128 A = _mm_mul_ps(S, _mm_min_ps(L, _mm_sub_ps(One, L)));
    __m128 MinPart = _mm_min_ps(One, _mm_min_ps(_mm_sub_ps(K, Three), _mm_sub_ps(Nine, K)));
    __m128 Result = _mm_sub_ps(L, _mm_mul_ps(A, _mm_max_ps(NegOne, MinPart)));
    return Result;
}

internal rgba_4
HSLToRGBA_4(__m128 H, __m128 S, __m128 L) {
    // NOTE: Storing L value in A for now
    // NOTE: This currently completely ignores alpha.

    __m128 HTimesTwelve = _mm_mul_ps(H, Twelve);
    __m128 R = SingleColorF(Zero, HTimesTwelve, S, L); // R
    __m128 G = SingleColorF(Eight, HTimesTwelve, S, L); // G
    __m128 B = SingleColorF(Four, HTimesTwelve, S, L); // B
    __m128i RI = _mm_cvttps_epi32(_mm_mul_ps(R, UCharMax));
    __m128i GI = _mm_cvttps_epi32(_mm_mul_ps(G, UCharMax));
    __m128i BI = _mm_cvttps_epi32(_mm_mul_ps(B, UCharMax));

    i32 RedShift = 16;
    i32 GreenShift = 8;
    i32 BlueShift = 0;
    RI = _mm_slli_epi32(RI, RedShift);
    GI = _mm_slli_epi32(GI, GreenShift);
    BI = _mm_slli_epi32(BI, BlueShift);

    rgba_4 Result;
    Result.IVec = _mm_or_si128(RI, _mm_or_si128(GI, BI));
    return Result;
}


internal void
UnswizzleRGBBlockBufferToFrameBuffer(rgb_block_buffer *SwizzledBuffer, platform_framebuffer *TargetBuffer)
{
    //NOTE: Currently this only works for entire buffers that are the same size!
    Assert(TargetBuffer->Height % 2 == 0);
    Assert(TargetBuffer->Width % 2 == 0);
    Assert(TargetBuffer->Width / 2 == SwizzledBuffer->Width);
    Assert(TargetBuffer->Height / 2 == SwizzledBuffer->Height);
    Assert(TargetBuffer->MemorySize == SwizzledBuffer->MemorySize);
    Assert(SwizzledBuffer->Width % 2 == 0);

    rgba_4 *Block = (rgba_4*)SwizzledBuffer->Memory; 
    for (i32 BlockY = 0;
         BlockY < SwizzledBuffer->Height;
         ++BlockY) 
    {

        rgba_4 *Line1 = (rgba_4*)(TargetBuffer->Memory + (BlockY*2*TargetBuffer->Pitch));
        rgba_4 *Line2 = (rgba_4*)(TargetBuffer->Memory + ((BlockY*2 + 1)*(TargetBuffer->Pitch)));
        for (i32 BlockX = 0;
             BlockX < SwizzledBuffer->Width;
             BlockX += 2)
        {
            rgba_4 ReadBlock0 = *Block;
            rgba_4 ReadBlock1 = *(Block + 1);

            constexpr u32 Line1Mask = _MM_SHUFFLE(2, 3, 2, 3);
            constexpr u32 Line2Mask = _MM_SHUFFLE(0, 1, 0, 1);

            rgba_4 Line1Out, Line2Out;
            Line1Out.Vec = _mm_shuffle_ps(ReadBlock0.Vec, ReadBlock1.Vec, Line1Mask);
            Line2Out.Vec = _mm_shuffle_ps(ReadBlock0.Vec, ReadBlock1.Vec, Line2Mask);
            *Line1 = AddRGBSaturating(*Line1, Line1Out);

            *Line2 = AddRGBSaturating(*Line2, Line2Out);

            ++Line1;
            ++Line2;
            Block += 2;
        }
        
    }
}

inline float 
CalculateLightScale(platform_framebuffer *Backbuffer)
{
    float Result = Square((float)Backbuffer->Width) / Square(SCREEN_WIDTH);
    return Result;
}

inline light_source 
ScaleLightSource(light_source *GameLight, float LightScale)
{
    light_source Result;
    Result.H = GameLight->H;
    Result.S = GameLight->S;
    Result.C_L = GameLight->C_L * LightScale;
    Result.ZDistSq = GameLight->ZDistSq * LightScale;
    return Result;
}

inline void 
DrawLBlockToRGBBlockBuffer(rgb_block_buffer *RGBBlockBuffer, __m128 L, __m128 H, __m128 S,
                           i32 BlockX, i32 BlockY)
{
    Assert(BlockX < RGBBlockBuffer->Width);
    rgba_4 *DestBlock = (rgba_4*)RGBBlockBuffer->Memory + RGBBlockBuffer->Width*BlockY + BlockX;

    rgba_4 Generated = HSLToRGBA_4(H, S, L);

    *DestBlock = AddRGBSaturating(*DestBlock, Generated);
}

inline void
DrawFromLBufferToRGBBlockBufferRaw(l_buffer LBuffer, rgb_block_buffer *RGBBlockBuffer, 
                            i32 DestX, i32 DestY, i32 SourceX, i32 SourceY, i32 Width, i32 Height, float H, float S)
{
    __m128 HVec = _mm_set_ps1(H);
    __m128 SVec = _mm_set_ps1(S);
    Assert(DestX + Width <= RGBBlockBuffer->Width);
    Assert(DestX >= 0);
    Assert(DestY + Height <= RGBBlockBuffer->Height);
    Assert(DestY >= 0);

    for (i32 BlockY = 0; BlockY < Height; ++BlockY) {
        __m128 *LValueBlock = (__m128*)LBuffer.Values + LBuffer.BWidth*(BlockY + SourceY) + SourceX;
                                                      
        for (i32 BlockX = 0; BlockX < Width; ++BlockX) {
            *LValueBlock = _mm_min_ps(*LValueBlock, _mm_set_ps1(1.0f));
            float *LValues = (float*)LValueBlock;
            DrawLBlockToRGBBlockBuffer(RGBBlockBuffer, *LValueBlock, HVec, SVec, BlockX + DestX, 
                                                BlockY + DestY);
            LValueBlock++;
        }
    }
}

inline void
DrawLBufferToRGBBlockBuffer(l_buffer LBuffer, rgb_block_buffer *RGBBlockBuffer, float H, float S)
{
    // NOTE: All coordinates here are in blocks
    Assert(LBuffer.BWidth <= RGBBlockBuffer->Width);
    Assert(LBuffer.BHeight <= RGBBlockBuffer->Height);

    i32 SplitX = 0; 
    i32 SplitY = 0;
    i32 OrigBMinX = LBuffer.BMinX;
    LBuffer.BMinX %= RGBBlockBuffer->Width;
    LBuffer.BMinY %= RGBBlockBuffer->Height;
    if (LBuffer.BMinX < 0) { 
        LBuffer.BMinX += RGBBlockBuffer->Width; 
    }
    if (LBuffer.BMinY < 0) { 
        LBuffer.BMinY += RGBBlockBuffer->Height; 
    }
    
    if (LBuffer.BMinX + LBuffer.BWidth > RGBBlockBuffer->Width) {
        SplitX = RGBBlockBuffer->Width - LBuffer.BMinX;
    }
    if (LBuffer.BMinY + LBuffer.BHeight > RGBBlockBuffer->Height) {
        SplitY = RGBBlockBuffer->Height - LBuffer.BMinY;
    }

    if (SplitX) {
        if (SplitY) {
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, LBuffer.BMinX, LBuffer.BMinY, 0, 0, SplitX, SplitY, H, S);
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, 0, LBuffer.BMinY, SplitX, 0, LBuffer.BWidth - SplitX, SplitY, H, S);
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, LBuffer.BMinX, 0, 0, SplitY, SplitX, LBuffer.BHeight - SplitY, H, S);
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, 0, 0, SplitX, SplitY, LBuffer.BWidth - SplitX, LBuffer.BHeight - SplitY, H, S);
        } else {
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, LBuffer.BMinX, LBuffer.BMinY, 0, 0, SplitX, LBuffer.BHeight, H, S);
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, 0, LBuffer.BMinY, SplitX, 0, LBuffer.BWidth - SplitX, LBuffer.BHeight, H, S);
        }
    } else {
        if (SplitY) {
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, LBuffer.BMinX, LBuffer.BMinY, 0, 0, LBuffer.BWidth, SplitY, H, S);
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, LBuffer.BMinX, 0, 0, SplitY, LBuffer.BWidth, LBuffer.BHeight - SplitY, H, S);
        } else {
            DrawFromLBufferToRGBBlockBufferRaw(LBuffer, RGBBlockBuffer, LBuffer.BMinX, LBuffer.BMinY, 0, 0, LBuffer.BWidth, LBuffer.BHeight, H, S);
        }
    }
}

inline void
ApplyPointSourceToLBlockMax(float4 *OutBlock, v2_4 PixelCoords, v2_4 PointSource, light_source Light)
{
    float4 DistSqXY = LengthSq(PixelCoords - PointSource);
    float4 DistSqTotal = DistSqXY + Float4(Light.ZDistSq);
    float4 CalculatedL = Float4(Light.C_L) / DistSqTotal;
    *OutBlock = Float4Max(*OutBlock, CalculatedL);
}

inline void
ApplyPointSourceToLBlockAdditive(float4 *OutBlock, v2_4 PixelCoords, v2_4 PointSource, light_source Light)
{
    float4 DistSqXY = LengthSq(PixelCoords - PointSource);
    float4 DistSqTotal = DistSqXY + Float4(Light.ZDistSq);
    float4 CalculatedL = Float4(Light.C_L) / DistSqTotal;
    *OutBlock = *OutBlock + CalculatedL;
}

internal l_buffer AllocateLBufferForBoundingRect(rect BoundingRect, memory_arena *Arena)
{
    i32 MinXI = RoundDownToMultipleOf(2, Floor(BoundingRect.Min.X));
    i32 MinYI = RoundDownToMultipleOf(2, Floor(BoundingRect.Min.Y));
    i32 OnePastMaxXI = Ceil(BoundingRect.Max.X);
    i32 OnePastMaxYI = Ceil(BoundingRect.Max.Y);
    i32 Width = RoundUpToMultipleOf(2, OnePastMaxXI - MinXI);
    i32 Height = RoundUpToMultipleOf(2, OnePastMaxYI - MinYI);

    l_buffer Result;
    Result.BWidth = Width / 2;
    Result.BHeight = Height / 2;
    size_t BlockCount = (size_t)Result.BWidth * (size_t)Result.BHeight;
    Result.Values = PushArrayAligned(Arena, 
                                     BlockCount, float4, 16);
    ZeroSize(Result.Values, BlockCount * sizeof(float4));
    AssertAligned(Result.Values, 16);

    Result.MinPoint = V2((float)MinXI, (float)MinYI);
    Result.BMinX = MinXI/2;
    Result.BMinY = MinYI/2;
    return Result;
}

internal void
DrawParticleSwizzled(v2 Position, rgb_block_buffer *RGBBlockBuffer, light_source Light, memory_arena *TransientArena)
{
    //BENCH_START_COUNTING_CYCLES(DrawParticle)
    float LightMargin = CalculateLightMargin(&Light, 1.0f/255.0f);
    if (LightMargin == 0.0f) {
        return;
    }
    Assert(TransientArena);
    temporary_memory Temp = BeginTemporaryMemory(TransientArena);

    rect BoundingRect;
    BoundingRect.Min = Position;
    BoundingRect.Max = Position;
    BoundingRect = ExpandRectByRadius(BoundingRect, V2(LightMargin, LightMargin));
    l_buffer LBuffer = AllocateLBufferForBoundingRect(BoundingRect, TransientArena);

    float4 *OutBlock = LBuffer.Values;

    v2_4 AdjustedPosition = V2_4FromV2(Position - LBuffer.MinPoint);

    for(i32 BlockY = 0; BlockY < LBuffer.BHeight; ++BlockY) {
        for(i32 BlockX = 0; BlockX < LBuffer.BWidth; ++BlockX) {
            v2_4 Pixel = BlockCoordsToPixelCoords(BlockX, BlockY);
            // NOTE: Postponing abstracting this since at least we can probably optimize by passing the light 
            // parameters as __m128s already?
            ApplyPointSourceToLBlockMax(OutBlock, Pixel, AdjustedPosition, Light);

            OutBlock++;
        }
    }
    DrawLBufferToRGBBlockBuffer(LBuffer, RGBBlockBuffer, Light.H, Light.S);
    EndTemporaryMemory(Temp);
    //BENCH_STOP_COUNTING_CYCLES(DrawParticle)
}

// TODO: Try saving these in scalar form and see if it's faster? 
typedef struct {
    v2_4 First;
    v2_4 FirstToSecond;
    v2_4 PerpFirstToSecond;
    float4 LineLengthSq;
    float4 LineLength;
} line_draw_data;

// NOTE: Endpoint coords are expected to be mapped into the L buffer to
// be drawn into
inline line_draw_data 
CalculateLineDrawData(v2 First, v2 Second)
{
    line_draw_data Result;
    Result.First = V2_4FromV2(First);
    Result.FirstToSecond = V2_4FromV2(Second - First);
    Result.PerpFirstToSecond = Perp(Result.FirstToSecond);
    Result.LineLengthSq = LengthSq(Result.FirstToSecond);
    Result.LineLength.Val = _mm_sqrt_ps(Result.LineLengthSq.Val);

    return Result;
}

inline void
ApplyLineSourceToLBlock(float4 *OutBlock, v2_4 PixelCoords, line_draw_data LineData, light_source Light)
{
    v2_4 FirstToPixel = PixelCoords - LineData.First;
    float4 DistanceToLinePerp = Dot(FirstToPixel, LineData.PerpFirstToSecond) / LineData.LineLength;
    float4 DistanceToLinePerpSq = Square(DistanceToLinePerp);

    float4 ScalarProjectionOntoLine0_1 = Dot(FirstToPixel, LineData.FirstToSecond) / LineData.LineLengthSq;
    float4 DistSqTotal = DistanceToLinePerpSq + Float4(Light.ZDistSq);
    float4 CalculatedL = Float4(Light.C_L) / DistSqTotal;

    __m128 NotOffFirstEnd = _mm_cmpgt_ps(ScalarProjectionOntoLine0_1.Val, _mm_set_ps1(0.0f));
    __m128 NotOffSecondEnd = _mm_cmplt_ps(ScalarProjectionOntoLine0_1.Val, _mm_set_ps1(1.0f));
    __m128 ShouldDraw = _mm_and_ps(NotOffSecondEnd, NotOffFirstEnd);
    CalculatedL.Val = _mm_and_ps(CalculatedL.Val, ShouldDraw);
    *OutBlock = Float4Max(*OutBlock, CalculatedL);
}

internal void
DrawObjectSwizzled(rgb_block_buffer *SwizzledBuffer, v2 *Vertices, u32 VerticesCount, light_source Light, memory_arena *TransientArena)
{
    float LightMargin = CalculateLightMargin(&Light, 1.0f/255.0f);
    if (LightMargin == 0.0f) {
        return;
    }
    Assert(TransientArena);
    temporary_memory Temp = BeginTemporaryMemory(TransientArena);

    Assert(VerticesCount > 1)
    rect BoundingRect;
    Vertices[0] = MapGameSpaceToWindowSpace(Vertices[0], SwizzledBuffer);
    BoundingRect.Min = Vertices[0];
    BoundingRect.Max = Vertices[0];

    for (u32 VIndex = 1; VIndex < VerticesCount; VIndex++) {
        Vertices[VIndex] = MapGameSpaceToWindowSpace(Vertices[VIndex], SwizzledBuffer);
        BoundingRect = ExpandRectToContainPoint(BoundingRect, Vertices[VIndex]);
    }
    BoundingRect = ExpandRectByRadius(BoundingRect, V2(LightMargin, LightMargin));
    l_buffer LBuffer = AllocateLBufferForBoundingRect(BoundingRect, TransientArena);
    for(u32 VIndex = 0; VIndex < VerticesCount; VIndex++) {
        Vertices[VIndex] -= LBuffer.MinPoint;
    }

    // NOTE: L values will be stored as a sequence of four-pixel squares starting in the 
    // upper left of the rect touched by the object and moving to the right then down

    line_draw_data *Lines = PushArrayAligned(TransientArena, VerticesCount, line_draw_data, 16);
    line_draw_data *RetainingLine = Lines;
    for (u32 VIndex = 0; VIndex < VerticesCount - 1; ++VIndex) {
        *RetainingLine++ = CalculateLineDrawData(Vertices[VIndex], Vertices[VIndex + 1]);
    }
    // Last line
    *RetainingLine = CalculateLineDrawData(Vertices[VerticesCount-1], Vertices[0]);

    float4 *OutBlock = LBuffer.Values;
    for(i32 BlockY = 0; BlockY < LBuffer.BHeight; ++BlockY) {
        for(i32 BlockX = 0; BlockX < LBuffer.BWidth; ++BlockX) {
            v2_4 Pixel = BlockCoordsToPixelCoords(BlockX, BlockY);

            for (u32 VIndex = 0; VIndex < VerticesCount; ++VIndex) {

                // TODO: Try retaining vertices as v2_4s?
                v2_4 Vertex = V2_4FromV2(Vertices[VIndex]);                
                ApplyPointSourceToLBlockMax(OutBlock, Pixel, Vertex, Light);

                line_draw_data *Line = Lines + VIndex;
                ApplyLineSourceToLBlock(OutBlock, Pixel, *Line, Light);

            }
            OutBlock++;
        }
    }


    DrawLBufferToRGBBlockBuffer(LBuffer, SwizzledBuffer, Light.H, Light.S);

    EndTemporaryMemory(Temp);

}

internal void
DrawGlyphSwizzled(rgb_block_buffer *SwizzledBuffer, glyph *Glyph, light_source Light, memory_arena *TransientArena)
{
    if (Glyph->SegCount < 1) {
        return;
    }
    float LightMargin = CalculateLightMargin(&Light, 1.0f/255.0f);
    if (LightMargin == 0.0f) {
        return;
    }
    Assert(TransientArena);
    temporary_memory Temp = BeginTemporaryMemory(TransientArena);

    rect BoundingRect;
    BoundingRect.Min = V2(FLT_MAX, FLT_MAX);
    BoundingRect.Max = V2(FLT_MIN, FLT_MIN);
    for(int SegIndex = 0; SegIndex < Glyph->SegCount; SegIndex++) {
        BoundingRect = ExpandRectToContainPoint(BoundingRect, Glyph->Segs[SegIndex].First);
        BoundingRect = ExpandRectToContainPoint(BoundingRect, Glyph->Segs[SegIndex].Second);
    }
    BoundingRect = ExpandRectByRadius(BoundingRect, V2(LightMargin, LightMargin));
    l_buffer LBuffer = AllocateLBufferForBoundingRect(BoundingRect, TransientArena);

    glyph AdjustedGlyph;
    AdjustedGlyph.SegCount = Glyph->SegCount;
    for(int SegIndex = 0; SegIndex < Glyph->SegCount; SegIndex++) {
        AdjustedGlyph.Segs[SegIndex].First = Glyph->Segs[SegIndex].First - LBuffer.MinPoint;
        AdjustedGlyph.Segs[SegIndex].Second = Glyph->Segs[SegIndex].Second - LBuffer.MinPoint;
    }

    line_draw_data *Lines = PushArrayAligned(TransientArena, AdjustedGlyph.SegCount, line_draw_data, 16);
    line_draw_data *RetainingLine = Lines;
    for(int SegIndex = 0; SegIndex < AdjustedGlyph.SegCount; SegIndex++) {
        line_seg Seg = AdjustedGlyph.Segs[SegIndex];
        *RetainingLine++ = CalculateLineDrawData(Seg.First, Seg.Second);
    }

    float4 *OutBlock = LBuffer.Values;
    for(i32 BlockY = 0; BlockY < LBuffer.BHeight; ++BlockY) {
        for(i32 BlockX = 0; BlockX < LBuffer.BWidth; ++BlockX) {
            v2_4 Pixel = BlockCoordsToPixelCoords(BlockX, BlockY);

            for (i32 SegIndex = 0; SegIndex < AdjustedGlyph.SegCount; ++SegIndex) {

                v2_4 First = V2_4FromV2(AdjustedGlyph.Segs[SegIndex].First);                
                v2_4 Second = V2_4FromV2(AdjustedGlyph.Segs[SegIndex].Second);                
                ApplyPointSourceToLBlockMax(OutBlock, Pixel, First, Light);
                ApplyPointSourceToLBlockMax(OutBlock, Pixel, Second, Light);

                line_draw_data *Line = Lines + SegIndex;
                ApplyLineSourceToLBlock(OutBlock, Pixel, *Line, Light);
            }
            OutBlock++;
        }
    }
    DrawLBufferToRGBBlockBuffer(LBuffer, SwizzledBuffer, Light.H, Light.S);
    EndTemporaryMemory(Temp);
}

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

                Entry->Light = ScaleLightSource(&Entry->Light, LightScale);
                DrawObjectSwizzled(&SwizzledBuffer, Entry->Vertices, Entry->VerticesCount, Entry->Light, TransientArena);
            } break;
            case RenderEntryType_render_entry_particle: {
                render_entry_particle* Entry = (render_entry_particle*) EntryType;
                NextEntry += sizeof(render_entry_particle);

                Entry->Position = MapGameSpaceToWindowSpace(Entry->Position, TargetBuffer);
                Entry->Light = ScaleLightSource(&Entry->Light, LightScale);
                DrawParticleSwizzled(Entry->Position, &SwizzledBuffer, Entry->Light, TransientArena);
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
