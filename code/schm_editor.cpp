#include "schm_pixelateddraw.h"
#include "schm_editor.h"

inline editor_state *
GetEditorState(metagame_state *Metagame)
{
    editor_state *Result = &Metagame->EditorState;
    return Result;
}
inline edit_mode
GetEditMode(metagame_state *Metagame)
{
    edit_mode Result = Metagame->EditMode;
    return Result;
}

internal void
PushWorkingGlyph(render_buffer *Renderer, 
        color NormalColor, color ActiveColor, working_glyph *Glyph)
{
    for(working_seg *Seg = Glyph->Segs;
        Seg;
        Seg = Seg->NextSeg) 
    {
        if((Glyph->ActiveSeg == Seg) || 
           (Glyph->ActivePoint && ((Glyph->ActivePoint == Seg->First) || Glyph->ActivePoint == Seg->Second))) 
        {
            PushPixelLine(Renderer, ActiveColor, Seg->First->Coords, Seg->Second->Coords);
        } else {
            PushPixelLine(Renderer, NormalColor, Seg->First->Coords, Seg->Second->Coords);
        }
    }
    if(Glyph->ActivePoint) {
        PushPixelRect(Renderer, ActiveColor, Glyph->DragRect);
    }
}

inline void
SaveGlyphs(metagame_state *Metagame, game_memory *GameMemory)
{
    u32 GlyphSize = sizeof(glyph);
    for(int Digit = 0;
        Digit < 10;
        Digit++)
    {
        char Filename[7] = "digit";
        Filename[5] = '0' + (char)Digit;
        Filename[6] = '\0';
        GameMemory->PlatformDebugWriteFile(Filename, (void*) &Metagame->Digits[Digit], GlyphSize);
    }
    for (int LetterIndex = 0;
        LetterIndex < 26;
        LetterIndex++)
    {
        char Filename[7] = "letter";
        Filename[5] = 'a' + (char)LetterIndex;
        Filename[6] = '\0';
        GameMemory->PlatformDebugWriteFile(Filename, (void*) &Metagame->Letters[LetterIndex], GlyphSize);
    }
}

inline void
CloseEditMode(metagame_state *Metagame, game_memory *GameMemory, edit_mode Mode) {
    if (Mode == EditMode_Digits || Mode == EditMode_Letters) {
        SaveGlyphs(Metagame, GameMemory);
    } else if (Mode == EditMode_AsteroidColors) {
        GameMemory->PlatformDebugWriteFile("lightparams", (void*)&Metagame->LightParams, 
                                           sizeof(Metagame->LightParams));
    }
}

inline void
PrevEditMode(game_memory *GameMemory)
{
    metagame_state *Metagame = (metagame_state*)GameMemory->PermanentStorage;
    CloseEditMode(Metagame, GameMemory, Metagame->EditMode);
    if (Metagame->EditMode == EditMode_Off) {
        Metagame->EditMode = EditMode_Terminate;
    }
    Metagame->EditMode = (edit_mode)((u32)Metagame->EditMode - 1);
    if(Metagame->EditMode != EditMode_Off) {
        SetUpEditor(GameMemory, Metagame->EditMode);
    }
}

inline void
NextEditMode(game_memory *GameMemory)
{
    metagame_state *Metagame = (metagame_state*)GameMemory->PermanentStorage;
    CloseEditMode(Metagame, GameMemory, Metagame->EditMode);
    Metagame->EditMode = (edit_mode)((u32)Metagame->EditMode + 1);
    if (Metagame->EditMode == EditMode_Terminate) {
        Metagame->EditMode = EditMode_Off;
    } else {
        SetUpEditor(GameMemory, Metagame->EditMode);
    }
}


inline v2
ClosestPointOnWorkingSeg(v2 TestPoint, working_seg *Seg) 
{
    v2 Result = ClosestPointOnLineSeg(TestPoint, Seg->First->Coords, Seg->Second->Coords);
    return Result;
}

