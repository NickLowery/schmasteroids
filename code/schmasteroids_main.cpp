#define internal static
#include "schm_platform_includes.h"
#include "schm_main.h"
#include "schm_platform.h"
#include "schm_math.h"
#include "schm_math.cpp"
#include "schm_strings.h"
#include "schm_sound.h"
#include "schm_color.h"
#include "schm_glyph.h"
#include "schm_start_screen.h"
#include "schm_game.h"
#if DEBUG_BUILD
#include "schm_editor.h"
#endif
#include "schm_metagame.h"

// NOTE: Always include, defines macros as nothing if we're not in benchmark mode
#include "schm_bench.h"
#include "schm_sound.cpp"
#include "schm_hsl_simd1.h"
#include "schm_render_buffer.h"

// NOTE: These glyphs are intended to be scaled to a rectangle, 
// so the coordinates should all be in a [0,1] interval.
#include "schm_glyph.cpp"
#include "schm_game.cpp"
#include "schm_titles.cpp"

#if RENDER_BENCHMARK
#include "schm_render_benchmark.h"
#endif
#if DEBUG_BUILD
#include "schm_editor.cpp"
#endif

inline game_state *
GetGameState(metagame_state *MetagameState)
{
    game_state *Result = &MetagameState->GameState;
    return Result;
}

extern "C"
GAME_SEED_PRNG(SeedRandom)
{
    SeedPRNG(Seed);
}

