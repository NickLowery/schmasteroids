internal void
PushAsteroids(render_buffer *Renderer, game_state *GameState, float Alpha = 1.0f)
{
    for (u32 AIndex = 0; AIndex < GameState->AsteroidCount; AIndex++) {
        asteroid *A = GameState->Asteroids + AIndex;
        PushObject(Renderer, &A->O, Alpha);
    }
}

// NOTE: Will return null if there is no space for more particles
// NOTE: This is to be used for particles that don't make sense to render as a group, as of this moment,
// that's the ship's exhaust only
internal particle*
CreateParticle(game_state *GameState)
{
    particle *Result = GameState->Particles;
    while(Result->Exists) {Result++;}
    if (Result - GameState->Particles < MAX_PARTICLES) {

        Result->SecondsToLive = PARTICLE_LIFETIME;
        Result->Exists = true;
    } else {
        Result = 0;
    }
    return Result;
}

internal void
PrintScore(render_buffer *Renderer, game_state *GameState, metagame_state *Metagame, float Alpha)
{
#define MAX_SCORE_DIGITS 20
#define MAX_SCORE_SPACES (MAX_SCORE_DIGITS - 1)
    char String[MAX_SCORE_DIGITS + 1];
    // UINT64_MAX is a 20 digit decimal number, plus one for null byte
    u32 DigitsInScore = SetStringFromNumber(String, GameState->Score, MAX_SCORE_DIGITS + 1);
    float Margin = CalculateLightMargin(&GameState->ScoreLight);
    float GlyphWidth = 30.0f;
    // NOTE: This is small enough that we will not run out of screen 
    // space, if changing for some reason keep this in mind
    v2 GlyphDim = V2(GlyphWidth, Metagame->GlyphYOverX*GlyphWidth);
    float ActualWidth = (DigitsInScore + ((DigitsInScore - 1) * GlyphSpacingOverWidth)) * GlyphWidth;
    v2 MinPoint = V2(SCREEN_RIGHT - Margin - ActualWidth, Margin);
    PrintFromMinCornerAndGlyphDim(Renderer, Metagame, &GameState->ScoreLight, String, MinPoint, GlyphDim);
}