internal void 
ResetAsteroids(editor_state *EditorState, metagame_state *Metagame)
{
    game_state *EditGame = &EditorState->EditGameState;
    InitGameState(EditGame, 0, Metagame);
    rect AsteroidsRect;
    AsteroidsRect.Top = SCREEN_TOP;
    AsteroidsRect.Bottom = SCREEN_BOTTOM;
    AsteroidsRect.Left = SCREEN_LEFT;
    AsteroidsRect.Right = ScreenCenter.X;
    v2 RectDim = Dim(AsteroidsRect);
    u32 Rows = MAX_ASTEROID_SIZE + 1;
    u32 Columns = 3;
    v2 PlacementRectDim = V2(RectDim.X/Columns, RectDim.Y/Rows);
    rect FirstPlacementRect = RectFromMinCornerAndDimensions(V2(AsteroidsRect.Left, AsteroidsRect.Top),
            PlacementRectDim);
    for(u32 Row = 0;
            Row < Rows;
            ++Row)
    {
        rect PlacementRect = FirstPlacementRect;
        for(u32 Col = 0;
                Col < Columns;
                ++Col)
        {
            asteroid *A = CreateAsteroid(Row, &EditorState->EditGameState, &Metagame->LightParams);
            A->O.Heading = RandHeading(&EditGame->RandState);
            A->O.Spin = RandFloatRange(-INIT_ASTEROID_MAX_SPIN, INIT_ASTEROID_MAX_SPIN, &EditGame->RandState);
            A->O.Position = Center(PlacementRect);
            A->O.Velocity = V2(0.0f,0.0f);
            PlacementRect += XOnly(PlacementRectDim);
        }
        FirstPlacementRect += YOnly(PlacementRectDim);
    }
}
internal void
SetUpGlyphEditPage(i32 GlyphCount, float GlyphSpacingOverGlyphWidth, i32 GlyphsPerRow, glyph *GlyphArray, 
                   metagame_state *Metagame)
{

    editor_state *EditorState = GetEditorState(Metagame);
    float GlyphWidthsToFit = (float)GlyphsPerRow + (GlyphSpacingOverGlyphWidth * (float)(GlyphsPerRow + 1));
    float GlyphWidth = SCREEN_WIDTH / GlyphWidthsToFit;
    float GlyphSpacing = GlyphWidth * GlyphSpacingOverGlyphWidth;
    i32 GlyphRowCount = GlyphCount / GlyphsPerRow;
    if ((GlyphCount % GlyphsPerRow)  != 0) {
        ++GlyphRowCount;
    }
    EditorState->GlyphRectCount = GlyphCount;

    v2 RowTopLeft = V2(GlyphSpacing, GlyphSpacing);
    v2 GlyphSize = V2(GlyphWidth, GlyphWidth * Metagame->GlyphYOverX);

    editor_glyph_slot *GlyphRect = EditorState->GlyphRects;
    glyph *GlyphSource = GlyphArray;
    glyph *LastGlyph = GlyphArray + GlyphCount - 1;
    bool32 DoneWithGlyphSlots = false;
    rect Rect = {};
    for(i32 GlyphRow = 0;
        GlyphRow < GlyphRowCount;
        ++GlyphRow)
    {
        Rect = RectFromMinCornerAndDimensions(RowTopLeft, GlyphSize);
        for(i32 GlyphIndexInRow = 0;
            GlyphIndexInRow < GlyphsPerRow;
            ++GlyphIndexInRow, ++GlyphRect, ++GlyphSource)
        {
            Assert(Rect.Left > 0);
            GlyphRect->Rect = Rect;
            GlyphRect->Glyph = GlyphSource;
            Rect.Right += (GlyphSpacing + GlyphSize.X);
            Rect.Left += (GlyphSpacing + GlyphSize.X);
            if (GlyphSource == LastGlyph) {
                DoneWithGlyphSlots = true;
                break;
            }
        }
        if(DoneWithGlyphSlots) {
            break;
        }
        RowTopLeft.Y += GlyphSize.Y + GlyphSpacing;
    }

    EditorState->ActiveRect.Top = Rect.Bottom + GlyphWidth;
    EditorState->ActiveRect.Bottom = SCREEN_HEIGHT - GlyphWidth;
    float ActiveHeight = EditorState->ActiveRect.Bottom - EditorState->ActiveRect.Top;
    float ActiveWidth = ActiveHeight / Metagame->GlyphYOverX; 
    float ScreenCenterX = ScreenCenter.X;
    EditorState->ActiveRect.Right = ScreenCenterX + (ActiveWidth/2);
    EditorState->ActiveRect.Left = ScreenCenterX - (ActiveWidth/2);
    LoadUnitGlyphForWork(EditorState, EditorState->GlyphRects->Glyph);
}