extern "C"
GAME_INITIALIZE(GameInitialize) 
{
    Assert(sizeof(metagame_state) < GameMemory->PermanentStorageSize);
    metagame_state *Metagame = (metagame_state*)GameMemory->PermanentStorage;
    Metagame->GameMemory = GameMemory;
    game_state *GameState = GetGameState(Metagame);
    Assert(GameMemory->IsSetToZero);
    GameMemory->IsSetToZero = false;
    InitializeArena(&Metagame->TransientArena, GameMemory->TransientStorageSize, GameMemory->TransientStorage);
    InitializeArena(&Metagame->PermanentArena, GameMemory->PermanentStorageSize - sizeof(*Metagame),
                    (char*)GameMemory->PermanentStorage + sizeof(*Metagame));
    u64 WavArenaSize = Megabytes(32);
    void* WavMemory = PushSize(&Metagame->PermanentArena, WavArenaSize);
    InitializeArena(&Metagame->WavArena, WavArenaSize, WavMemory);

    Assert(&(Metagame->Sounds.Terminator) == &(Metagame->Sounds.Wavs[ArrayCount(Metagame->Sounds.Wavs)-1]));
    Metagame->Sounds.ShipLaser = LoadWav(GameMemory, &Metagame->WavArena, "ShipLaser.wav");
    Metagame->Sounds.SaucerLaser = LoadWav(GameMemory, &Metagame->WavArena, "SaucerLaser.wav");
    Metagame->Sounds.AsteroidExplosion= LoadWav(GameMemory, &Metagame->WavArena, "AsteroidExplosion.wav");
    Metagame->Sounds.ShipExplosion= LoadWav(GameMemory, &Metagame->WavArena, "ShipExplosion.wav");
    Metagame->Sounds.SaucerExplosion = LoadWav(GameMemory, &Metagame->WavArena, "SaucerExplosion.wav");
    Metagame->Sounds.SaucerSpawn = LoadWav(GameMemory, &Metagame->WavArena, "SaucerSpawn.wav");

    Metagame->Sounds.GameLoop.MeasureCount = 24;
    Metagame->Sounds.GameLoop.Wav = LoadWav(GameMemory, &Metagame->WavArena, "test-music-loop.wav");
    Metagame->Sounds.OneBarKick.MeasureCount = 1;
    Metagame->Sounds.OneBarKick.Wav = LoadWav(GameMemory, &Metagame->WavArena, "OneBarKick.wav");
    Metagame->Sounds.TitleIntro.MeasureCount = 8;
    Metagame->Sounds.TitleIntro.Wav = LoadWav(GameMemory, &Metagame->WavArena, "rough-title-intro.wav");
    Metagame->Sounds.TitleLoop.MeasureCount = 8;
    Metagame->Sounds.TitleLoop.Wav = LoadWav(GameMemory, &Metagame->WavArena, "rough-title-loop.wav");
    Metagame->SoundState.CurrentClip = &Metagame->Sounds.TitleIntro;
    Metagame->SoundState.NextClip = &Metagame->Sounds.TitleLoop;
    ResetClip(Metagame->SoundState.CurrentClip);
    ResetClip(Metagame->SoundState.NextClip);


#if DEBUG_BUILD
    editor_state *EditorState = GetEditorState(Metagame);
    u64 EditMemorySize = Kilobytes(16);
    InitializeArena(&EditorState->EditArena, 
            EditMemorySize,
            PushSize(&Metagame->PermanentArena, EditMemorySize));
#endif

    GameState->Ship.VerticesCount = 4;
    GameState->Ship.CollisionRadius = SHIP_COLLISION_RADIUS;
    GameState->Ship.Vertices[0] = {20.0f, 0.0f};
    GameState->Ship.Vertices[1] = {-5.0f, -10.0f};
    GameState->Ship.Vertices[2] = {-2.5f, 0.0f};
    GameState->Ship.Vertices[3] = {-5.0f, 10.0f};

    GameState->Saucer.Spin = SAUCER_SPIN;
    GameState->Saucer.VerticesCount = MAX_VERTICES;
    for(int VIndex = 0; VIndex < MAX_VERTICES; VIndex++) {
        float Angle = (2.0f*PI)/MAX_VERTICES * VIndex;
        GameState->Saucer.Vertices[VIndex] = V2FromAngleAndMagnitude(Angle, SAUCER_RADIUS);
    }
    GameState->Saucer.CollisionRadius = SAUCER_RADIUS;

    // TODO: Update these defaults with what I generated in the editor?
    Metagame->LightParams = {};
    Metagame->LightParams.ShipLightMin.ZDistSq = 3.0f;
    Metagame->LightParams.ShipLightMin.H = 0.66f;
    Metagame->LightParams.ShipLightMin.S = 0.1f;
    Metagame->LightParams.ShipLightMin.C_L = 2.1f;

    Metagame->LightParams.SaucerLightBase.ZDistSq = 4.0f;
    Metagame->LightParams.SaucerLightBase.H = 0.8f;
    Metagame->LightParams.SaucerLightBase.S = 0.9f;
    Metagame->LightParams.SaucerLightBase.C_L = 0.5f;
    Metagame->LightParams.SaucerC_LRange = 2.5f;

    Metagame->LightParams.SaucerShotLightBase.H = 0.67f;
    Metagame->LightParams.SaucerShotLightBase.S = 0.9f;
    Metagame->LightParams.SaucerShotLightBase.ZDistSq = 3.0f;
    Metagame->LightParams.SaucerShotLightBase.C_L = 2.7f;
    Metagame->LightParams.SaucerShotC_LRange = 0.1f;
    Metagame->LightParams.SaucerHCharged = 0.99f;
    Metagame->LightParams.SaucerHChangeThreshold = 0.9f;

    Metagame->LightParams.ShipShotLightBase.H = 0.33f;
    Metagame->LightParams.ShipShotLightBase.S = 0.9f;
    Metagame->LightParams.ShipShotLightBase.ZDistSq = 3.0f;
    Metagame->LightParams.ShipShotLightBase.C_L = 2.7f;
    Metagame->LightParams.ShipShotC_LRange = 0.1f;

    Metagame->LightParams.AsteroidLightMin.H = 0.0f;
    Metagame->LightParams.AsteroidLightMin.S = 0.2f;
    Metagame->LightParams.AsteroidLightMin.C_L = 2.4f;
    Metagame->LightParams.AsteroidLightMin.ZDistSq = 3.0f;

    Metagame->LightParams.AsteroidHRange = 0.16f;
    Metagame->LightParams.AsteroidSRange = 0.4f;
    Metagame->LightParams.AsteroidC_LRange = 0.0f;
    Metagame->LightParams.AsteroidZDistSqRange = 0.0f;
    u32 LightParamsBytesRead = GameMemory->PlatformDebugReadFile("lightparams",
                                                      (void*) &Metagame->LightParams, 
                                                      sizeof(Metagame->LightParams));

    GameState->Saucer.Light = Metagame->LightParams.SaucerLightBase;
    GameState->Ship.Light = Metagame->LightParams.ShipLightMin;
    GameState->ScoreLight.H = 0.0f;
    GameState->ScoreLight.S = 0.0f;
    GameState->ScoreLight.C_L = 2.0f;
    GameState->ScoreLight.ZDistSq = 2.0f;

    Metagame->GlyphYOverX = 1.7f;
    u32 GlyphSize = sizeof(glyph);
    for(int Digit = 0;
        Digit < 10;
        Digit++)
    {
        char Filename[7] = "digit";
        Filename[5] = '0' + (char)Digit;
        Filename[6] = '\0';
        u32 Result = GameMemory->PlatformDebugReadFile(Filename, 
                (void*) &Metagame->Digits[Digit], GlyphSize);
        Assert(Result == GlyphSize);
    }
    for (int LetterIndex = 0;
         LetterIndex < 26;
         LetterIndex++)
    {
        char Filename[7] = "letter";
        Filename[5] = 'a' + (char)LetterIndex;
        Filename[6] = '\0';
        u32 Result = GameMemory->PlatformDebugReadFile(Filename, 
                (void*) &Metagame->Letters[LetterIndex], GlyphSize);
        if (Result != GlyphSize) {
            Metagame->Letters[LetterIndex] = {};
        }
    }

    // Super sweet procedural decimal point.
    Metagame->Decimal = glyph{};
    Metagame->Decimal.SegCount = 4;
    float DecSize = 0.01f;
    float DecLeft = 0.5f - DecSize*0.5f;
    float DecRight = 0.5f + DecSize*0.5f;
    float DecBottom = 1.0f;
    float DecTop = 1.0f - DecSize;
    Metagame->Decimal.Segs[0] = {V2(DecLeft, DecTop), V2(DecRight, DecTop)};
    Metagame->Decimal.Segs[1] = {V2(DecRight, DecBottom), V2(DecRight, DecTop)};
    Metagame->Decimal.Segs[2] = {V2(DecLeft, DecBottom), V2(DecRight, DecBottom)};
    Metagame->Decimal.Segs[3] = {V2(DecLeft, DecTop), V2(DecLeft, DecBottom)};

#if RENDER_BENCHMARK
    SeedPRNG(42);
    SetUpRenderBenchmark(GameState, Metagame);
    
    Metagame->SoundState.CurrentClip = 0;
    Metagame->CurrentGameMode = GameMode_Playing;
#elif CREATE_GAME_BENCHMARK
    SetUpLevel(Metagame, 10);
    Metagame->CurrentGameMode = GameMode_Playing;
#elif RUN_GAME_BENCHMARK
    SeedPRNG(42);
    GlobalFrameCount = 0;
    GlobalFramesToRender = 120;
#else
    i32 StartingLevel = 1;
    SetUpLevel(Metagame, StartingLevel);
    SetUpLevelStartScreen(Metagame, StartingLevel);
    SetUpStartScreen(Metagame);
    Metagame->CurrentGameMode = GameMode_StartScreen;
#endif
}

