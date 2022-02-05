#ifndef NICKS_GAME_H

#define internal static

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


#include <math.h>
#include <stdio.h>
#include <float.h>
#include <stdlib.h>
#define PI 3.141592f

#if ASSERTIONS
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else 
#define Assert(Expression) 
#endif
#define InvalidCodePath Assert(!"Code should be unreachable")
#define InvalidDefault default: Assert(!"Default should not be reached")

struct _game_memory;
struct _memory_arena;


// Platform debug and intrinsic stuff
// TODO: Move this out somewhere to keep things cleaner?
#include <immintrin.h>
#if SCHM_SDL
#include <emmintrin.h>
#include <string.h>
// TODO: Do this in a nicer way and only define for DEBUG_BUILD
#define sprintf_s snprintf
#else
#include <windows.h>
#if DEBUG_BUILD
#include <intrin.h>
// TODO: Is this being used somehow?
#pragma intrinsic(__rdtsc)
LARGE_INTEGER _DEBUGPerformanceFrequency;
bool _Junk_ = QueryPerformanceFrequency(&_DEBUGPerformanceFrequency);
static float DEBUGPerfFrequency = (float)_DEBUGPerformanceFrequency.QuadPart;
#endif
#endif

#include "schmasteroids_math.h"
#include "schmasteroids_math.cpp"
#include "schmasteroids_strings.h"
#include "schm_sound.h"

#define Kilobytes(n) (n*1024LL)
#define Megabytes(n) (Kilobytes(n)*1024LL)
#define Gigabytes(n) (Megabytes(n)*1024LL)
#define Terabytes(n) (Gigabytes(n)*1024LL)
// TODO: Eventually calculate this based on what we actually need.
#define PERMANENT_STORE_SIZE Megabytes(64)
#define TRANSIENT_STORE_SIZE Megabytes(64)
#define GAME_MEMORY_SIZE (PERMANENT_STORE_SIZE + TRANSIENT_STORE_SIZE)

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))
#define MAX_VERTICES 8

#include "schmasteroids_color.h"
#include "schmasteroids_start_screen.h"
// Game constants
#define MAX_ABS_SHIP_VEL 180.0f
#define SHIP_ABS_ACCEL 240.0f
#define SHIP_TURN_SPEED 3.3f
#define SHIP_PARTICLES 100
#define SHIP_COLLISION_RADIUS 15.0f

#define MAX_ASTEROIDS 100 
//TODO: Calculate MAX_ASTEROIDS from how we explode the asteroids? There needs to be one extra since an extra
//will exist while exploding
#define MAX_ASTEROID_SIZE 2 //There are 3 sizes of asteroids, 2 is the largest
//#define INIT_ASTEROID_CT 1
//#define INIT_ASTEROID_MAX_ABS_VEL 70.0f
//#define INIT_ASTEROID_MIN_ABS_VEL 20.0f
#define INIT_ASTEROID_MAX_SPIN 1.0F
#define INIT_ASTEROID_MIN_DISTANCE 200.0f
#define EXPLODE_VELOCITY 60.0f
#define EXPLODE_SPIN 1.0f

#define SAUCER_SPIN 2.0f
#define SAUCER_RADIUS 18.0f
//#define SAUCER_SPAWN_TIME 20.0f
//#define SAUCER_ABS_VEL 60.0f
//#define SAUCER_COURSE_CHANGE_TIME 2.0f
//#define SAUCER_SHOOT_TIME 1.5f

#define MAX_SHOTS 8
#define SHOT_ABS_VEL 250.0f
#define SHOT_COOLDOWN_TIME 0.15f
#define SHOT_LIFETIME 1.0f

// NOTE: This is only for ungrouped particles, which currently means only the ship's exhaust, so 500 seems 
// to be plenty
#define MAX_PARTICLES 500
#define PARTICLE_LIFETIME 1.5f
#define PARTICLE_VELOCITY 60.0f

#define ASTEROID_COLOR 0xFF998800
#define SHIP_COLOR 0xFFFFFFFF
#define SAUCER_COLOR 0xFFDB17E6
#define SHOT_COLOR 0xFF00FF00