internal void
SetUpEditor(game_memory *GameMemory, edit_mode EditMode)
{
    Assert(sizeof(metagame_state) < GameMemory->PermanentStorageSize);
    metagame_state *Metagame = (metagame_state*)GameMemory->PermanentStorage;
    editor_state *EditorState = GetEditorState(Metagame);
    memory_arena *EditArena = &EditorState->EditArena;
    ResetArena(EditArena);
    switch (EditMode) {
        case EditMode_Digits: 
            {
                SetUpGlyphEditPage(10, 0.5f, 10, Metagame->Digits, Metagame);
            } break;
        case EditMode_Letters: 
            {
                SetUpGlyphEditPage(26, 0.25f, 13, Metagame->Letters, Metagame);
            } break;
        case EditMode_AsteroidColors:
            {
                ResetAsteroids(EditorState, Metagame);
            } break;
        InvalidDefault;
    }
}
internal void
PushMouseRect(render_buffer *Renderer, v2 MousePosition, game_input *Input, color ColorIn = NullColor())
{
    rect MouseRect = RectFromCenterAndDimensions(MousePosition, V2(20.0f, 20.0f));
    color MouseRectColor;
    if (ColorIn.Full != NullColor().Full) {
        MouseRectColor = ColorIn;
    } else if (Input->Keyboard.Shift.IsDown) {
        MouseRectColor = Color(0xFF008800);
    }
    else if(Input->Mouse.LButton.IsDown) {
        MouseRectColor = Color(0xFF880000);
    } else {
        MouseRectColor = Color(0xFF888888);
    }
    PushPixelRect(Renderer, MouseRectColor, MouseRect);
}