extern "C"
GAME_UPDATE_AND_RENDER(GameUpdateAndRender) 
{
    Assert(sizeof(metagame_state) < GameMemory->PermanentStorageSize);
    metagame_state *Metagame = (metagame_state*)GameMemory->PermanentStorage;
    game_state *GameState = GetGameState(Metagame);
    BENCH_START_FRAME_ALL
    CheckArenaNotTemporary(&Metagame->TransientArena);
    CheckArenaNotTemporary(&Metagame->PermanentArena);
    ResetArena(&Metagame->TransientArena);
    render_buffer OutputRender = AllocateRenderBuffer(&Metagame->TransientArena, Megabytes(1));
    
    float dTime = Input->SecondsElapsed;

#if 0
    // NOTE: Turn on to get flashes on the beat
    bool32 BeatThisFrame = false;
    if (Metagame->SoundState.CurrentClip) {
        float BeatsPerSecond = (float)BEATS_PER_MINUTE / 60.0f;
        float MeasuresPerSecond = BeatsPerSecond / 4.0f;
        music_position_data Playhead = GetMusicPositionData(&Metagame->SoundState);
        float PortionOfMeasureThisFrame = dTime * MeasuresPerSecond;
        if ((Playhead.PositionInMeasure < 1.0f && 
             Playhead.PositionInMeasure + PortionOfMeasureThisFrame > 1.0f) ||
            (Playhead.PositionInMeasure < 0.25f && 
             Playhead.PositionInMeasure + PortionOfMeasureThisFrame > 0.25f) ||
            (Playhead.PositionInMeasure < 0.5f && 
             Playhead.PositionInMeasure + PortionOfMeasureThisFrame > 0.5f) ||
            (Playhead.PositionInMeasure < 0.75f && 
             Playhead.PositionInMeasure + PortionOfMeasureThisFrame > 0.75f)) 
        {
            BeatThisFrame = true;
        }
    }
    color ClearColor = BeatThisFrame ? Color(0x00444444) : Color(0x00000000);
    PushClear(&OutputRender, ClearColor);
#else
    PushClear(&OutputRender, Color(0x00000000));
#endif

#if DEBUG_BUILD
    if (WentDown(&Input->DebugKeys.F6)) {
        DebugToggleMusic(Metagame);
    }
    if (WentDown(&Input->DebugKeys.F7)) {
        if(Input->Keyboard.Shift.IsDown) {
            PrevEditMode(GameMemory);
        } else {
            NextEditMode(GameMemory);
        }
    }
    if (WentDown(&Input->DebugKeys.F8)) {
        if(Input->Keyboard.Shift.IsDown && GameState->ExtraLives >= 0) {
            --GameState->ExtraLives;
        } else {
            ++GameState->ExtraLives;
        }
    }
    if (WentDown(&Input->DebugKeys.F11)) {
        GameMemory->PlatformQuit();
    }

    if(Metagame->EditMode) {
        v2 MousePosition = MapWindowSpaceToGameSpace(
                V2((float)Input->Mouse.WindowX, (float)Input->Mouse.WindowY),
                Backbuffer);
        v2 LastMousePosition = MapWindowSpaceToGameSpace(
                V2((float)Input->Mouse.LastWindowX, (float)Input->Mouse.LastWindowY),
                Backbuffer);
        RunEditor(GameMemory, &OutputRender, Input, MousePosition, LastMousePosition);
    }
    else 
#endif
    {
        float CurrentSceneAlpha = 1.0f;
        float NextSceneAlpha = 0.0f;
        if (Metagame->NextGameMode) {
            Assert(Metagame->ModeChangeTimerMax >= Metagame->ModeChangeTimer);
            bool32 TransitionFinished = UpdateAndCheckTimer(&Metagame->ModeChangeTimer, dTime);
            if (TransitionFinished) {
                switch(Metagame->CurrentGameMode) {
                    case (GameMode_LevelStartScreen): {
                    } break;
                    case (GameMode_StartScreen): {
                    } break;
                    case (GameMode_Playing): {
                        SetUpLevel(Metagame, GameState->Level.Number);
                    } break;
                    InvalidDefault;
                }
                switch(Metagame->NextGameMode) {
                    case (GameMode_LevelStartScreen): {
                        Metagame->StartScreenTimer = LEVEL_START_SCREEN_HOLD_TIME;
                    } break;
                    case (GameMode_StartScreen): {
                        SetUpLevel(Metagame, 1);
                        SetUpLevelStartScreen(Metagame, 1);
                    } break;
                    case (GameMode_Playing): 
                    case (GameMode_EndScreen):
                    break;
                    InvalidDefault;                        
                }
                Metagame->CurrentGameMode = Metagame->NextGameMode;
                Metagame->NextGameMode = GameMode_None;

            } else {
                NextSceneAlpha = (Lerp(1.0f, Metagame->ModeChangeTimer/Metagame->ModeChangeTimerMax, 
                                        0.0f));
                CurrentSceneAlpha = 1.0f - NextSceneAlpha;
            }
        } 

        switch (Metagame->CurrentGameMode)
        {
            case (GameMode_StartScreen): {
                UpdateAndDrawTitleScreen(Metagame, &OutputRender, Input, CurrentSceneAlpha);
            } break;
            case (GameMode_LevelStartScreen): {
                UpdateAndDrawLevelStartScreen(Metagame, &OutputRender, Input, CurrentSceneAlpha);
            } break;
            case (GameMode_Playing): {
#if RENDER_BENCHMARK
                UpdateAndDrawRenderBenchmark(Metagame, GameMemory, &OutputRender, Input);
#else
                if (GameState->GameOverTimer < 0.0f && Metagame->NextGameMode != GameMode_StartScreen) {
                    Metagame->NextGameMode = GameMode_StartScreen;
                    SetModeChangeTimer(Metagame, 3.0f);
                    SetUpStartScreen(Metagame);
                }
                UpdateAndDrawGameplay(GameState, &OutputRender, Input, Metagame, CurrentSceneAlpha);
                if(GameState->LevelComplete && !Metagame->NextGameMode) {
                    SetModeChangeTimer(Metagame, 3.0f);
                    if (GameState->Level.Number > MAX_LEVEL) {
                        SetUpEndScreen(Metagame);
                        Metagame->NextGameMode = GameMode_EndScreen;
                    } else {
                        SetUpLevelStartScreen(Metagame, GameState->Level.Number);
                        Metagame->NextGameMode = GameMode_LevelStartScreen;
                    }
                } 
#endif
            } break;
            case (GameMode_EndScreen): {
                DrawEndScreen(Metagame, &OutputRender, Input, CurrentSceneAlpha);
            } break;
            case (GameMode_None): {
                InvalidCodePath;
            } break;
            
            InvalidDefault;
        }
        switch(Metagame->NextGameMode) 
        {
            case (GameMode_StartScreen): {
            } break;
            case (GameMode_LevelStartScreen): {
                UpdateAndDrawLevelStartScreen(Metagame, &OutputRender, Input, NextSceneAlpha);
            } break;
            case (GameMode_Playing): {
                UpdateAndDrawGameplay(GameState, &OutputRender, Input, Metagame, NextSceneAlpha);
            } break;
            case (GameMode_EndScreen): {
                DrawEndScreen(Metagame, &OutputRender, Input, NextSceneAlpha);
            } break;
            case (GameMode_None): break;
            InvalidDefault;
        }
    }
    BENCH_START_COUNTING_CYCLES_USECONDS_NO_INC(DrawGame)
    Render(&OutputRender, Backbuffer, &Metagame->TransientArena);
    BENCH_STOP_COUNTING_CYCLES_USECONDS(DrawGame)
#if RENDER_BENCHMARK
    BENCH_FINISH_FRAME_ALL
    if (GlobalFrameCount == GlobalFramesToRender) {
        BENCH_FINISH_ALL
        BENCH_WRITE_LOG_TO_FILE("render_log.txt")

        GameMemory->PlatformQuit();
    }
#endif

#if RUN_GAME_BENCHMARK
    if (GlobalFrameCount == GlobalFramesToRender) {
        BENCH_FINISH_ALL
        BENCH_WRITE_LOG_TO_FILE("GameBenchmark.txt")
        Metagame->SoundState.CurrentClip = 0;

        GameMemory->PlatformQuit();
    }
#endif

        
}