inline void 
SpawnShip(game_state *GameState)
{

    GameState->Ship.Position = ScreenCenter;
    GameState->Ship.Velocity = {0.0f, 0.0f};
    GameState->Ship.Heading = 1.5f * PI;
    GameState->Ship.Spin = 0.0f;
    GameState->ShipExists = true;
}
inline void
InitGameState(game_state *GameState, i32 LevelNumber, metagame_state *MetagameState)
{
    if (GameState->GameArena.Base) {
        ResetArena(&GameState->GameArena);
    } else {
        u64 GameArenaSize = Megabytes(1);
        void *GameMemory = PushSize(&MetagameState->PermanentArena, GameArenaSize);
        InitializeArena(&GameState->GameArena, GameArenaSize, GameMemory);
    }

    GameState->GameOverTimer = GAME_OVER_TIME;
    GameState->LevelComplete = false;
    GameState->Level.Number = LevelNumber;
    level *Level = &GameState->Level;
    if (LevelNumber == 1) {
        GameState->Score = 0;
        GameState->ExtraLives = 4;
    }

    if (LevelNumber > 0) {
        Level->StartingAsteroids = 1 + (2*(LevelNumber-1));
        Level->AsteroidMaxSpeed = 70.0f + (5.0f*(LevelNumber-1));
        Level->AsteroidMinSpeed = 20.0f + (2.0f*(LevelNumber-1));
        Level->SaucerSpawnTime = 20.0f * Pow(0.8f, (LevelNumber-1));
        Level->SaucerSpeed = 50.0f + (10.0f * (LevelNumber-1));
        Level->SaucerShootTime = 2.0f * Pow(0.9f, (LevelNumber-1));
        Level->SaucerCourseChangeTime = 3.0f * Pow(0.9f, (LevelNumber-1));
    } else {
        InvalidCodePath;
    }

    SpawnShip(GameState);

    ZeroArray(GameState->Asteroids, asteroid);
    GameState->AsteroidCount = 0;
    ZeroArray(GameState->Shots, particle);
    ZeroArray(GameState->Particles, particle);
    for(int AsteroidIndex = 0; AsteroidIndex < Level->StartingAsteroids; AsteroidIndex++) {
        asteroid *A = CreateAsteroid(MAX_ASTEROID_SIZE, GameState, &MetagameState->LightParams);
        A->O.Position = V2(RandFloatRange(SCREEN_LEFT, SCREEN_RIGHT), RandFloatRange(SCREEN_TOP, SCREEN_BOTTOM));
        A->O.Velocity = V2FromAngleAndMagnitude(
                RandHeading(), 
                RandFloatRange(Level->AsteroidMinSpeed, Level->AsteroidMaxSpeed));
        A->O.Heading = RandHeading();
        A->O.Spin = RandFloatRange(-INIT_ASTEROID_MAX_SPIN, INIT_ASTEROID_MAX_SPIN);


        v2 ToShip = GameState->Ship.Position - A->O.Position;
        float ToShipMagnitude = Length(ToShip);
        if (ToShipMagnitude < INIT_ASTEROID_MIN_DISTANCE) {
            v2 Offset = ToShip * -((INIT_ASTEROID_MIN_DISTANCE - ToShipMagnitude)/ToShipMagnitude);
            A->O.Position += Offset;
        }
    }

    GameState->SaucerExists = false;
    GameState->SaucerSpawnTimer = Level->SaucerSpawnTime;

    GameState->FirstFreeParticleBlock = 0;
    GameState->FirstFreeParticleGroup = 0;
    GameState->FirstParticleGroup = 0;
}
internal void
UpdateAndDrawGameplay(game_state *GameState, render_buffer *Renderer, game_input *Input, 
        metagame_state *Metagame, float SceneAlpha = 1.0f)
{
    BENCH_START_COUNTING_CYCLES_USECONDS(UpdateGame)
    float dTime = Input->SecondsElapsed;
    static float ShootCoolDown = 0.0f;

    if (!GameState->LevelComplete && GameState->AsteroidCount == 0 && !GameState->SaucerExists &&
            GameState->ShipExists)
    {
        GameState->Level.Number++;
        GameState->LevelComplete = true;
        StartMusicCrossfade(Metagame, &Metagame->Sounds.OneBarKick, 2.0f);
    }

    if (GameState->ShipExists) {
        GameState->Ship.Spin = 0;
        if (Input->Keyboard.MoveRight.IsDown) {
            GameState->Ship.Spin = SHIP_TURN_SPEED;
        }
        if (Input->Keyboard.MoveLeft.IsDown) {
            GameState->Ship.Spin = -SHIP_TURN_SPEED;
        }

        if (Input->Keyboard.MoveUp.IsDown) {
            v2 PrevVelocity = GameState->Ship.Velocity;
            v2 Accel = V2FromAngleAndMagnitude(GameState->Ship.Heading + (0.5f * GameState->Ship.Spin * dTime), 
                    SHIP_ABS_ACCEL);
            GameState->Ship.Velocity += (Accel * dTime);
            float ShipAbsVelocity = Length(GameState->Ship.Velocity);
            if (ShipAbsVelocity > MAX_ABS_SHIP_VEL) {
                GameState->Ship.Velocity *= (MAX_ABS_SHIP_VEL / ShipAbsVelocity);
            }
            v2 dVelocity = GameState->Ship.Velocity - PrevVelocity;
            v2 dPos = ((0.5f*dVelocity) + PrevVelocity) * dTime;
            v2 OldTailPos = MapFromObjectPosition(GameState->Ship.Vertices[2], GameState->Ship);
            MovePosition(&GameState->Ship.Position, dPos, GameState);
            GameState->Ship.Heading += GameState->Ship.Spin * dTime;
            // TODO: Use this to have an arc of particles?
            // v2 NewTailPos = MapFromObjectPosition(GameState->Ship.Vertices[2], GameState->Ship);
            float RocketParticlesPerSecond = 180.0f;
            i32 RocketParticles = i32(RocketParticlesPerSecond * dTime);
            for (i32 ParticleI = 0;
                 ParticleI < RocketParticles;
                 ++ParticleI)
            {
                particle *P = CreateParticle(GameState);
                if (P) {
                    v2 ParticlePositionSpread = OldTailPos - V2FromAngleAndMagnitude((GameState->Ship.Heading +
                                                                                    RandFloatRange(-1.2f, 1.2f)), 
                                                                                    8.0f);

                    P->Position = Lerp(OldTailPos, RandFloat01(), ParticlePositionSpread);
                    float PHeading = GameState->Ship.Heading + PI + RandFloatRange(-1.0f, 1.0f);
                    P->Velocity = V2FromAngleAndMagnitude(PHeading, 15.0f) + GameState->Ship.Velocity;
                    float C_L = 6.0f;
                    P->Light = {RandFloatRange(0.03f, 0.10f),
                                1.0f, 
                                1.0f, 
                                C_L};
                    P->C_LOriginal = C_L;
                    P->SecondsToLive = 0.8f;
                }
            }
        } else {
            // Free fall movement
            MoveObject(&GameState->Ship, dTime, GameState);
        }

        if (UpdateAndCheckTimer(&ShootCoolDown, dTime) && 
                Input->Keyboard.Fire.IsDown && !Input->Keyboard.Fire.WasDown) {
            ShipShoot(GameState, Metagame);
            ShootCoolDown = SHOT_COOLDOWN_TIME;
        }
    } else if (GameState->ExtraLives > 0 && UpdateAndCheckTimer(&GameState->ShipRespawnTimer, dTime)) {
        // Look for opportunity to respawn ship
        bool32 CenterIsClear = true;
        float MinimumDistanceSq = Square(AsteroidProps[2].Radius * 2.0f);
        for (u32 AIndex = 0; AIndex < GameState->AsteroidCount; AIndex++) {
            asteroid *A = GameState->Asteroids + AIndex;
            if (LengthSq(ScreenCenter - A->O.Position) < MinimumDistanceSq) {
                CenterIsClear = false;
            }
        }
        if (GameState->SaucerExists && LengthSq(ScreenCenter - GameState->Saucer.Position) < MinimumDistanceSq) {
            CenterIsClear = false;
        }
        if (CenterIsClear) {
            SpawnShip(GameState);
            --GameState->ExtraLives;
        }
    }

    if(GameState->SaucerExists) {
        if (UpdateAndCheckTimer(&GameState->SaucerCourseChangeTimer, dTime)) {
            SetSaucerCourse(GameState);
            GameState->SaucerCourseChangeTimer = GameState->Level.SaucerCourseChangeTime;
        }
        MoveObject(&GameState->Saucer, dTime, GameState);
        if (GameState->ShipExists && UpdateAndCheckTimer(&GameState->SaucerShootTimer, dTime)) {
            SaucerShootLeading(GameState, Metagame);
        }
    } else if (!GameState->LevelComplete) {
        if (UpdateAndCheckTimer(&GameState->SaucerSpawnTimer, dTime)) {
            SpawnSaucer(GameState, Metagame);
        }
    }

    MoveShots(dTime, GameState);
    MoveAsteroids(dTime, GameState);
    // COLLISIONS

    // Collide shots
    for (int SIndex = 0; SIndex < MAX_SHOTS; ++SIndex) {
        particle* ThisShot = &GameState->Shots[SIndex];
        if (ThisShot->Exists) {
            if (GameState->SaucerExists && Collide(ThisShot, &GameState->Saucer, GameState)) {
                ExplodeSaucer(GameState, Metagame);
                ThisShot->Exists = false;
                break;
            }
            if (GameState->ShipExists && Collide(ThisShot, &GameState->Ship, GameState)) {
                ExplodeShip(GameState, Metagame);
                ThisShot->Exists = false;
                break;
            }
        }
    }
    // Collide asteroids
    for (u32 AIndex = 0; AIndex < GameState->AsteroidCount; ++AIndex) {
        if (GameState->ShipExists && Collide(&GameState->Asteroids[AIndex].O, &GameState->Ship, GameState)) {
            ExplodeShip(GameState, Metagame);
        }
        if (GameState->SaucerExists && Collide(&GameState->Asteroids[AIndex].O, &GameState->Saucer, GameState)) {
            ExplodeSaucer(GameState, Metagame);
        }
        for (int SIndex = 0; SIndex < MAX_SHOTS; ++SIndex) {
            particle* ThisShot = &GameState->Shots[SIndex];
            if (ThisShot->Exists) {
                if (Collide(ThisShot, &GameState->Asteroids[AIndex].O, GameState)) {
                    ExplodeAsteroid(&GameState->Asteroids[AIndex], GameState, Metagame);
                    ThisShot->Exists = false;
                    break;
                }
            }
        }
    }
    // Collide ship with saucer
    if (GameState->SaucerExists && GameState->ShipExists && Collide(&GameState->Ship, &GameState->Saucer, GameState)) {
        ExplodeShip(GameState, Metagame);
        ExplodeSaucer(GameState, Metagame);
    }

    MoveParticles(dTime, GameState);
    BENCH_STOP_COUNTING_CYCLES_USECONDS(UpdateGame)

    BENCH_START_COUNTING_CYCLES_USECONDS(DrawGame)
    PushAsteroids(Renderer, GameState, SceneAlpha);
    for (u32 SIndex = 0; SIndex < (u32)ArrayCount(GameState->Shots); SIndex++) {
        particle *ThisParticle = &GameState->Shots[SIndex];
        if (ThisParticle->Exists) {
            PushParticle(Renderer, ThisParticle, SceneAlpha);
        }
    }
    for (u32 PIndex = 0; PIndex < (u32)ArrayCount(GameState->Particles); PIndex++) {
        particle* P = &GameState->Particles[PIndex];
        if (P->Exists) {
            PushParticle(Renderer, P, SceneAlpha);
        }
    }

    for (particle_group *Group = GameState->FirstParticleGroup;
         Group;
         Group = Group->NextGroup)
    {
        float C_L = Lerp(0.0f, Group->SecondsToLive / PARTICLE_LIFETIME, Group->C_LOriginal);
        PushParticleGroup(Renderer, Group, C_L, SceneAlpha);
    }
         

    if (GameState->SaucerExists) {
        float SaucerCharge = 1.0f - (GameState->SaucerShootTimer / GameState->Level.SaucerShootTime);
        float Threshold = Metagame->LightParams.SaucerHChangeThreshold;
        float HCharged = Metagame->LightParams.SaucerHCharged;
        float HBase = Metagame->LightParams.SaucerLightBase.H;
        if (SaucerCharge > Threshold) {
            GameState->Saucer.Light.H = Lerp(HBase, (SaucerCharge - Threshold)/(1.0f - Threshold), HCharged);
        } else {
            GameState->Saucer.Light.H = HBase;
        }
        GameState->Saucer.Light.C_L = Metagame->LightParams.SaucerLightBase.C_L +
                                      (Metagame->LightParams.SaucerC_LRange * SaucerCharge);

        PushObject(Renderer, &GameState->Saucer, SceneAlpha);
    }
    if (GameState->ShipExists) {
        Metagame->LightParams.ShipLightMin.C_L = 2.0f;
        Metagame->LightParams.ShipC_LRange = 6.0f;
        music_position_data Playhead = GetMusicPositionData(&Metagame->SoundState);
        float ShipC_LPulse = ((0.25f - EuclideanMod(Playhead.PositionInMeasure, 0.25f)) * 
                              Metagame->LightParams.ShipC_LRange);
        GameState->Ship.Light.C_L = Metagame->LightParams.ShipLightMin.C_L + ShipC_LPulse;
        PushObject(Renderer, &GameState->Ship, SceneAlpha);
    } else if (GameState->ExtraLives == 0) {
        light_source GameOverLight;
        GameOverLight.H = 0.0f;
        GameOverLight.S = 0.9f;
        GameOverLight.ZDistSq = 32.0f;
        GameState->GameOverTimer -= Input->SecondsElapsed;
        GameOverLight.C_L = Lerp(8.0f, GameState->GameOverTimer/GAME_OVER_TIME, 0.0f);
        if (GameOverLight.C_L > 8.0f) {
            GameOverLight.C_L = 8.0f;
        }
        PrintFromWidthAndCenter(Renderer, Metagame, &GameOverLight, "game over", 700.0, ScreenCenter);
    }

    PrintScore(Renderer, GameState, Metagame, SceneAlpha);
    if (GameState->ExtraLives > 0) {
        v2 ExtraLifeDim = V2(10, 12.5);
        v2 ExtraLifeSpacing = V2(5,10);
        v2 ExtraLifePosition = ExtraLifeSpacing + (ExtraLifeDim/2);
        for (u32 ExtraLifeIndex = 0; ExtraLifeIndex < GameState->ExtraLives; ++ExtraLifeIndex)
        {
            object *ExtraLife = PushStruct(&Metagame->TransientArena, object);
            ExtraLife->VerticesCount = 4;
            ExtraLife->Heading = 1.5f*PI;
            ExtraLife->Light = Metagame->LightParams.ShipLightMin;
            ExtraLife->Position = ExtraLifePosition;
            for (i32 VIndex = 0; VIndex < ExtraLife->VerticesCount; ++VIndex)
            {
                ExtraLife->Vertices[VIndex] = GameState->Ship.Vertices[VIndex] / 2;
            }
            PushObject(Renderer, ExtraLife);
            ExtraLifePosition.X += ExtraLifeSpacing.X + ExtraLifeDim.X;
        }
    }

    BENCH_STOP_COUNTING_CYCLES_USECONDS(DrawGame)

}