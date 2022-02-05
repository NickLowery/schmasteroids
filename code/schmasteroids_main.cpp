#define internal static
#include "schmasteroids_main.h"

// NOTE: Always include, defines macros as nothing if we're not in benchmark mode
#include "schm_bench.h"
#include "schm_sound.cpp"
#include "schmasteroids_hsl_simd1.h"
#include "schm_render_buffer.h"

#include "schm_glyph.cpp"
#include "schmasteroids_game.cpp"
#include "schmasteroids_titles.cpp"

#if RENDER_BENCHMARK
#include "schmasteroids_render_benchmark.h"
#endif
#if DEBUG_BUILD
#include "schmasteroids_editor.cpp"
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
    SetUpLevel(Metagame, 1);
    SetUpLevelStartScreen(Metagame, 1);
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
                    case (GameMode_Playing): {
                    } break;
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
                    SetUpLevelStartScreen(Metagame, GameState->Level.Number);
                    Metagame->NextGameMode = GameMode_LevelStartScreen;
                } 
#endif
            } break;
            case (GameMode_None): {
                InvalidCodePath;
            }
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
    particle* Result = GameState->Shots;
    particle* OOBShot = Result + MAX_SHOTS;
    while(Result->Exists && Result < OOBShot) {Result++;}
    Assert(Result < OOBShot);

    Result->Exists = true;
    Result->tLMod = 0.0f;
    Result->SecondsToLive = SHOT_LIFETIME;
    return Result;
}
internal void
ShipShoot(game_state *GameState, metagame_state *Metagame) 
{
    particle* S = CreateShot(GameState);
    S->Position = MapFromObjectPosition(GameState->Ship.Vertices[0], GameState->Ship);
    S->Velocity = V2FromAngleAndMagnitude(GameState->Ship.Heading, SHOT_ABS_VEL) +
        GameState->Ship.Velocity;
    S->Light = Metagame->LightParams.ShipShotLightBase;
    S->C_LOriginal = S->Light.C_L;
    PlaySound(Metagame, &Metagame->Sounds.ShipLaser);
}


//TODO: There's definitely common code in these SaucerShoot functions that could be compressed.
internal void
SaucerShootDumb(game_state *GameState, metagame_state *Metagame) 
{
    particle* S = CreateShot(GameState);
    v2 VectorToShip = ShortestPath(GameState->Saucer.Position, GameState->Ship.Position);
    S->Velocity = ScaleV2ToMagnitude(VectorToShip, SHOT_ABS_VEL) +
        GameState->Saucer.Velocity;
    v2 ShotOffset = ScaleV2ToMagnitude(VectorToShip, SAUCER_RADIUS);
    S->Position = ShotOffset + GameState->Saucer.Position;
    S->Light = Metagame->LightParams.SaucerShotLightBase;
    S->C_LOriginal = S->Light.ZDistSq;

    PlaySound(Metagame, &Metagame->Sounds.SaucerLaser);
    GameState->SaucerShootTimer = GameState->Level.SaucerShootTime;

}