internal void
RunGlyphEditPage(metagame_state *Metagame, render_buffer *Renderer, game_input *Input, v2 MousePosition, v2 LastMousePosition)
{
    editor_state *EditorState = GetEditorState(Metagame);
    editor_glyph_slot *GlyphRect = EditorState->GlyphRects;
    for(int GlyphRectIndex = 0;
            GlyphRectIndex < EditorState->GlyphRectCount;
            ++GlyphRectIndex, ++GlyphRect)
    {
        PushPixelRect(Renderer, Color(0xFF8888FF), GlyphRect->Rect);
        PushPixelGlyph(Renderer, Color(0xFFAAAAAA), GlyphRect->Glyph, GlyphRect->Rect);
    }
    working_glyph *WorkingGlyph = &EditorState->WorkingGlyph;
    if (WorkingGlyph->Dragging) {
        Assert(WorkingGlyph->ActivePoint);
        if (!Input->Mouse.LButton.IsDown) {
            WorkingGlyph->Dragging = false;
        } else {
            v2 NewPoint = ClampPointToRect(MousePosition + WorkingGlyph->DragOffset, EditorState->ActiveRect);
            WorkingGlyph->ActivePoint->Coords = NewPoint;
            WorkingGlyph->DragRect = RectFromCenterAndDimensions(WorkingGlyph->ActivePoint->Coords, 
                    V2(10.0f, 10.0f));
        }

    } else if(WentDown(&Input->Mouse.LButton)) {
        if (InRect(MousePosition, EditorState->ActiveRect)) {
            if (Input->Keyboard.Shift.IsDown && WorkingGlyph->ActiveSeg) {
                v2 ClosestOnActiveSeg = ClosestPointOnWorkingSeg(MousePosition, 
                        WorkingGlyph->ActiveSeg);
                if (LengthSq(ClosestOnActiveSeg - MousePosition) < SELECT_ERROR_MARGIN_SQ) {
                    DeleteActiveSeg(EditorState, WorkingGlyph);
                }
            } else if (WorkingGlyph->ActivePoint) {
                if ( InRect(MousePosition, WorkingGlyph->DragRect)) {
                    v2 DragOffset = WorkingGlyph->ActivePoint->Coords - MousePosition;
                    StartDraggingPoint(WorkingGlyph, WorkingGlyph->ActivePoint, DragOffset);
                } else if (WorkingGlyph->SegCount < GLYPH_MAX_SEGS) {
                    working_point *OtherPoint = ClosestPointWithinError(MousePosition, WorkingGlyph);
                    if (OtherPoint && (OtherPoint != WorkingGlyph->ActivePoint)) {
                        AddSeg(WorkingGlyph, WorkingGlyph->ActivePoint, OtherPoint, 
                                &EditorState->EditArena);
                    } else {
                        working_point *NewPoint = AddPoint(WorkingGlyph, 
                                MousePosition, &EditorState->EditArena);
                        AddSeg(WorkingGlyph, WorkingGlyph->ActivePoint, NewPoint, 
                                &EditorState->EditArena);
                        StartDraggingPoint(WorkingGlyph, NewPoint, V2(0,0));
                    }
                    
                }
            } else if (WorkingGlyph->SegCount < GLYPH_MAX_SEGS) {
                working_point *FirstPoint = AddPoint(WorkingGlyph,
                        MousePosition, &EditorState->EditArena);
                working_point *SecondPoint = AddPoint(WorkingGlyph,
                        MousePosition, &EditorState->EditArena);
                AddSeg(WorkingGlyph, FirstPoint, SecondPoint,
                        &EditorState->EditArena);
                StartDraggingPoint(WorkingGlyph, SecondPoint, V2(0,0));
            }
        } else {
            GlyphRect = EditorState->GlyphRects;
            for(int GlyphRectIndex = 0;
                    GlyphRectIndex < EditorState->GlyphRectCount;
                    ++GlyphRectIndex, ++GlyphRect)
            {
                if (InRect(MousePosition, GlyphRect->Rect)) {
                    if(Input->Keyboard.Shift.IsDown) {
                        SaveWorkingGlyphToUnitGlyph(EditorState, GlyphRect->Glyph);
                    } else {
                        LoadUnitGlyphForWork(EditorState, GlyphRect->Glyph);
                    }
                }
            }
        }

    } else if(WentDown(&Input->Mouse.RButton) && InRect(MousePosition, EditorState->ActiveRect) &&
            WorkingGlyph->SegCount > 0) {
        WorkingGlyph->ActiveSeg = 0;
        float ClosestDistanceSq = SELECT_ERROR_MARGIN_SQ;
        WorkingGlyph->ActivePoint = ClosestPointWithinError(MousePosition, WorkingGlyph);
        if (WorkingGlyph->ActivePoint) {
            WorkingGlyph->DragRect = RectFromCenterAndDimensions(WorkingGlyph->ActivePoint->Coords, 
                    V2(10.0f, 10.0f));
        } else {
            for(working_seg *TestSeg = WorkingGlyph->Segs;
                    TestSeg;
                    TestSeg = TestSeg->NextSeg)
            {
                v2 ClosestPoint = ClosestPointOnLineSeg(MousePosition, TestSeg->First->Coords, 
                        TestSeg->Second->Coords);
                float DistanceSq = LengthSq(MousePosition - ClosestPoint);
                if (DistanceSq < ClosestDistanceSq) {
                    ClosestDistanceSq = DistanceSq;
                    WorkingGlyph->ActiveSeg = TestSeg;
                }
            }
        }
    }
    PushMouseRect(Renderer, MousePosition, Input);

    PushPixelRect(Renderer, Color(0xFF8888FF), EditorState->ActiveRect);
    PushWorkingGlyph(Renderer, Color(0xFFAAAAAA), Color(0xFFFF0000), &EditorState->WorkingGlyph);
}

