#define GlyphSpacingOverWidth 0.17f

internal glyph
ProjectGlyphIntoRect(glyph *Original, rect Rect) 
{
    Assert(Original->SegCount <= GLYPH_MAX_SEGS);
    glyph Result;
    Result.SegCount = Original->SegCount;
    for(int SegIndex = 0;
        SegIndex < Original->SegCount;
        SegIndex++) 
    {
        v2 FirstScreenPoint = ProjectNormalizedV2ToRect(Original->Segs[SegIndex].First, Rect);
        v2 SecondScreenPoint = ProjectNormalizedV2ToRect(Original->Segs[SegIndex].Second, Rect);
        Result.Segs[SegIndex] = line_seg{FirstScreenPoint, SecondScreenPoint};
    }
    return Result;
}

// Returns max corner
// Note: Printing functions do NOT take margins into account. Calling code is in charge of that and 
// can call CalculateLightMarginSq.  I think I need the flexibility of that.
internal v2
PrintFromMinCornerAndGlyphDim(render_buffer *Renderer, metagame_state *Metagame,
                                light_source *Light, const char* CString,
                                v2 MinPoint, v2 GlyphDim)
{
    // TODO: It seems like this should take a char count?
    Assert(Light);
    rect GlyphRect = RectFromMinCornerAndDimensions(MinPoint, GlyphDim);
    u32 CharCount = StringLength(CString);
    float GlyphSpacing = GlyphSpacingOverWidth * GlyphDim.X;
    for(u32 CharIndex = 0;
            CharIndex < CharCount;
            ++CharIndex, ++CString)
    {
        char CharToDraw = *CString;
        glyph* Glyph = 0;
        if ((CharToDraw >= 'a') && (CharToDraw <= 'z')) {
            Glyph = Metagame->Letters + (CharToDraw - 'a');
        } else if ((CharToDraw >= 'A') && (CharToDraw <= 'Z')) {
            Glyph = Metagame->Letters + (CharToDraw - 'A');
        } else if ((CharToDraw >= '0') && (CharToDraw <= '9')) {
            Glyph = Metagame->Digits + (CharToDraw - '0');
        } else if (CharToDraw != ' ') {
            Glyph = &Metagame->Decimal;
        }
        if(Glyph) {
            PushGlyph(Renderer, Glyph, *Light, GlyphRect);
        }
        GlyphRect += V2(GlyphDim.X + GlyphSpacing, 0);
    }
    return V2(GlyphRect.Right, GlyphRect.Bottom);
}

internal v2
CalculateGlyphDimFromWidth(metagame_state *Metagame, u32 CharCount, float Width)
{
    Assert(CharCount >= 1);
    Assert(Width > 0);
    v2 Result;
    Result.X = Width/(CharCount + ((CharCount - 1) * GlyphSpacingOverWidth));
    Result.Y = Metagame->GlyphYOverX * Result.X;
    return Result;
}

internal float
CalculateWidthFromCharCountAndGlyphDim(metagame_state *Metagame, u32 CharCount, v2 GlyphDim)
{
    Assert(CharCount >= 1);
    Assert(GlyphDim.X > 0);
    float Result = (CharCount * GlyphDim.X) + ((CharCount - 1)*(GlyphSpacingOverWidth * GlyphDim.X));
    return Result;
}

// NOTE: Returns max corner
internal v2
PrintFromXBoundsAndCenterY(render_buffer *Renderer, metagame_state *Metagame, 
        light_source *Light, const char* CString, float Left, float Right, float CenterY)
{
    u32 CharCount = StringLength(CString);
    v2 GlyphDim = CalculateGlyphDimFromWidth(Metagame, CharCount, Right - Left);
    float Top = CenterY - (GlyphDim.Y / 2.0f);
    v2 MaxCorner = PrintFromMinCornerAndGlyphDim(Renderer, Metagame, Light, CString, V2(Left, Top), GlyphDim);
    return MaxCorner;
}

// NOTE: Returns max corner
internal v2 
PrintFromXBoundsAndTop(render_buffer *Renderer, metagame_state *Metagame, 
        light_source *Light, char* CString, float Left, float Right, float Top)
{
    u32 CharCount = StringLength(CString);
    v2 GlyphDim = CalculateGlyphDimFromWidth(Metagame, CharCount, Right - Left);
    v2 MaxCorner = PrintFromMinCornerAndGlyphDim(Renderer, Metagame, Light, CString, V2(Left, Top), GlyphDim);
    return MaxCorner;
}

// NOTE: Return min corner!
internal v2
PrintFromXBoundsAndBottom(render_buffer *Renderer, metagame_state *Metagame, 
        light_source *Light, char* CString, float Left, float Right, float Bottom)
{
    u32 CharCount = StringLength(CString);
    v2 GlyphDim = CalculateGlyphDimFromWidth(Metagame, CharCount, Right - Left);
    float Top = Bottom - GlyphDim.Y;
    v2 MinCorner = V2(Left, Top);
    PrintFromMinCornerAndGlyphDim(Renderer, Metagame, Light, CString, MinCorner, GlyphDim);
    return MinCorner;
}

inline v2 
PrintFromWidthAndCenter(render_buffer *Renderer, metagame_state *Metagame,
        light_source *Light, const char* CString, float Width, v2 Center)
{
    v2 Result = PrintFromXBoundsAndCenterY(Renderer, Metagame, Light, CString, 
                                           Center.X - (Width*0.5f), Center.X + (Width*0.5f), Center.Y);
    return Result;
}

inline void
PrintFromGlyphSizeAndCenterBottom(render_buffer *Renderer, metagame_state *Metagame,
        light_source *Light, const char* CString, v2 GlyphDim, v2 CenterBottom)
{
    u32 CharCount = StringLength(CString);
    float Width = CalculateWidthFromCharCountAndGlyphDim(Metagame, CharCount, GlyphDim);
    v2 MinCorner = V2(CenterBottom.X - (0.5f * Width), CenterBottom.Y - GlyphDim.Y);
    PrintFromMinCornerAndGlyphDim(Renderer, Metagame, Light, CString, MinCorner, GlyphDim);
}

