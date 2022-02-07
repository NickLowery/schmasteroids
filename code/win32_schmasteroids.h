#define TARGETFPS 30

#define DEFAULT_BACKBUFFERW 1200 
#define DEFAULT_BACKBUFFERH 900

#include <limits.h>
// Knowledge about lengths of int types
#if SHRT_MAX == 32767
#define BITS_IN_SHORT 16
#else
Assert("We don't know how many bits a short is (for parsing Windows messages)");
#endif

typedef struct {
    HMODULE GameDLL;
    game_update_and_render *UpdateAndRender;
    game_initialize *Initialize;
    game_get_sound_output *GetSoundOutput;
    game_seed_prng *SeedRandom;
    bool32 IsValid;
} win32_game_code;

typedef struct {
    // NOTE: MAX_PATH should not be in user-facing code
    char EXEFilename[MAX_PATH];
    char SourceGameDLLFullPath[MAX_PATH];
    char TempGameDLLFullPath[MAX_PATH];
    char GameCodeLockFile[MAX_PATH];
    FILETIME LastSourceDLLWriteTime;
} win32_exe_info;

typedef struct {
    game_memory GameMemory;

    char BuildDirPath[MAX_PATH];
    u16 BuildDirPathLen;

    u8 GamePlaybackIndex;
    u8 GameRecordIndex;
    HANDLE GameRecordHandle;
    HANDLE GamePlaybackHandle;

    i32 GameTop;
    i32 GameLeft;
    platform_framebuffer GameBuffer;
    BITMAPINFO GameBufferBitmapInfo;
} win32_state;