#define ISCREEN_WIDTH 800
#define ISCREEN_HEIGHT 600
#define SCREEN_WIDTH 800.0f
#define SCREEN_HEIGHT 600.0f
#define SCREEN_TOP 0.0f
#define SCREEN_BOTTOM SCREEN_HEIGHT
#define SCREEN_LEFT 0.0f
#define SCREEN_RIGHT SCREEN_WIDTH
static constexpr v2 ScreenCenter = {SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f};
static constexpr rect ScreenRect = {SCREEN_LEFT, SCREEN_TOP, SCREEN_RIGHT, SCREEN_BOTTOM};

#define GLYPH_MAX_SEGS 8

#define GAME_OVER_TIME 1.5f


// The properties of a size of asteroid
typedef struct {
    float Radius;
    float Irregularity;
    i32 Children;
    i32 Particles;
    i32 DestroyPoints;
} asteroid_size;

constexpr asteroid_size 
AsteroidProps[3] = {
    { 10.0f, 4.0f, 0, 30, 100},
    { 20.0f, 8.0f, 3, 40, 50},
    { 40.0f, 16.0f, 2, 60, 20}};

// NOTE: Pitch must be a multiple of 16. Currently to make SIMD drawing easier the width
// and height must be multiples of 4.
typedef struct {
    i32 Width;
    i32 Height;
    i32 Pitch;
    i32 BytesPerPixel;
    i32 MemorySize;
    u8 *Memory;
} platform_framebuffer;
// TODO: Should be called game_framebuffer for consistency?

typedef struct {
    bool32 IsDown;
    bool32 WasDown;
    // NOTE: I don't think we need super fast taps for any reason, but I think 
    // Windows gives us a transition count on keyboard messages if we do.
} game_button_input;

inline bool32 
WentDown(game_button_input *Button) {
    bool32 Result = (Button->IsDown && !Button->WasDown);
    return Result;
}

inline bool32 
WentUp(game_button_input *Button) {
    bool32 Result = (!Button->IsDown && Button->WasDown);
    return Result;
}
typedef struct {
    union {
        game_button_input Keys[7];
        struct {
            game_button_input MoveUp;
            game_button_input MoveDown;
            game_button_input MoveLeft;
            game_button_input MoveRight;
            game_button_input Fire;

            game_button_input Shift;
            // This is for making sure that I don't screw up the array
            // count
            game_button_input Terminator;
        };
    };
} game_keyboard_input;

typedef struct {
    i32 LastWindowX;
    i32 LastWindowY;
    i32 WindowX;
    i32 WindowY;
    union {
        game_button_input Buttons[3];
        struct{
            game_button_input LButton;
            game_button_input RButton;
            game_button_input MButton;
        };
    };
} game_mouse_input;

#if DEBUG_BUILD
typedef union {
    game_button_input Keys[8];
    struct {
        game_button_input F6;
        game_button_input F7;
        game_button_input F8;
        game_button_input F9;
        game_button_input F10;
        game_button_input F11;
        game_button_input F12;
        game_button_input Terminator;
    };
} debug_command_keys;
#endif

typedef struct {
    // NOTE: Add direct input later. Do tuning of the keyboard vs controller in the game?
    game_keyboard_input Keyboard;
#if DEBUG_BUILD
    debug_command_keys DebugKeys;
#endif
    game_mouse_input Mouse;
    float SecondsElapsed;
} game_input;


// Service the platform layer provides to the game
// NOTE: This should tell us the memory size for whatever sample rate we are actually using (if we support
// multiple sample rates)!

#define PLATFORM_GET_WAV_LOAD_INFO(name) platform_wav_load_info name(const char *Filename)
typedef PLATFORM_GET_WAV_LOAD_INFO(platform_get_wav_load_info);

#define PLATFORM_LOAD_WAV(name) void name(const char *Filename, void* Memory, platform_wav_load_info Info)
typedef PLATFORM_LOAD_WAV(platform_load_wav);

// TODO: This seems to be unused
#define PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_FREE_FILE_MEMORY(platform_free_file_memory);