inline void
SetUpStartScreen(metagame_state *Metagame)
{
    Metagame->StartScreenState = StartScreenState_TitleAppearing;
    Metagame->StartScreenTimer = TITLE_APPEAR_TIME;
    Metagame->StartMenuOption = MenuOption_Play;
}

inline void
SetUpLevelStartScreen(metagame_state *Metagame, i32 LevelNumber)
{
    Assert(LevelNumber <= 99);

    SetStringFromNumber(Metagame->LevelNumberString, LevelNumber, ArrayCount(Metagame->LevelNumberString));
}

inline void
SetModeChangeTimer(metagame_state *Metagame, float Seconds)
{
    Metagame->ModeChangeTimer = Seconds;
    Metagame->ModeChangeTimerMax = Seconds;
}


inline void
SetUpLevel(metagame_state *Metagame, i32 LevelNumber)
{
    game_state *GameState = GetGameState(Metagame);
    Assert(LevelNumber <= 99);
    InitGameState(GameState, LevelNumber, Metagame);
}

inline bool32
UpdateAndCheckTimer(float *Timer, float SecondsElapsed)
{
    *Timer -= SecondsElapsed;
    bool32 TimeUp = (*Timer <= 0.0f);
    return TimeUp;
}

// We just use simple circular collisions
internal bool32
Collide(object *O1, object *O2, game_state *GameState) 
{
    return (GetAbsoluteDistance(O1->Position, O2->Position) < (O1->CollisionRadius + O2->CollisionRadius)); 
}