internal void
RunEditor(game_memory *GameMemory, render_buffer *Renderer, game_input *Input, v2 MousePosition, v2 LastMousePosition) 
{
    Assert(sizeof(metagame_state) < GameMemory->PermanentStorageSize);
    metagame_state *Metagame = (metagame_state*)GameMemory->PermanentStorage;
    editor_state *EditorState = GetEditorState(Metagame);
    edit_mode EditMode = GetEditMode(Metagame);
    if (EditMode == EditMode_Digits || EditMode == EditMode_Letters) {
        RunGlyphEditPage(Metagame, Renderer, Input, MousePosition, LastMousePosition);
    } else if (EditMode == EditMode_AsteroidColors) {
        game_state *EditGame = &EditorState->EditGameState;
        MoveAsteroids(Input->SecondsElapsed, EditGame);
        PushAsteroids(Renderer, EditGame);
        ResetSliders(EditorState);
        
        EditorState->Sliders = PushSlider(EditorState, &Metagame->TransientArena,
                                          &Metagame->LightParams.AsteroidLightMin.H,
                                          0.0f, 1.0f, "hmin");
        PushSlider(EditorState, &Metagame->TransientArena,
                   &Metagame->LightParams.AsteroidHRange,
                   0.0f, 1.0f, "hrng");
        PushSlider(EditorState, &Metagame->TransientArena,
                   &Metagame->LightParams.AsteroidLightMin.S,
                   0.0f, 1.0f, "smin");
        float MaxSRange = 1.0f - Metagame->LightParams.AsteroidLightMin.S;
        PushSlider(EditorState, &Metagame->TransientArena,
                   &Metagame->LightParams.AsteroidSRange,
                   0.0f, MaxSRange, "srng");
        PushSlider(EditorState, &Metagame->TransientArena, 
                   &Metagame->LightParams.AsteroidLightMin.C_L,
                   0.0f, 100.0f, "lmin");
        PushSlider(EditorState, &Metagame->TransientArena, 
                   &Metagame->LightParams.AsteroidC_LRange,
                   0.0f, 100.0f, "lrng");
        PushSlider(EditorState, &Metagame->TransientArena, 
                   &Metagame->LightParams.AsteroidLightMin.ZDistSq,
                   0.5f, 100.0f, "dmin");
        PushSlider(EditorState, &Metagame->TransientArena, 
                   &Metagame->LightParams.AsteroidZDistSqRange,
                   0.0f, 100.0f, "drng");

        rect SlidersArea = RectFromMinCornerAndDimensions(V2(ScreenCenter.X, SCREEN_TOP), 
                                                          V2(SCREEN_WIDTH/2.0f, SCREEN_HEIGHT));
        v2 SliderRectDim = V2(Width(SlidersArea)/(float)EditorState->SliderCount, SCREEN_HEIGHT/2.0f);
        rect SliderRect = RectFromMinCornerAndDimensions(SlidersArea.Min, SliderRectDim);
        v2 SliderHandleDim = V2(SliderRectDim.X * 0.8f, SliderRectDim.Y / 16.0f);
        float SliderRangeHeight = SliderRectDim.Y - SliderHandleDim.Y;
        float SliderRangeOffset = (SliderRectDim.Y - SliderRangeHeight)*0.5f;

        constexpr u32 ValCharCount = 7;
        char ValBuffer[ValCharCount + 1]; // 4 decimal places

        for(u32 SliderIndex = 0;
            SliderIndex < EditorState->SliderCount;
            ++SliderIndex)
        {
            slider *Slider = EditorState->Sliders + SliderIndex;
            PushPixelRect(Renderer, Color(0xFFFFFFFF), SliderRect);
            float SliderCenterX = CenterX(SliderRect);
            float SliderRangeTop = SliderRect.Top + SliderRangeOffset;
            float SliderRangeBottom = SliderRangeTop + SliderRangeHeight;
            ClampToRange(Slider->MinVal, Slider->Val, Slider->MaxVal);
            float NormalizedVal = NormalizeToRange(Slider->MinVal, *Slider->Val, Slider->MaxVal);
            float SliderHandleY = Lerp(SliderRangeTop, NormalizedVal, SliderRangeBottom);
            rect SliderHandle = RectFromCenterAndDimensions(V2(SliderCenterX, SliderHandleY),
                                                            SliderHandleDim);

            PushPixelLine(Renderer, Color(0xFF888888), V2(SliderCenterX, SliderRangeTop),
                                                           V2(SliderCenterX, SliderRangeBottom));
            if(InRect(LastMousePosition, SliderHandle) && Input->Mouse.LButton.WasDown && 
               Input->Mouse.LButton.IsDown)
            {
                if(MousePosition.Y != LastMousePosition.Y) {
                // NOTE: This is a "drag event"
                    SliderHandleY += (MousePosition.Y - LastMousePosition.Y);
                    ClampToRange(SliderRangeTop, &SliderHandleY, SliderRangeBottom);
                    SliderHandle = RectFromCenterAndDimensions(V2(SliderCenterX, SliderHandleY), SliderHandleDim);
                    NormalizedVal = NormalizeToRange(SliderRangeTop, SliderHandleY, SliderRangeBottom);
                    *Slider->Val = Lerp(Slider->MinVal, NormalizedVal, Slider->MaxVal);
                    ResetAsteroids(EditorState, Metagame);
                }
                PushPixelRect(Renderer, Color(0xFF0000FF), SliderHandle);
            } else {
                PushPixelRect(Renderer, Color(0xFFFF0000), SliderHandle);
            }

            float NameTop = SliderRect.Bottom + 2.0f;
            float NameLeft = SliderRect.Left + 2.0f;
            float NameRight = SliderRect.Right - 2.0f;
            v2 NameMaxCorner = PrintPixelatedFromXBoundsAndTop(Renderer, Metagame, 
                                                      Color(0xFFFFFFFF), Slider->Name,
                                                      NameLeft, NameRight, NameTop);
            

            float ValTop = NameMaxCorner.Y;
            // TODO: Put printf calls in a debug #define
            sprintf_s(ValBuffer, sizeof(ValBuffer), "%3.3f", *(Slider->Val));
            PrintPixelatedFromXBoundsAndTop(Renderer, Metagame, Color(0xFFFFFFFF), ValBuffer,
                                   NameLeft, NameRight, ValTop);
            SliderRect += XOnly(SliderRectDim);
        }
        PushMouseRect(Renderer, MousePosition, Input);
    }

}