#define PLATFORM_DEBUG_WRITE_FILE(name) void name(const char *Filename, void* Memory, u32 WriteSize)
typedef PLATFORM_DEBUG_WRITE_FILE(platform_debug_write_file);

#define PLATFORM_DEBUG_READ_FILE(name) u32 name(const char *Filename, void* Memory, u32 ReadSize)
typedef PLATFORM_DEBUG_READ_FILE(platform_debug_read_file);

#define PLATFORM_DEBUG_SAVE_FRAMEBUFFER_AS_BMP(name) void name(const char *Filename)
typedef PLATFORM_DEBUG_SAVE_FRAMEBUFFER_AS_BMP(platform_debug_save_framebuffer_as_bmp);

#define PLATFORM_QUIT(name) void name(void)
typedef PLATFORM_QUIT(platform_quit);

#define PLATFORM_TOGGLE_FULLSCREEN(name) void name(void)
typedef PLATFORM_TOGGLE_FULLSCREEN(platform_toggle_fullscreen);

typedef struct _game_memory {
    bool32 IsSetToZero;
    size_t PermanentStorageSize;
    size_t TransientStorageSize;
    void* PermanentStorage;
    void* TransientStorage;
    // TODO: Move these into their own struct for cleanliness?
    platform_get_wav_load_info* PlatformGetWavLoadInfo;
    platform_load_wav* PlatformLoadWav;
    platform_free_file_memory* PlatformFreeFileMemory;
    platform_debug_write_file* PlatformDebugWriteFile;
    platform_debug_read_file* PlatformDebugReadFile;
    platform_debug_save_framebuffer_as_bmp * PlatformDebugSaveFramebufferAsBMP;
    platform_quit* PlatformQuit;
    platform_toggle_fullscreen* PlatformToggleFullscreen;
} game_memory;

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

// TODO: Avoid memset call?
#define ZeroArray(Array, type) ZeroSize_(Array, ArrayCount(Array)*sizeof(type))
#define ZeroSize(StartAddress, Size) ZeroSize_(StartAddress, Size)
inline void ZeroSize_(void* StartAddress, size_t Size)
{
    memset(StartAddress, 0, Size);
}

// Services the game provides to the platform layer
#define GAME_INITIALIZE(name) void name(platform_framebuffer *Backbuffer, game_memory *GameMemory)
typedef GAME_INITIALIZE(game_initialize);
GAME_INITIALIZE(GameInitializeStub)
{
}

#define GAME_UPDATE_AND_RENDER(name) void name(platform_framebuffer *Backbuffer, \
        game_input *Input, game_memory *GameMemory) 
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}

#define GAME_GET_SOUND_OUTPUT(name) void name(platform_sound_output_buffer *SoundBuffer, game_memory *GameMemory)
typedef GAME_GET_SOUND_OUTPUT(game_get_sound_output);
GAME_GET_SOUND_OUTPUT(GameGetSoundOutputStub)
{
}

#define GAME_SEED_PRNG(name) void name(u32 Seed)
typedef GAME_SEED_PRNG(game_seed_prng);
GAME_SEED_PRNG(GameSeedPRNGStub)
{
}


typedef struct {
    float H;
    float S;
    float C_L;
    float ZDistSq;
} light_source;


typedef struct {
    light_source ShipLightMin;
    // TODO: Use this
    float ShipC_LRange;
    light_source SaucerLightBase;
    float SaucerC_LRange;

    light_source SaucerShotLightBase;
    float SaucerShotC_LRange;
    float SaucerShotModRate;

    light_source ShipShotLightBase;
    float ShipShotC_LRange;
    float ShipShotModRate;

    light_source AsteroidLightMin;
    float AsteroidHRange;
    float AsteroidSRange;
    float AsteroidC_LRange;
    float AsteroidZDistSqRange;

    float SaucerHCharged;
    float SaucerHChangeThreshold;
} light_params;
typedef struct {
    bool32 Exists;
    v2 Position;
    v2 Velocity;
    float Heading;
    float Spin;
    float CollisionRadius;
    int VerticesCount;
    v2 Vertices[MAX_VERTICES];
    light_source Light;
} object;

