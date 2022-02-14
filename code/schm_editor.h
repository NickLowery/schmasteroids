#ifndef SCHM_EDITOR_H

#define SELECT_ERROR_MARGIN_SQ 100.0f
#define COLOR_ASTEROIDS_COUNT 10
typedef enum _edit_mode: u32 {
    EditMode_Off = 0,
    EditMode_Digits,
    EditMode_Letters,
    EditMode_AsteroidColors,
    EditMode_Terminate,
} edit_mode;

typedef struct working_point_ {
    v2 Coords;
    struct working_point_ *NextPoint;
} working_point;

typedef struct working_seg_ {
    working_point *First, *Second;
    struct working_seg_ *NextSeg;
} working_seg;


typedef struct {
    i32 SegCount;
    working_point *Points;
    working_point *ActivePoint;
    working_seg *Segs;
    working_seg *ActiveSeg;
    glyph *LoadedGlyphStorageLocation;

    working_seg *FirstFreeSeg;
    working_point *FirstFreePoint;

    rect DragRect;
    bool32 Dragging;
    v2 DragOffset;

} working_glyph;

typedef struct {
    rect Rect;
    glyph *Glyph;
} editor_glyph_slot;

// NOTE: These are designed to be transient and get pushed per frame, unless I find out that causes problems.
#define SLIDER_NAME_MAX_LENGTH 6
typedef struct {
    float *Val;
    float MinVal;
    float MaxVal;
    char Name[SLIDER_NAME_MAX_LENGTH + 1]; 
} slider;

typedef struct {
    memory_arena EditArena;

    editor_glyph_slot GlyphRects[26];
    i32 GlyphRectCount;

    rect ActiveRect;
    working_glyph WorkingGlyph;

    u32 SliderCount;
    slider *Sliders;

    asteroid Color_Asteroids[COLOR_ASTEROIDS_COUNT];
    game_state EditGameState;
} editor_state;

internal void ResetSliders(editor_state *EditorState)
{
    EditorState->SliderCount = 0;
    EditorState->Sliders = 0;
}
internal slider* PushSlider(editor_state *EditorState, memory_arena *Arena, 
        float* Val = 0, float MinVal = 0.0f, float MaxVal = 0.0f, const char *Name = 0)
{
    slider* Result = PushStruct(Arena, slider);
    EditorState->SliderCount++;
    Result->Val = Val;
    Result->MinVal = MinVal;
    Result->MaxVal = MaxVal;
    StringCopyCounted(Result->Name, Name, SLIDER_NAME_MAX_LENGTH);
    return Result;
}

internal void
SetUpEditor(game_memory *GameMemory, edit_mode EditMode);

internal void
RunEditor(game_memory *GameMemory, platform_framebuffer *Backbuffer, game_input *Input);

internal working_point *
ClosestPointWithinError(v2 SearchCoords, working_glyph *WorkingGlyph);

internal working_point* 
FindOrAddWorkingPoint(v2 Coords, working_glyph *WorkingGlyph, memory_arena *Arena);

internal working_point*
AddPoint(working_glyph *WorkingGlyph, v2 Coords, memory_arena *Arena);

internal void
StartDraggingPoint(working_glyph *WorkingGlyph, working_point *Point, v2 DragOffset) {
    WorkingGlyph->Dragging = true;
    WorkingGlyph->DragOffset = DragOffset;
    WorkingGlyph->ActivePoint = Point;
}

internal working_seg*
AddSeg(working_glyph *WorkingGlyph, working_point *First, working_point *Second, memory_arena *Arena);

internal void
LoadUnitGlyphForWork(editor_state *EditorState, glyph* Glyph);

internal void
SaveWorkingGlyphToUnitGlyph(editor_state *EditorState, glyph* Glyph);

internal void
DeletePointIfOrphaned(editor_state *EditorState, working_point *Point, working_glyph *Glyph);
    
internal void
DeleteActiveSeg(editor_state *EditorState, working_glyph *Glyph); 

inline v2 MapWindowSpaceToGameSpace(v2 WindowCoords, platform_framebuffer *Backbuffer)
{
    v2 Result;
    Result.X = (SCREEN_WIDTH / Backbuffer->Width ) * (WindowCoords.X + 0.5f);
    Result.Y = (SCREEN_HEIGHT / Backbuffer->Height ) * (WindowCoords.Y + 0.5f);
    return Result;
}

inline v2 MapGameSpaceToWindowSpace(v2 In, platform_framebuffer *Backbuffer)
{
    v2 Result;
    Result.X = ((Backbuffer->Width / SCREEN_WIDTH) * In.X) - 0.5f;
    Result.Y = ((Backbuffer->Height / SCREEN_HEIGHT) * In.Y) - 0.5f;
    return Result;
}

#define SCHM_EDITOR_H 1
#endif
