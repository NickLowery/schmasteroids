#ifndef SCHM_GAME_H

// Game constants
#define MAX_ABS_SHIP_VEL 180.0f
#define SHIP_ABS_ACCEL 240.0f
#define SHIP_TURN_SPEED 3.3f
#define SHIP_PARTICLES 100
#define SHIP_COLLISION_RADIUS 15.0f

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

#define SHOT_ABS_VEL 250.0f
#define SHOT_COOLDOWN_TIME 0.15f
#define SHOT_LIFETIME 1.0f

#define GAME_OVER_TIME 1.5f

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


inline v2 
ShortestPathWarped(v2 From, v2 To)
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

inline float
GetDistanceWarped(v2 FirstV, v2 SecondV) 
{
    return Length(ShortestPathWarped(FirstV, SecondV));
}


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

typedef struct {
    float H;
    float S;
    float C_L;
    float ZDistSq;
} light_source;

typedef struct {
    light_source ShipLightMin;
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

#define MAX_VERTICES 8
typedef struct {
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
    GameMode_EndScreen,
};

typedef struct {
    u32 Number;
    i32 StartingAsteroids;
    float AsteroidMaxSpeed;
    float AsteroidMinSpeed;
    float SaucerSpawnTime;
    float SaucerSpeed;
    float SaucerShootTime;
    float SaucerCourseChangeTime;
} level;

constexpr inline u32 
CalculateStartingAsteroids(u32 LevelNumber)
{
    u32 Result = 1 + (2*(LevelNumber-1));
    return Result;
}

constexpr inline float 
CalculateSaucerShootTime(u32 LevelNumber)
{
    float Result = 2.0f * Pow(0.9f, (LevelNumber-1));
    return Result; 
}

constexpr inline u32
CalculateMaxShotsOnScreen(u32 LevelNumber)
{
    u32 Result = Ceil(SHOT_LIFETIME / SHOT_COOLDOWN_TIME) + 
                 Ceil(SHOT_LIFETIME / CalculateSaucerShootTime(LevelNumber));
    return Result;
}

#define MAX_LEVEL 13 
#define MAX_ASTEROIDS ((CalculateStartingAsteroids(MAX_LEVEL) * 6) + 1) 
#define MAX_SHOTS (CalculateMaxShotsOnScreen(MAX_LEVEL))
// Maximum number of asteroids that could appear,  plus one for the temp that exists during  ExplodeAsteroid

typedef struct {
    xoshiro128pp_state RandState;
    //PLAYING
    level Level;
    u64 Score;
    u32 ExtraLives;
    float GameOverTimer;
    object Ship;
    bool32 ShipExists;
    object Saucer;
    bool32 SaucerExists;
    float SaucerSpawnTimer;
    float SaucerCourseChangeTimer;
    float SaucerShootTimer;
    float ShipRespawnTimer;
    bool32 LevelComplete;

    asteroid Asteroids[MAX_ASTEROIDS];
    u32 AsteroidCount;
    particle Shots[MAX_SHOTS];
    u32 ShotCount;
    particle Particles[MAX_PARTICLES];
    u32 ParticleCount;
    particle_block *FirstFreeParticleBlock;
    particle_group *FirstParticleGroup;
    particle_group *FirstFreeParticleGroup;
    memory_arena GameArena;
} game_state;

inline bool32
UpdateAndCheckTimer(float *Timer, float SecondsElapsed);

internal asteroid* 
CreateAsteroid(int Size, game_state *GameState, light_params *LightParams);

internal void MoveObject(object *O, 
        float SecondsElapsed, game_state *GameState);

internal void
RenderTestGradient(platform_framebuffer *Backbuffer, game_input *Input);

internal void 
DrawRadarLine(float SecondsElapsed, 
        platform_framebuffer *Backbuffer, game_input *KeyboardInput);

inline particle*
CreateShot(game_state *GameState);

internal glyph
ProjectGlyphIntoRect(glyph *Original, rect Rect);

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

internal void WarpPositionToScreen(v2 *Pos);

inline v2
MapFromObjectPosition(v2 V, object O)
{
    v2 Result = Rotate(V, O.Heading);
    Result += O.Position;
    return Result;
}

internal v2 MapWindowSpaceToGameSpace(v2 WindowCoords, platform_framebuffer *Backbuffer);

internal v2 MapGameSpaceToWindowSpace(v2 In, platform_framebuffer *Backbuffer);


#define SCHM_GAME_H 1
#endif
