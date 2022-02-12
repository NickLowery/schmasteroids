#ifndef SCHM_PLATFORM_H


#define BYTES_PER_PIXEL 4
// NOTE: Pitch must be a multiple of 16. Currently to make SIMD drawing easier the width
// and height must be multiples of 4.
typedef struct {
    i32 Width;
    i32 Height;
    i32 Pitch;
    i32 MemorySize;
    u8 *Memory;
} platform_framebuffer;

// Sound 
typedef struct _platform_wav_load_info {
    size_t StartOfSamplesInFile;
    size_t SampleByteCount;
    u32 Channels;
} platform_wav_load_info;
//NOTE: Currently we assume sound output is always interleaved stereo, 16bit integer PCM,
// sampling rate 48000Hz
typedef struct _platform_sound_output_buffer {
    i32 AFramesPerSecond;
    i32 AFramesToWrite;
    i16 *SampleData;
} platform_sound_output_buffer;

#pragma pack(push,1)
typedef struct {
    char ckID[4];
    u32 ckSize;
} wav_chunk_header;

typedef struct {
    wav_chunk_header ChunkHeader;
    char WAVEID[4];
} wav_header;

typedef struct {
    wav_chunk_header ChunkHeader;
    u16 wFormatTag;
    u16 nChannels;
    u32 nSamplesPerSec;
    u32 nAvgBytesPerSec;
    u16 BlockAlign;
    u16 wBitsPerSample;
    u16 cbSize;
    u16 wValidBitsPerSample;
    u32 dwChannelMask;
    char SubFormat[16];
} wav_chunk;

#pragma pack(pop)

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
    platform_get_wav_load_info* PlatformGetWavLoadInfo;
    platform_load_wav* PlatformLoadWav;
    platform_debug_write_file* PlatformDebugWriteFile;
    platform_debug_read_file* PlatformDebugReadFile;
    platform_debug_save_framebuffer_as_bmp * PlatformDebugSaveFramebufferAsBMP;
    platform_quit* PlatformQuit;
    platform_toggle_fullscreen* PlatformToggleFullscreen;
} game_memory;

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
#define SCHM_PLATFORM_H 1
#endif
