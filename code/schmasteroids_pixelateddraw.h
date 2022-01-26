#ifndef SCHMASTEROIDS_PIXELATEDDRAW_H
#define SCHMASTEROIDS_PIXELATEDDRAW_H

// NOTE: None of this code is being used in HSL version of the game. Saving it in case I want to have a pixelated
// draw option.
// TODO: Make drawing primitives available in pixel space and game space for flexibility

// Based on Casey's initial way of doing this. See if the effect is okay.
inline void SetPixelWithAlpha(color *Dest, color Source)
{
    float DestFR = (float)Dest->R / 255.0f;
    float DestFG = (float)Dest->G / 255.0f;
    float DestFB = (float)Dest->B / 255.0f;

    float SourceFR = (float)Source.R / 255.0f;
    float SourceFG = (float)Source.G / 255.0f;
    float SourceFB = (float)Source.B / 255.0f;
    float SourceFA = (float)Source.A / 255.0f;
    
    Dest->R = (u8) (Lerp(DestFR, SourceFA, SourceFR) * 255.0f);
    Dest->G = (u8) (Lerp(DestFG, SourceFA, SourceFG) * 255.0f);
    Dest->B = (u8) (Lerp(DestFB, SourceFA, SourceFB) * 255.0f);

}

// TODO: This is probably broken now that the pitch is forced to be 16-byte aligned!!!
internal void
DrawLinePixels(platform_framebuffer *Backbuffer, color Color,
            v2 V1, v2 V2)
{
    Assert(InRect(V1, ScreenRect));
    Assert(InRect(V2, ScreenRect));

    v2 TopEndpoint, BottomEndpoint;
    if (V1.Y < V2.Y) {
        TopEndpoint = MapGameSpaceToWindowSpace(V1, Backbuffer);
        BottomEndpoint = MapGameSpaceToWindowSpace(V2, Backbuffer);
    } else {
        TopEndpoint = MapGameSpaceToWindowSpace(V2, Backbuffer);
        BottomEndpoint = MapGameSpaceToWindowSpace(V1, Backbuffer);
    }

    int TopRow = (int)(TopEndpoint.Y);
    Assert(TopRow >= 0);
    Assert(TopRow <= Backbuffer->Height);
    if(TopRow == Backbuffer->Height) {
        TopRow--;
    }

    int BottomRow = (int)(BottomEndpoint.Y);
    Assert(BottomRow >= 0);
    Assert(BottomRow <= Backbuffer->Height);
    if(BottomRow == Backbuffer->Height) {
        BottomRow--;
    }
    // NOTE: Short circuit for horizontal lines! Dividing by 0 is no good! This should prevent it.
    if (TopRow == BottomRow) {
        int Column1 = RoundToInt(TopEndpoint.X);
        int Column2 = RoundToInt(BottomEndpoint.X);
        int FirstCol, LastCol;

        if (Column1 < Column2) {
            FirstCol = Column1;
            LastCol = Column2;
        } else {
            FirstCol = Column2;
            LastCol = Column1;
        }

        i32 *Pixel = (i32*)Backbuffer->Memory + (FirstCol + (TopRow * Backbuffer->Width));
        for(int Col = FirstCol; Col <= LastCol; Col++) {
            SetPixelWithAlpha((color*)Pixel++, Color);
        }
        return;
    }
    //TODO:Short circuit for vertical lines also?

    int PixelX = (int)(TopEndpoint.X);
    Assert(PixelX >= 0);
    Assert(PixelX <= Backbuffer->Width);
    if(PixelX == Backbuffer->Width) {
        PixelX -= 1;
    }
    
    float RowStartX = TopEndpoint.X;
    float NegInvSlope = ((BottomEndpoint.X - TopEndpoint.X)/(BottomEndpoint.Y - TopEndpoint.Y));

    i32 *Pixel = (i32*)Backbuffer->Memory + PixelX + (Backbuffer->Width * TopRow);
    bool XMovingRight = (BottomEndpoint.X > TopEndpoint.X);
    int Row = TopRow;


    //Draw TopRow;
    float RowBottomY = (float)TopRow + 1;
    float RowBottomX = TopEndpoint.X + ((NegInvSlope) * (RowBottomY - TopEndpoint.Y));
    for(;
            (XMovingRight ? PixelX < (int)(RowBottomX) : PixelX > (int)(RowBottomX)); 
            (XMovingRight ? PixelX++ : PixelX--)) {
        SetPixelWithAlpha((color*)Pixel, Color);
        if (XMovingRight) {
            Pixel++;
        } else {
            Pixel--;
        }
    }
    SetPixelWithAlpha((color*)Pixel, Color);
    Row++;
    RowStartX = RowBottomX;
    Pixel += Backbuffer->Width;

    // Draw all the other rows
    for(; Row < BottomRow; Row++) {
        RowBottomX = RowStartX + (NegInvSlope);
        for(; 
                (XMovingRight ? PixelX < (int)(RowBottomX) : PixelX > (int)(RowBottomX)); 
                (XMovingRight ? PixelX++ : PixelX--)) {
            SetPixelWithAlpha((color*)Pixel, Color);
            if (XMovingRight) {
                Pixel++;
            } else {
                Pixel--;
            }
        }
        RowStartX = RowBottomX;
        SetPixelWithAlpha((color*)Pixel, Color);
        Pixel += Backbuffer->Width;
    }
    // Draw the Bottom row
    RowBottomX = BottomEndpoint.X;
    for(; 
            (XMovingRight ? PixelX < (int)(RowBottomX) : PixelX > (int)(RowBottomX)); 
            (XMovingRight ? PixelX++ : PixelX--)) {
        SetPixelWithAlpha((color*)Pixel, Color);
        if (XMovingRight) {
            Pixel++;
        } else {
            Pixel--;
        }
    }
    SetPixelWithAlpha((color*)Pixel, Color);
}