internal working_point *
ClosestPointWithinError(v2 SearchCoords, working_glyph *WorkingGlyph) 
{
    working_point * Result = 0;
    float ClosestDistanceSq = SELECT_ERROR_MARGIN_SQ;
    for(working_point *TestPoint = WorkingGlyph->Points;
            TestPoint;
            TestPoint = TestPoint->NextPoint)
    {
        float DistanceSq = LengthSq(SearchCoords - TestPoint->Coords);
        if (DistanceSq < ClosestDistanceSq) {
            Result = TestPoint;
            ClosestDistanceSq = DistanceSq;
        }
    }

    return Result;
}

internal working_point*
AddPoint(working_glyph *WorkingGlyph, v2 Coords, memory_arena *Arena)
{
    working_point *Result = WorkingGlyph->FirstFreePoint;
    if (Result) {
        WorkingGlyph->FirstFreePoint = Result->NextPoint;
    } else {
        Result = PushStruct(Arena, working_point);
    }
    Result->NextPoint = WorkingGlyph->Points;
    WorkingGlyph->Points = Result;
    Result->Coords = Coords;

    return Result;
}

internal working_seg*
AddSeg(working_glyph *WorkingGlyph, working_point *First, working_point *Second, memory_arena *Arena)
{
    Assert(WorkingGlyph->SegCount < GLYPH_MAX_SEGS);
    WorkingGlyph->SegCount++;
    working_seg *Result = WorkingGlyph->FirstFreeSeg;
    if (Result) {
        WorkingGlyph->FirstFreeSeg = Result->NextSeg;
    } else {
        Result = PushStruct(Arena, working_seg);
    }
    Result->NextSeg = WorkingGlyph->Segs;
    WorkingGlyph->Segs = Result;
    Result->First = First;
    Result->Second = Second;

    return Result;
}

internal working_point* 
FindOrAddWorkingPoint(v2 Coords, working_glyph *WorkingGlyph, memory_arena *Arena) 
{
    working_point* Result = 0;
    for (working_point* TestPoint = WorkingGlyph->Points;
            TestPoint;
            TestPoint = TestPoint->NextPoint)
    {
        if (TestPoint->Coords == Coords) {
            Result = TestPoint;
            break;
        }
    }
    if(!Result) {
        Result = PushStruct(Arena, working_point);
        Result->Coords = Coords;
        Result->NextPoint = WorkingGlyph->Points;
        WorkingGlyph->Points = Result;
    }

    return Result;
}

