#ifndef SCHMASTEROIDS_RENDER_BENCHMARK_H
#define SCHMASTEROIDS_RENDER_BENCHMARK_H 1




internal void
SetUpRenderBenchmark(game_state *GameState, metagame_state *Metagame)
{
    GlobalFrameCount = 0;

    if (GameState->GameArena.Base) {
        ResetArena(&GameState->GameArena);
    } else {
        u64 GameArenaSize = Megabytes(1);
        void *GameMemory = PushSize(&Metagame->PermanentArena, GameArenaSize);
        InitializeArena(&GameState->GameArena, GameArenaSize, GameMemory);
    }

    GameState->LevelComplete = false;
    level *Level = &GameState->Level;
    Level->StartingAsteroids = MAX_ASTEROIDS;
    Level->AsteroidMaxSpeed = 70.0f;
    Level->AsteroidMinSpeed = 20.0f;
    Level->SaucerSpawnTime = 20.0f;
    Level->SaucerSpeed = 50.0f;
    Level->SaucerShootTime = 2.0f;
    Level->SaucerCourseChangeTime = 3.0f;

    GameState->Ship.Exists = false;
    ZeroArray(GameState->Asteroids, asteroid);
    GameState->AsteroidCount = 0;
    ZeroArray(GameState->Shots, particle);
    ZeroArray(GameState->Particles, particle);

    light_params BenchmarkLightParams = {};
    BenchmarkLightParams = {};
    BenchmarkLightParams.ShipLightMin.ZDistSq = 3.0f;
    BenchmarkLightParams.ShipLightMin.H = 0.66f;
    BenchmarkLightParams.ShipLightMin.S = 0.1f;
    BenchmarkLightParams.ShipLightMin.C_L = 2.1f;

    /*
    BenchmarkLightParams.SaucerLightMin.ZDistSq = 3.0f;
    BenchmarkLightParams.SaucerLightMin.H = 0.8f;
    BenchmarkLightParams.SaucerLightMin.S = 0.9f;
    BenchmarkLightParams.SaucerLightMin.C_L = 2.1f;
    */

    BenchmarkLightParams.SaucerShotLightBase.H = 0.67f;
    BenchmarkLightParams.SaucerShotLightBase.S = 0.9f;
    BenchmarkLightParams.SaucerShotLightBase.ZDistSq = 3.0f;
    BenchmarkLightParams.SaucerShotLightBase.C_L = 2.7f;
    BenchmarkLightParams.SaucerShotC_LRange = 0.1f;

    BenchmarkLightParams.ShipShotLightBase.H = 0.33f;
    BenchmarkLightParams.ShipShotLightBase.S = 0.9f;
    BenchmarkLightParams.ShipShotLightBase.ZDistSq = 3.0f;
    BenchmarkLightParams.ShipShotLightBase.C_L = 2.7f;
    BenchmarkLightParams.ShipShotC_LRange = 0.1f;

    BenchmarkLightParams.AsteroidLightMin.H = 0.0f;
    BenchmarkLightParams.AsteroidLightMin.S = 0.6f;
    BenchmarkLightParams.AsteroidLightMin.C_L = 2.4f;
    BenchmarkLightParams.AsteroidLightMin.ZDistSq = 3.0f;

    BenchmarkLightParams.AsteroidHRange = 1.0f;
    BenchmarkLightParams.AsteroidSRange = 0.4f;
    BenchmarkLightParams.AsteroidC_LRange = 0.0f;
    BenchmarkLightParams.AsteroidZDistSqRange = 0.0f;

    for(int AsteroidIndex = 0; AsteroidIndex < Level->StartingAsteroids; AsteroidIndex++) {
        asteroid *A = CreateAsteroid(0, GameState, &BenchmarkLightParams);
        A->O.Position = {RandFloatRange(0.0f, SCREEN_WIDTH), RandFloatRange(0.0f, SCREEN_HEIGHT)};
        A->O.Velocity = V2FromAngleAndMagnitude(
                RandHeading(), 
                RandFloatRange(Level->AsteroidMinSpeed, Level->AsteroidMaxSpeed));
        A->O.Heading = RandHeading();
        A->O.Spin = RandFloatRange(-INIT_ASTEROID_MAX_SPIN, INIT_ASTEROID_MAX_SPIN);
    }
    GameState->Saucer.Exists = false;
}

internal void
UpdateAndDrawRenderBenchmark(metagame_state *Metagame, game_memory *GameMemory, render_buffer *Renderer, game_input *Input)
{
    game_state *GameState = GetGameState(Metagame);
    GlobalFramesToRender = 150;

    float dTime = Input->SecondsElapsed;

    if (GlobalFrameCount < GlobalFramesToRender)
    {
        BENCH_START_FRAME_ALL

        BENCH_START_COUNTING_CYCLES_USECONDS(UpdateGame)
        MoveAsteroids(dTime, GameState);
        // Explode an asteroid each frame so we are testing particles too
        i32 AIndex = 0;
        while (!GameState->Asteroids[AIndex].O.Exists && AIndex < ArrayCount(GameState->Asteroids)) {
            ++AIndex;
        }
        ExplodeAsteroid(&GameState->Asteroids[AIndex], GameState, Metagame);

        MoveParticles(dTime, GameState);
        


        BENCH_STOP_COUNTING_CYCLES_USECONDS(UpdateGame)

        BENCH_START_COUNTING_CYCLES_USECONDS(DrawGame)

        //DrawAsteroidsHSL(Backbuffer, GameState);
        PushAsteroids(Renderer, GameState);
        for (particle_group *Group = GameState->FirstParticleGroup;
            Group;
            Group = Group->NextGroup)
        {
            float C_L = Lerp(0.0f, Group->SecondsToLive / PARTICLE_LIFETIME, Group->C_LOriginal);
            PushParticleGroup(Renderer, Group, C_L);
        }

        BENCH_STOP_COUNTING_CYCLES_USECONDS(DrawGame)

        BENCH_DUMP_FRAME_BMP

    } 
}

#endif
