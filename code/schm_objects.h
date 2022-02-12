#ifndef SCHM_OBJECTS_H

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

// NOTE: This is only for ungrouped particles, which currently means only the ship's exhaust, so 500 seems 
// to be plenty
#define MAX_PARTICLES 500
#define PARTICLE_LIFETIME 1.5f
#define PARTICLE_VELOCITY 60.0f

#define ASTEROID_COLOR 0xFF998800
#define SHIP_COLOR 0xFFFFFFFF
#define SAUCER_COLOR 0xFFDB17E6
#define SHOT_COLOR 0xFF00FF00

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

#define SCHM_OBJECTS_H
#endif