typedef struct {
    object O;
    u8 Size;
} asteroid;

typedef struct {
    bool32 Exists;
    v2 Position;
    v2 Velocity;
    light_source Light;
    float tLMod;
    float C_LOriginal;
    float SecondsToLive;
} particle;

#define MAX_PARTICLES_IN_BLOCK 8
typedef struct particle_block_ {

    v2 Position[MAX_PARTICLES_IN_BLOCK];
    v2 Velocity[MAX_PARTICLES_IN_BLOCK];
    float ZDistSq[MAX_PARTICLES_IN_BLOCK];
    i32 Count;
    // TODO: ZDistSq is the same for everybody for now but try varying it.
    particle_block_ *NextBlock;
} particle_block;

typedef struct particle_group_{
    float H;
    float S;
    float SecondsToLive;
    float C_LOriginal;
    particle_block *FirstBlock;
    particle_group_ *NextGroup;
    i32 TotalParticles;
} particle_group;

// NOTE: These glyphs are intended to be scaled to a rectangle, 
// so the coordinates should all be in a [0,1] interval.
typedef struct {
    i32 SegCount;
    line_seg Segs[GLYPH_MAX_SEGS];
} glyph;

enum menu_options {
    MenuOption_Play,
    MenuOption_Instructions,
    MenuOption_ToggleFullScreen,
    MenuOption_Quit,
    MenuOption_Terminator,
};

enum game_mode {
    GameMode_None = 0,
    GameMode_StartScreen,
    GameMode_LevelStartScreen,
    GameMode_Playing,
};

typedef struct {
    i32 Number;
    i32 StartingAsteroids;
    float AsteroidMaxSpeed;
    float AsteroidMinSpeed;
    float SaucerSpawnTime;
    float SaucerSpeed;
    float SaucerShootTime;
    float SaucerCourseChangeTime;
} level;

typedef struct {
    //PLAYING
    level Level;
    u32 Score;
    u32 ExtraLives;
    light_source ScoreLight;
    float GameOverTimer;
    object Ship;
    object Saucer;
    float SaucerSpawnTimer;
    float SaucerCourseChangeTimer;
    float SaucerShootTimer;
    float ShipRespawnTimer;
    bool32 LevelComplete;

    // TODO: Keep these arrays compact. It's better to check against a count than to be loading in extra 
    // memory, I think.
    asteroid Asteroids[MAX_ASTEROIDS];
    i32 AsteroidCount;
    particle Shots[MAX_SHOTS];
    particle Particles[MAX_PARTICLES];
    particle_block *FirstFreeParticleBlock;
    particle_group *FirstParticleGroup;
    particle_group *FirstFreeParticleGroup;
    memory_arena GameArena;
} game_state;

#if DEBUG_BUILD
#include "schmasteroids_editor.h"
#endif

typedef struct metagame_state_ {
    // NOTE: For now, nothing persists past frame boundaries in the transient arena, i.e.
    // it's all temporary memory
    memory_arena TransientArena;
    memory_arena PermanentArena;
    memory_arena WavArena;

    game_memory *GameMemory;

    game_state GameState;
    game_mode CurrentGameMode;
    game_mode NextGameMode;
    float ModeChangeTimer;
    float ModeChangeTimerMax;

    float GlyphYOverX;
    glyph Digits[10];
    glyph Letters[26];
    glyph Decimal;
    
    game_sounds Sounds;
    sound_state SoundState;

    light_params LightParams;

    //LEVEL START SCREEN
    char LevelNumberString[3];
    //START SCREEN
    float StartScreenTimer;
    start_screen_state StartScreenState;
    float InstructionsY;
    menu_options StartMenuOption;

#if DEBUG_BUILD
    edit_mode EditMode;
    editor_state EditorState;
#endif
} metagame_state;

inline game_state *
GetGameState(metagame_state *MetagameState);
    
inline void
SetModeChangeTimer(metagame_state *MetagameState, float Seconds);

inline void
SetUpLevel(metagame_state *MetagameState, i32 LevelNumber);

inline void
SetUpStartScreen(metagame_state *MetagameState);