internal bool32
Collide(particle *P, object *O, game_state *GameState)
{
    return (GetAbsoluteDistance(P->Position, O->Position) < (O->CollisionRadius));
}

inline particle*
CreateShot(game_state *GameState)
{
    particle* Result = &GameState->Shots[GameState->ShotCount++];
    Assert(GameState->ShotCount < MAX_SHOTS);

    Result->tLMod = 0.0f;
    Result->SecondsToLive = SHOT_LIFETIME;
    return Result;
}

internal float
GetAbsoluteDistance(v2 FirstV, v2 SecondV) 
{
    return Length(ShortestPath(FirstV, SecondV));
}

internal v2 
ShortestPath(v2 From, v2 To)
{
    Assert(InRect(From, ScreenRect));
    Assert(InRect(To, ScreenRect));

    v2 Diff = To - From;

    float HalfScreenWidth = SCREEN_WIDTH/2.0f;
    if (Diff.X > HalfScreenWidth) {
        Diff.X -= SCREEN_WIDTH;
    }
    if (Diff.X < -HalfScreenWidth) {
        Diff.X += SCREEN_WIDTH;
    }
    float HalfScreenHeight = SCREEN_HEIGHT/2.0f;
    if (Diff.Y > HalfScreenHeight) {
        Diff.Y -= SCREEN_HEIGHT;
    }
    if (Diff.Y < -HalfScreenHeight) {
        Diff.Y += SCREEN_HEIGHT;
    }

    return Diff;
}