internal void
SaucerShootLeading(game_state *GameState, metagame_state *Metagame)
{
    // NOTE: For now this ignores warping!!!
    // TODO: Once this works, factor in warping. Should be relatively simple to use ShortestPath
    // Quadratic equation to find time when shot can hit ship
    v2 RelativeShipPosition = GameState->Ship.Position - GameState->Saucer.Position;
    v2 ShotOffset = ScaleV2ToMagnitude(RelativeShipPosition, SAUCER_RADIUS);
    RelativeShipPosition -= ShotOffset;
    v2 RelativeShipVelocity = GameState->Ship.Velocity - GameState->Saucer.Velocity;
    float A = Square(RelativeShipVelocity.X) + Square(RelativeShipVelocity.Y) - Square(SHOT_ABS_VEL);
    float B = 2.0f * 
        ((RelativeShipVelocity.X * RelativeShipPosition.X) + (RelativeShipVelocity.Y * RelativeShipPosition.Y));
    float C = Square(RelativeShipPosition.X) + Square(RelativeShipPosition.Y);
    float Desc = Square(B) - (4.0f*A*C);
    if(Desc < 0) { // No solution
        SaucerShootDumb(GameState, Metagame);
    } else {
        float HitTime = -1.0f;
        if (Desc == 0) {
            HitTime = -B / (2 * A);
        } else {
            float TwoA = 2 * A;
            float SolutionOne = (-B - Sqrt(Desc)) / TwoA;
            float SolutionTwo = (-B + Sqrt(Desc)) / TwoA;
            float Lower, Higher;
            if(SolutionOne < SolutionTwo) {
                Lower = SolutionOne;
                Higher = SolutionTwo;
            } else {
                Higher = SolutionOne;
                Lower = SolutionTwo;
            }
            if(Lower < 0) {
                HitTime = Higher;
            } else {
                HitTime = Lower;
            }

            if (HitTime < 0) {
                SaucerShootDumb(GameState, Metagame);
            } else {
                v2 RelShipPositionAtHitTime = RelativeShipPosition + (HitTime * RelativeShipVelocity);
                particle* S = CreateShot(GameState);
                S->Velocity = ScaleV2ToMagnitude(RelShipPositionAtHitTime, SHOT_ABS_VEL) + 
                    GameState->Saucer.Velocity;
                S->Position = ShotOffset + GameState->Saucer.Position;
                S->Light = Metagame->LightParams.SaucerShotLightBase;
                S->C_LOriginal = S->Light.ZDistSq;

                PlaySound(Metagame, &Metagame->Sounds.SaucerLaser);
                GameState->SaucerShootTimer = GameState->Level.SaucerShootTime;
            }


        }
    }
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

// TODO: CLEANUP: Confirm this is never called with anything except the screen rect and compress
internal void WarpPositionToRect(v2 *Pos, rect Rect)
{
    v2 RectDim = Dim(Rect);
    if(Pos->X > Rect.Right) {
        do {
            Pos->X -= RectDim.X;
        } while (Pos->X > Rect.Right);
    } else while (Pos->X < Rect.Left) {
        Pos->X += RectDim.X;
    }
    if(Pos->Y > Rect.Bottom) {
        do {
            Pos->Y -= RectDim.Y;
        } while (Pos->Y > Rect.Bottom);
    } else while (Pos->Y < 0.0f) {
        Pos->Y += RectDim.Y;
    }
    Assert(InRect(*Pos, Rect));
}

internal void WarpPositionToScreen(v2 *Pos)
{
    WarpPositionToRect(Pos, ScreenRect);
}

// TODO: Change to by value
inline void MovePosition(v2 *Pos, v2 Delta, game_state *GameState)
{
    *Pos += Delta;
    WarpPositionToRect(Pos, ScreenRect);
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
    asteroid *Result = GameState->Asteroids;
    while(Result->O.Exists) {Result++;}
    Assert(Result - GameState->Asteroids < MAX_ASTEROIDS);

    Result->Size = (u8)Size;
    Result->O.Exists = true;
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

    ++GameState->AsteroidCount;

    return Result;
}

internal void
SpawnSaucer(game_state *GameState, metagame_state *Metagame)
{
    Assert(GameState->Saucer.Exists == false);
    GameState->Saucer.Exists = true;
    i32 Edge = RandI32() % 4;
    switch (Edge) {
        // TOP
        case 0: { 
            GameState->Saucer.Position.Y = SCREEN_TOP;
            GameState->Saucer.Position.X = 
                RandFloatRange(SCREEN_LEFT, SCREEN_RIGHT);
        } break;
        // BOTTOM
        case 1: { 
            GameState->Saucer.Position.Y = SCREEN_BOTTOM;
            GameState->Saucer.Position.X = 
                RandFloatRange(SCREEN_LEFT, SCREEN_RIGHT);
        } break;
        case 2: { 
        //LEFT
            GameState->Saucer.Position.X = SCREEN_LEFT;
            GameState->Saucer.Position.Y = RandFloatRange(SCREEN_TOP, SCREEN_BOTTOM);
        } break;
        case 3: { 
        //RIGHT
            GameState->Saucer.Position.X = SCREEN_RIGHT;
            GameState->Saucer.Position.Y = RandFloatRange(SCREEN_TOP, SCREEN_BOTTOM);
        } break;
    }
    WarpPositionToScreen(&GameState->Saucer.Position);
    GameState->Saucer.CollisionRadius = SAUCER_RADIUS;
    SetSaucerCourse(GameState);
    GameState->SaucerCourseChangeTimer = GameState->Level.SaucerCourseChangeTime;
    GameState->SaucerShootTimer = GameState->Level.SaucerShootTime;
    PlaySound(Metagame, &Metagame->Sounds.SaucerSpawn, 0.6f);
}

internal void
SetSaucerCourse(game_state *GameState) 
{
    if (GameState->Ship.Exists) {
        v2 SaucerToShip = ShortestPath(GameState->Saucer.Position, GameState->Ship.Position);
        GameState->Saucer.Velocity = ScaleV2ToMagnitude(SaucerToShip, GameState->Level.SaucerSpeed);
    }
}


inline void
ExplodeObject(game_state *GameState, object *O, i32 ParticleCount, color Color)
{
    O->Exists = false;
    particle_group *NewGroup;
    if (GameState->FirstFreeParticleGroup) {
        NewGroup = GameState->FirstFreeParticleGroup;
        GameState->FirstFreeParticleGroup = NewGroup->NextGroup;
    } else {
        NewGroup = PushStruct(&GameState->GameArena, particle_group);
    }
    NewGroup->NextGroup = GameState->FirstParticleGroup;
    GameState->FirstParticleGroup = NewGroup;

    NewGroup->H = O->Light.H;
    NewGroup->S = O->Light.S;
    NewGroup->SecondsToLive = PARTICLE_LIFETIME;
    NewGroup->C_LOriginal = O->Light.C_L;
    NewGroup->TotalParticles = ParticleCount;

    particle_block *PreviousBlock = 0;
    while (ParticleCount > 0) {
        particle_block *NewBlock;
        if (GameState->FirstFreeParticleBlock) {
            NewBlock = GameState->FirstFreeParticleBlock;
            GameState->FirstFreeParticleBlock = NewBlock->NextBlock;
        } else {
            NewBlock = PushStruct(&GameState->GameArena, particle_block);
        }
        NewBlock->NextBlock= PreviousBlock;
        if (ParticleCount > MAX_PARTICLES_IN_BLOCK) {
            NewBlock->Count = MAX_PARTICLES_IN_BLOCK;
        } else {
            NewBlock->Count = ParticleCount;
        }
        for (i32 PIndex = 0; PIndex < NewBlock->Count; ++PIndex) {
            NewBlock->ZDistSq[PIndex] = O->Light.ZDistSq;
            v2 Scatter = RandV2InRadius(1.0f);
            NewBlock->Position[PIndex] = O->Position + (Scatter * O->CollisionRadius);
            NewBlock->Velocity[PIndex] = O->Velocity + (Scatter * PARTICLE_VELOCITY);
        }
        ParticleCount -= NewBlock->Count;
        PreviousBlock = NewBlock;
    }
    NewGroup->FirstBlock = PreviousBlock;
}

internal void
ExplodeShip(game_state *GameState, metagame_state *Metagame)
{
    PlaySound(Metagame, &Metagame->Sounds.ShipExplosion, 0.6f);
    ExplodeObject(GameState, &GameState->Ship, SHIP_PARTICLES, Color(SHIP_COLOR));
    if (GameState == &Metagame->GameState) {
        // NOTE: Only if we're actually in gameplay
        if (GameState->ExtraLives > 0) {
            GameState->ShipRespawnTimer = 3.0f;
        } else {
            Metagame->SoundState.MusicFadingOut = true;
            Metagame->SoundState.SecondsLeftInFade = 2.0f;
        }
    }
}

internal void
ExplodeSaucer(game_state *GameState, metagame_state *Metagame)
{
    ExplodeObject(GameState, &GameState->Saucer, SHIP_PARTICLES, Color(SAUCER_COLOR));
    GameState->SaucerSpawnTimer = GameState->Level.SaucerSpawnTime;
    PlaySound(Metagame, &Metagame->Sounds.SaucerExplosion, 0.6f);
    GameState->Score += 1000;
}

internal void
ExplodeAsteroid(asteroid* Exploding, game_state *GameState, metagame_state *Metagame)
{
    --GameState->AsteroidCount;
    if(Exploding->Size > 0) {
        u8 ChildSize = Exploding->Size - 1;
        for (int i = 0; i < AsteroidProps[Exploding->Size].Children; i++) {
            asteroid* Child = CreateAsteroid(ChildSize, GameState, &Metagame->LightParams);
            Child->O.Position = Exploding->O.Position + 
                    RandV2InRadius(AsteroidProps[Exploding->Size].Radius/2);
            WarpPositionToScreen(&Child->O.Position);
            Child->O.Velocity = Exploding->O.Velocity + 
                    RandV2InRadius(EXPLODE_VELOCITY);
            Child->O.Heading = RandFloatRange(0.0f, PI);
            Child->O.Spin = Exploding->O.Spin + 
                RandFloatRange(-EXPLODE_SPIN, EXPLODE_SPIN);
            Child->O.Light = Exploding->O.Light;
        }
    }
    ExplodeObject(GameState, &Exploding->O, AsteroidProps[Exploding->Size].Particles, Color(ASTEROID_COLOR));
    PlaySound(Metagame, &Metagame->Sounds.AsteroidExplosion, 0.25f);
    GameState->Score += AsteroidProps[Exploding->Size].DestroyPoints;
}

internal void
MoveAsteroids(float SecondsElapsed, game_state *GameState) 
{
    for (u32 AIndex = 0; AIndex < (u32)ArrayCount(GameState->Asteroids); AIndex++) {
        if (GameState->Asteroids[AIndex].O.Exists) {
            MoveObject(&(GameState->Asteroids[AIndex].O), SecondsElapsed, GameState);
        }
    }
}

internal void
MoveShots(float SecondsElapsed, game_state *GameState)
{
    for (u32 SIndex = 0; SIndex < (u32)ArrayCount(GameState->Shots); SIndex++) {
        particle *ThisShot = &GameState->Shots[SIndex];
        if (ThisShot->Exists) {
            if (UpdateAndCheckTimer(&ThisShot->SecondsToLive, SecondsElapsed)) {
                ThisShot->Exists = false;
            } else {
                ThisShot->tLMod += 16.0f * SecondsElapsed;
                if (ThisShot->tLMod > 2.0f*PI) {
                    ThisShot->tLMod -= 2.0f*PI;
                }
                ThisShot->Light.C_L = ThisShot->C_LOriginal + (0.5f*Sin(ThisShot->tLMod));
                MoveParticle(ThisShot, SecondsElapsed, GameState);
            }
        }
    }
}

internal void
MoveParticles(float SecondsElapsed, game_state *GameState)
{
    // Move ungrouped particles
    for (u32 PIndex = 0; PIndex < (u32)ArrayCount(GameState->Particles); PIndex++) {
        particle *ThisParticle = &GameState->Particles[PIndex];
        if (ThisParticle->Exists) {
            if (UpdateAndCheckTimer(&ThisParticle->SecondsToLive, SecondsElapsed)) {
                ThisParticle->Exists = false;
            } else {
                ThisParticle->Light.C_L = ThisParticle->C_LOriginal * (ThisParticle->SecondsToLive / PARTICLE_LIFETIME);
                MoveParticle(ThisParticle, SecondsElapsed, GameState);
            }
        }
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

internal inline v2
MapFromObjectPosition(v2 V, object O)
{
    v2 Result = Rotate(V, O.Heading);
    Result += O.Position;
    return Result;
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