inline void
SetUpLevelStartScreen(metagame_state *MetagameState, i32 LevelNumber);

#if DEBUG_BUILD
inline void
DebugToggleMusic(metagame_state *Metagame);
#endif

inline bool32
UpdateAndCheckTimer(float *Timer, float SecondsElapsed);

internal asteroid* 
CreateAsteroid(int Size, game_state *GameState, light_params *LightParams);

internal void
SpawnSaucer(game_state *GameState, metagame_state *MetagameState);

internal void
SetSaucerCourse(game_state *GameState);

internal void
ExplodeShip(game_state *GameState, metagame_state *MetagameState);

internal void
ExplodeSaucer(game_state *GameState, metagame_state *MetagameState);

internal void
ExplodeAsteroid(asteroid* Exploding, game_state *GameState, metagame_state *MetagameState);

internal void MoveObject(object *O, 
        float SecondsElapsed, game_state *GameState);

internal void
RenderTestGradient(platform_framebuffer *Backbuffer, game_input *Input);

internal void 
DrawRadarLine(float SecondsElapsed, 
        platform_framebuffer *Backbuffer, game_input *KeyboardInput);

inline particle*
CreateShot(game_state *GameState);

internal void
ShipShoot(game_state *GameState, metagame_state *MetagameState);

internal void
SaucerShootDumb(game_state *GameState, metagame_state *MetagameState);

internal void
SaucerShootLeading(game_state *GameState, metagame_state *MetagameState);

internal glyph
ProjectGlyphIntoRect(glyph *Original, rect Rect);

internal void
PrintScore(platform_framebuffer *Backbuffer, game_state *GameState, metagame_state *MetagameState, u8 Alpha = 255);

internal void
DrawShip(platform_framebuffer *Backbuffer, game_state *GameState, u8 Alpha = 255);

internal void
DrawSaucer(platform_framebuffer *Backbuffer, game_state *GameState, u8 Alpha = 255);

internal void
DrawAsteroids(platform_framebuffer *Backbuffer, game_state *GameState, u8 Alpha = 255);

internal void
DrawParticles(platform_framebuffer *Backbuffer, game_state *GameState, u8 Alpha = 255);

internal void
DrawShots(platform_framebuffer *Backbuffer, game_state *GameState, u8 Alpha = 255);

internal void 
MovePosition(v2 *Pos, v2 Delta, game_state *GameState);

internal void
MoveShots(float SecondsElapsed, game_state *GameState);

internal void
MoveAsteroids(float SecondsElapsed, game_state *GameState);

internal void
MoveParticles(float SecondsElapsed, game_state *GameState);

internal bool32
Collide(object *O1, object *O2, game_state *GameState); 

internal bool32
Collide(particle *P, object *O, game_state *GameState);

internal inline v2
MapFromObjectPosition(v2 Vertex, object O);

//Takes warping into account
internal v2 
ShortestPath(v2 From, v2 To);

internal float
GetAbsoluteDistance(v2 FirstV, v2 SecondV);

internal v2 
MapWindowSpaceToGameSpace(v2 WindowCoords, platform_framebuffer *Backbuffer);

internal v2
MapGameSpaceToWindowSpace(v2, platform_framebuffer *Backbuffer);

internal void WarpPositionToScreen(v2 *Pos);

internal void
DrawLineWarpedWindowSpaceHSL(platform_framebuffer*Backbuffer, metagame_state *MetagameState, hsl_color HSLColor,
                             v2 First, v2 Second);

internal void
DrawLine(platform_framebuffer*Backbuffer, color Color,
        v2 V1, v2 V2);

inline void DrawPixelHSL(platform_framebuffer *Backbuffer, i32 PixelX, i32 PixelY, hsl_color HSLColor);

inline void SetPixelWithAlpha(color *Dest, color Source);

internal void 
SeedPRNG(u32 Seed);

internal inline i32
RandI32();

internal inline float
RandFloat();

internal inline float
RandFloatRange(float Min, float Max);

internal inline float
RandHeading();

internal inline v2
RandV2InRadius(float R);

#define NICKS_GAME_H 1
#endif