internal void MoveObject(object *O, float SecondsElapsed, game_state *GameState) 
{
    MovePosition(&O->Position, SecondsElapsed * O->Velocity, GameState);
    O->Heading += (SecondsElapsed * O->Spin);
}


internal void MoveParticle(particle *P, float SecondsElapsed, game_state *GameState)
{
    MovePosition(&P->Position, SecondsElapsed * P->Velocity, GameState);
}

internal void WarpPositionToScreen(v2 *Pos)
{
    if(Pos->X > SCREEN_RIGHT) {
        do {
            Pos->X -= SCREEN_WIDTH;
        } while (Pos->X > SCREEN_RIGHT);
    } else while (Pos->X < SCREEN_LEFT) {
        Pos->X += SCREEN_WIDTH;
    }
    if(Pos->Y > SCREEN_BOTTOM) {
        do {
            Pos->Y -= SCREEN_HEIGHT;
        } while (Pos->Y > SCREEN_BOTTOM);
    } else while (Pos->Y < SCREEN_TOP) {
        Pos->Y += SCREEN_HEIGHT;
    }
    Assert(InRect(*Pos, ScreenRect));
}

inline void MovePosition(v2 *Pos, v2 Delta, game_state *GameState)
{
    *Pos += Delta;
    WarpPositionToScreen(Pos);
}

inline void 
MovePositionUnwarped(v2 *Pos, v2 Delta)
{
    *Pos += Delta;
}

// Calling code is in charge of all the other object properties except the collision radius
// and vertices
internal asteroid*
CreateAsteroid(int Size, game_state *GameState, light_params *LightParams)
{
    Assert(Size <= MAX_ASTEROID_SIZE);
    asteroid *Result = GameState->Asteroids + GameState->AsteroidCount++;

    Result->Size = (u8)Size;
    Result->O.CollisionRadius = AsteroidProps[Size].Radius;

    Result->O.Light = LightParams->AsteroidLightMin;
    Result->O.Light.H += RandFloatRange(0.0f, LightParams->AsteroidHRange);
    Result->O.Light.H = EuclideanMod(Result->O.Light.H, 1.0f);
    Result->O.Light.S += RandFloatRange(0.0f, LightParams->AsteroidSRange);
    Result->O.Light.ZDistSq += RandFloatRange(0.0f, LightParams->AsteroidZDistSqRange);
    Result->O.Light.C_L += RandFloatRange(0.0f, LightParams->AsteroidC_LRange);
        
    int VCount = Result->O.VerticesCount = MAX_VERTICES;
    for(int VIndex = 0; VIndex < VCount; VIndex++) {
        float Angle = (2.0f * PI / (float)VCount) * VIndex;
        v2 Vertex = V2FromAngleAndMagnitude(Angle, AsteroidProps[Size].Radius);
        Vertex += RandV2InRadius(AsteroidProps[Size].Irregularity);
        Result->O.Vertices[VIndex] = Vertex;
    }

    return Result;
}