internal void
LoadUnitGlyphForWork(editor_state *EditorState, glyph* Glyph)
{
    memory_arena *Arena = &EditorState->EditArena;
    working_glyph *WorkingGlyph = &EditorState->WorkingGlyph;
    ResetArena(Arena);
    *WorkingGlyph = {};
    for(i32 SegIndex = 0;
        SegIndex < Glyph->SegCount;
        ++SegIndex)
    {
        working_seg* NewSeg = PushStruct(Arena, working_seg);
        v2 FirstCoords = ProjectNormalizedV2ToRect(Glyph->Segs[SegIndex].First, EditorState->ActiveRect);
        NewSeg->First = FindOrAddWorkingPoint(FirstCoords, WorkingGlyph, Arena);
        v2 SecondCoords =  ProjectNormalizedV2ToRect(Glyph->Segs[SegIndex].Second, EditorState->ActiveRect);
        NewSeg->Second = FindOrAddWorkingPoint(SecondCoords, WorkingGlyph, Arena);
        NewSeg->NextSeg = WorkingGlyph->Segs;
        WorkingGlyph->Segs = NewSeg;
    }
    WorkingGlyph->SegCount = Glyph->SegCount;
    WorkingGlyph->LoadedGlyphStorageLocation = Glyph;
}

internal void
SaveWorkingGlyphToUnitGlyph(editor_state *EditorState, glyph* Glyph)
{
    working_glyph *WorkingGlyph = &EditorState->WorkingGlyph;
    Assert(WorkingGlyph->SegCount <= GLYPH_MAX_SEGS);
    Glyph->SegCount = WorkingGlyph->SegCount;
    working_seg *WorkingSeg = WorkingGlyph->Segs;
    for (i32 SegIndex = 0;
         SegIndex < Glyph->SegCount;
         SegIndex++, WorkingSeg = WorkingSeg->NextSeg)
    {
        v2 FirstCoords = NormalizeV2FromRect(WorkingSeg->First->Coords, EditorState->ActiveRect);
        v2 SecondCoords = NormalizeV2FromRect(WorkingSeg->Second->Coords, EditorState->ActiveRect);
        Glyph->Segs[SegIndex] = line_seg{FirstCoords, SecondCoords};
    }
}


internal void
DeletePointIfOrphaned(editor_state *EditorState, working_point *Point, working_glyph *Glyph)
{
    bool32 Orphaned = true;
    for(working_seg *TestSeg = Glyph->Segs;
        TestSeg;
        TestSeg = TestSeg->NextSeg)
    {
        if (TestSeg->First == Point || TestSeg->Second == Point) {
            Orphaned = false;
            break;
        }
    }
    if (Orphaned) {
        for(working_point **TestPointLocation = &Glyph->Points;
            *TestPointLocation;
            TestPointLocation = &((*TestPointLocation)->NextPoint))
        {
            if(*TestPointLocation == Point) {
                *TestPointLocation = Point->NextPoint;
                Point->NextPoint = Glyph->FirstFreePoint;
                Glyph->FirstFreePoint = Point;
                break;
            }
        }
    }
}

internal void
DeleteActiveSeg(editor_state *EditorState, working_glyph *Glyph)
{
    Assert(Glyph->SegCount > 0);
    Assert(Glyph->ActiveSeg);
    for(working_seg **TestSegLocation = &Glyph->Segs;
        *TestSegLocation;
        TestSegLocation = &((*TestSegLocation)->NextSeg))
    {
        if(*TestSegLocation == Glyph->ActiveSeg) {
            *TestSegLocation = Glyph->ActiveSeg->NextSeg;
            Glyph->ActiveSeg->NextSeg = Glyph->FirstFreeSeg;
            Glyph->FirstFreeSeg = Glyph->ActiveSeg;
            DeletePointIfOrphaned(EditorState, Glyph->ActiveSeg->First, Glyph);
            DeletePointIfOrphaned(EditorState, Glyph->ActiveSeg->Second, Glyph);
            
            Glyph->ActiveSeg = 0;
            break;
        }
    }
    --Glyph->SegCount;
}