internal void
DrawLineClipped(platform_framebuffer*Backbuffer, color Color,
        v2 V1, v2 V2) 
{
    bool32 V1In = InRect(V1, ScreenRect);
    bool32 V2In = InRect(V2, ScreenRect);
    if(!V1In && !V2In) {
        return;
    }
    else if (!V1In) {
        V1 = ClipEndPointToRect(V2, V1, ScreenRect);
    } else if (!V2In) {
        V2 = ClipEndPointToRect(V1, V2, ScreenRect);
    }
    DrawLinePixels(Backbuffer, Color, V1, V2);
}

internal void DrawLineSegClipped(platform_framebuffer*Backbuffer, game_state *GameState, color Color,
        line_seg Seg)
{
    DrawLineClipped(Backbuffer, Color,
            Seg.First, Seg.Second);
}

internal void
DrawRectangle(platform_framebuffer *Backbuffer, color Color, rect Rect)
{
    DrawLineClipped(Backbuffer, Color, V2(Rect.Left, Rect.Top), V2(Rect.Right, Rect.Top));
    DrawLineClipped(Backbuffer, Color, V2(Rect.Left, Rect.Top), V2(Rect.Left, Rect.Bottom));
    DrawLineClipped(Backbuffer, Color, V2(Rect.Right, Rect.Top), V2(Rect.Right, Rect.Bottom));
    DrawLineClipped(Backbuffer, Color, V2(Rect.Left, Rect.Bottom), V2(Rect.Right, Rect.Bottom));
}

internal void
DrawGlyphInRect(platform_framebuffer *Backbuffer, 
        color Color, rect Rect, glyph *Glyph)
{
    Assert(Glyph->SegCount <= GLYPH_MAX_SEGS);
    for(int SegIndex = 0;
        SegIndex < Glyph->SegCount;
        SegIndex++) 
    {
        v2 FirstScreenPoint = ProjectNormalizedV2ToRect(Glyph->Segs[SegIndex].First, Rect);
        v2 SecondScreenPoint = ProjectNormalizedV2ToRect(Glyph->Segs[SegIndex].Second, Rect);
        DrawLineClipped(Backbuffer, Color, FirstScreenPoint, SecondScreenPoint);
    }
}

internal void
DrawGlyph(platform_framebuffer *Backbuffer, 
        color Color, glyph *Glyph)
{
    Assert(Glyph->SegCount <= GLYPH_MAX_SEGS);
    for(int SegIndex = 0;
        SegIndex < Glyph->SegCount;
        SegIndex++) 
    {
        DrawLineClipped(Backbuffer, Color, Glyph->Segs[SegIndex].First, Glyph->Segs[SegIndex].Second);
    }
}