internal void
MoveAsteroids(float SecondsElapsed, game_state *GameState) 
{
    for (u32 AIndex = 0; AIndex < GameState->AsteroidCount; AIndex++) {
        MoveObject(&(GameState->Asteroids[AIndex].O), SecondsElapsed, GameState);
    }
}

internal void
MoveShots(float SecondsElapsed, game_state *GameState)
{
    for (u32 SIndex = 0; SIndex < GameState->ShotCount; SIndex++) {
        particle *ThisShot = &GameState->Shots[SIndex];
        if (UpdateAndCheckTimer(&ThisShot->SecondsToLive, SecondsElapsed)) {
            RemoveShot(GameState, SIndex);
        } 

        // We could be moving a shot we just invalidated if it's the last one in the array
        // but it won't hurt anything
        ThisShot->tLMod += 16.0f * SecondsElapsed;
        if (ThisShot->tLMod > 2.0f*PI) {
            ThisShot->tLMod -= 2.0f*PI;
        }
        ThisShot->Light.C_L = ThisShot->C_LOriginal + (0.5f*Sin(ThisShot->tLMod));
        MoveParticle(ThisShot, SecondsElapsed, GameState);
    }
}

internal void
MoveParticles(float SecondsElapsed, game_state *GameState)
{
    // Move ungrouped particles
    for (u32 PIndex = 0; PIndex < GameState->ParticleCount; PIndex++) {
        particle *ThisParticle = &GameState->Particles[PIndex];
        if (UpdateAndCheckTimer(&ThisParticle->SecondsToLive, SecondsElapsed)) {
            RemoveParticle(GameState, PIndex);
        }
        // We could be moving a particle we just invalidated but it won't hurt anything
        ThisParticle->Light.C_L = ThisParticle->C_LOriginal * (ThisParticle->SecondsToLive / PARTICLE_LIFETIME);
        MoveParticle(ThisParticle, SecondsElapsed, GameState);
    }
    // Move particles in groups
    for (particle_group **GroupPointer = &GameState->FirstParticleGroup;
         *GroupPointer;
        )
    {
        particle_group *Group = *GroupPointer;
        if (UpdateAndCheckTimer(&Group->SecondsToLive, SecondsElapsed)) {
            particle_block *Block = Group->FirstBlock;
            while(Block->NextBlock) {
                Block = Block->NextBlock;
            }
            Block->NextBlock= GameState->FirstFreeParticleBlock;
            GameState->FirstFreeParticleBlock = Group->FirstBlock;

            particle_group *NextGroupToProcess = Group->NextGroup;
            Group->NextGroup = GameState->FirstFreeParticleGroup;
            GameState->FirstFreeParticleGroup = Group;
            *GroupPointer = NextGroupToProcess;
        } else {
            for (particle_block *Block = Group->FirstBlock;
                Block;
                Block = Block->NextBlock)
            {
                for (i32 PIndex = 0;
                    PIndex < Block->Count;
                    ++PIndex)
                {
                    MovePositionUnwarped(&Block->Position[PIndex], Block->Velocity[PIndex] * SecondsElapsed);
                }
            }
            GroupPointer = &Group->NextGroup;
        }
    }
}

internal v2 MapWindowSpaceToGameSpace(v2 WindowCoords, platform_framebuffer *Backbuffer)
{
    v2 Result;
    Result.X = (SCREEN_WIDTH / Backbuffer->Width ) * (WindowCoords.X + 0.5f);
    Result.Y = (SCREEN_HEIGHT / Backbuffer->Height ) * (WindowCoords.Y + 0.5f);
    return Result;
}

internal v2 MapGameSpaceToWindowSpace(v2 In, platform_framebuffer *Backbuffer)
{
    v2 Result;
    Result.X = ((Backbuffer->Width / SCREEN_WIDTH) * In.X) - 0.5f;
    Result.Y = ((Backbuffer->Height / SCREEN_HEIGHT) * In.Y) - 0.5f;
    return Result;
}