internal void
DrawLineWarped(platform_framebuffer*Backbuffer, color Color,
        v2 V1, v2 V2)
{
    bool32 V1In = InRect(V1, ScreenRect);
    bool32 V2In = InRect(V2, ScreenRect);
    if(V1In && V2In) {
        DrawLinePixels(Backbuffer, Color, V1, V2);
    } else {
        if (!V1In) {
            v2 OldV1 = V1;
            WarpPositionToScreen(&V1);
            v2 Diff = V1 - OldV1;
            V2 += Diff;
            V1In = InRect(V1, ScreenRect);
            V2In = InRect(V2, ScreenRect);
            Assert(V1In);
        }
        bool32 V2Right = (V2.X > SCREEN_RIGHT);
        bool32 V2Left = (V2.X < SCREEN_LEFT);
        bool32 V2Down = (V2.Y > SCREEN_BOTTOM);
        bool32 V2Up = (V2.Y < SCREEN_TOP);
        v2 FirstSplit = {};
        v2 SecondSplit = {};
        float Slope = (V2.Y - V1.Y)/(V2.X - V1.X);
        if (V2Right) {
            FirstSplit.X = SCREEN_RIGHT;
            SecondSplit.X = SCREEN_LEFT;
            SecondSplit.Y = FirstSplit.Y = (Slope * (FirstSplit.X - V1.X)) + V1.Y;
            V2.X -= SCREEN_WIDTH;
        } else if (V2Left) {
            FirstSplit.X = SCREEN_LEFT;
            SecondSplit.X = SCREEN_RIGHT;
            SecondSplit.Y = FirstSplit.Y = (Slope * (FirstSplit.X - V1.X)) + V1.Y;
            V2.X += SCREEN_WIDTH;
        } else if (V2Down) {
            FirstSplit.Y = SCREEN_BOTTOM;
            SecondSplit.Y = SCREEN_TOP;
            SecondSplit.X = FirstSplit.X = ((FirstSplit.Y - V1.Y) / Slope) + V1.X;
            V2.Y -= SCREEN_HEIGHT;
        } else if (V2Up) {
            FirstSplit.Y = SCREEN_TOP;
            SecondSplit.Y = SCREEN_BOTTOM;
            SecondSplit.X = FirstSplit.X = ((FirstSplit.Y - V1.Y) / Slope) + V1.X;
            V2.Y += SCREEN_HEIGHT;
        } else {
            // The warp put both endpoints on screen
            DrawLinePixels(Backbuffer, Color, V1, V2);
            return;
        }
        DrawLineWarped(Backbuffer, Color, V1, FirstSplit);
        DrawLineWarped(Backbuffer, Color, SecondSplit, V2);
    }
}

internal void 
DrawObject(platform_framebuffer *Backbuffer, object O, color Color, game_state *GameState) 
{
    Assert(O.VerticesCount > 1)
    // NOTE: Handle point objects, like shot in this same function?
    v2 MappedVertices[MAX_VERTICES];
    for(int VIndex = 0; VIndex < O.VerticesCount; VIndex++) {
        MappedVertices[VIndex] = MapFromObjectPosition(O.Vertices[VIndex], O);
    }
    for(int VIndex = 0; VIndex < O.VerticesCount; VIndex++) {
        v2 Vertex1 = MappedVertices[VIndex];
        v2 Vertex2 = MappedVertices[(VIndex + 1) % O.VerticesCount];
        DrawLineWarped(Backbuffer, Color, Vertex1, Vertex2);
    }
}
//
// Returns Max corner of printing
internal v2
PrintPixelatedFromMinCornerAndGlyphSize(render_buffer *Renderer, metagame_state *Metagame,
                                color Color, char* CString,
                                v2 MinPoint, v2 GlyphSize)
{
    // TODO: It seems like this should take a char count?
    rect GlyphRect = RectFromMinCornerAndDimensions(MinPoint, GlyphSize);
    u32 CharCount = StringLength(CString);
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
            PushPixelGlyph(Renderer, Color, Glyph, GlyphRect);
        }
        GlyphRect += V2(GlyphSize.X, 0);
    }
    return V2(GlyphRect.Right, GlyphRect.Bottom);
}


internal v2 
PrintPixelatedFromXBoundsAndTop(render_buffer *Renderer, metagame_state *Metagame, 
        color Color, char* CString, float Left, float Right, float Top)
{
    u32 CharCount = StringLength(CString);
    v2 GlyphSize;
    GlyphSize.X = (Right - Left)/CharCount;
    GlyphSize.Y = Metagame->GlyphYOverX * GlyphSize.X;
    v2 MaxCorner = PrintPixelatedFromMinCornerAndGlyphSize(Renderer, Metagame, Color, CString, V2(Left, Top), GlyphSize);
    return MaxCorner;
}
#endif
