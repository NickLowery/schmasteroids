// TODO: Linux binary distribution stuff!!! Reference https://davidgow.net/handmadepenguin/ch16.html
#include <SDL.h>
#include <schmasteroids_main.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#define DEFAULT_W 640
#define DEFAULT_H 480
#define GLOBAL_SAMPLES_PER_SECOND 48000
#define TARGETFPS 30

static bool32 GlobalRunning;
static platform_framebuffer GlobalBackbuffer;

typedef struct _output_data {
    SDL_Texture *Texture;
    platform_framebuffer FrameBuffer;
} output_data;

typedef struct _linux_game_code {
    void *SO;
    game_update_and_render *UpdateAndRender;
    game_initialize *Initialize;
    game_get_sound_output *GetSoundOutput;
    game_seed_prng *SeedRandom;
    bool32 IsValid;
} linux_game_code;

#define HANDLE_KEY_EVENT(KeyEvent, Button) \
    (Button).IsDown = ((KeyEvent).state == SDL_PRESSED);

void ErrorOut(i32 Code) {
    SDL_Quit();
    exit(-1);
}

internal void ResizeBackbuffer(SDL_Renderer *Renderer, output_data *OutputData, i32 Width, i32 Height)
{
    if (OutputData->Texture) {
        SDL_DestroyTexture(OutputData->Texture);
    }
    if (OutputData->FrameBuffer.Memory) {
        free(OutputData->FrameBuffer.Memory);
    }
    OutputData->Texture = SDL_CreateTexture(Renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                Width,
                                Height);
    i32 AdjustedWidth = RoundDownToMultipleOf(4,Width);
    i32 AdjustedHeight = RoundDownToMultipleOf(4,Height);
    OutputData->FrameBuffer.Width = AdjustedWidth; 
    OutputData->FrameBuffer.Height = AdjustedHeight;
    OutputData->FrameBuffer.Pitch = AdjustedWidth * 4;
    OutputData->FrameBuffer.MemorySize = 
        (OutputData->FrameBuffer.Pitch * AdjustedHeight);
    // TODO: Use mmap? Might not be Mac-compatible.
    // TODO: Error out if malloc fails?
    OutputData->FrameBuffer.Memory = (u8*)malloc(OutputData->FrameBuffer.MemorySize);
    OutputData->FrameBuffer.BytesPerPixel = 4;
}

internal void SDLDrawBackbufferToWindow(SDL_Window *Window, SDL_Renderer *Renderer, output_data *OutputData)
{
    // TODO: Use SDL_LockTexture? Supposedly faster. Just write directly to
    // what it returns?
    if (SDL_UpdateTexture(OutputData->Texture, 0, 
                      OutputData->FrameBuffer.Memory, OutputData->FrameBuffer.Pitch))
    {
        printf("Problem with updating texture");
        //TODO: Do something about this error
    }
    SDL_RenderCopy(Renderer, OutputData->Texture, 0, 0);
    SDL_RenderPresent(Renderer);
}

PLATFORM_LOAD_WAV(LinuxSDLLoadWav)
{
    FILE* File = fopen(Filename, "r");
    if (!File) 
    {
        printf("Couldn't open wav file: %s\n", Filename);
        ErrorOut(-1);
    }
    wav_chunk_header Header = {};
    i32 SeekResult = fseek(File, Info.StartOfSamplesInFile - sizeof(Header), SEEK_CUR);
    if (SeekResult) {
        printf("Couldn't read wav file: %s\n", Filename);
        ErrorOut(-1);
    }
    size_t ReadResult = fread(&Header, sizeof(Header), 1, File);
    if(ReadResult != 1) 
    {
        printf("Couldn't read wav file: %s\n", Filename);
        ErrorOut(-1);
    } 
    Assert(strncmp(Header.ckID, "data", 4) == 0);
    Assert(Header.ckSize == Info.SampleByteCount);
    ReadResult = fread(Memory, Info.SampleByteCount, 1, File);
    if(ReadResult != 1) 
    {
        printf("Couldn't read wav file: %s\n", Filename);
        ErrorOut(-1);
    } 
    fclose(File);

}
PLATFORM_GET_WAV_LOAD_INFO(LinuxSDLGetWavLoadInfo) 
{
    u32 PathLength = 128;
    char Dir[PathLength];
    getcwd(Dir, PathLength);
    printf("Looking in %s\n", Dir);
    platform_wav_load_info Result = {};
    FILE* File = fopen(Filename, "r");
    if (!File) 
    {
        printf("Couldn't open wav file: %s\n", Filename);
        ErrorOut(-1);
    }

    wav_header WavHeader = {};
    wav_chunk Chunk = {};
    size_t ReadResult = fread(&WavHeader, sizeof(WavHeader), 1, File);
    if(ReadResult != 1) 
    {
        printf("Couldn't read file: %s\n", Filename);
        ErrorOut(-1);
    } 
    Result.StartOfSamplesInFile += sizeof(WavHeader);
    Assert(strncmp(WavHeader.ChunkHeader.ckID, "RIFF", 4) == 0);
    Assert(strncmp(WavHeader.WAVEID, "WAVE", 4) == 0);

    //ReadFile(FileHandle, &Chunk, sizeof(Chunk.ChunkHeader), &BytesRead, 0);
    ReadResult = fread(&Chunk, sizeof(Chunk.ChunkHeader), 1, File);
    if(ReadResult != 1) 
    {
        printf("Couldn't read file: %s\n", Filename);
        ErrorOut(-1);
    } 
    Result.StartOfSamplesInFile += sizeof(Chunk.ChunkHeader);
    Assert(strncmp(Chunk.ChunkHeader.ckID, "fmt ", 4) == 0);
    //ReadFile(FileHandle, &Chunk.wFormatTag, Chunk.ChunkHeader.ckSize, &BytesRead, 0);
    ReadResult = fread(&Chunk.wFormatTag, Chunk.ChunkHeader.ckSize, 1, File);
    if(ReadResult != 1) 
    {
        printf("Couldn't read file: %s\n", Filename);
        ErrorOut(-1);
    } 
    Result.StartOfSamplesInFile += Chunk.ChunkHeader.ckSize;
    Assert(Chunk.wFormatTag == 1);
    Assert(Chunk.nChannels <= 2);
    Result.Channels = Chunk.nChannels;
    Assert(Chunk.nSamplesPerSec == GlobalSamplesPerSecond);
    Assert(Chunk.wBitsPerSample == 16);

    //ReadFile(FileHandle, &Chunk, sizeof(Chunk.ChunkHeader), &BytesRead, 0);
    ReadResult = fread(&Chunk, sizeof(Chunk.ChunkHeader), 1, File);
    if(ReadResult != 1) 
    {
        printf("Couldn't read file: %s\n", Filename);
        ErrorOut(-1);
    } 
    Result.StartOfSamplesInFile += sizeof(Chunk.ChunkHeader);
    while(strncmp(Chunk.ChunkHeader.ckID, "data", 4)) {
        u32 BytesToSkip = Chunk.ChunkHeader.ckSize;
        i32 SeekResult = fseek(File, BytesToSkip, SEEK_CUR);
        if (SeekResult) {
            printf("Couldn't read file: %s\n", Filename);
            ErrorOut(-1);
        }
        Result.StartOfSamplesInFile += BytesToSkip;

        //ReadFile(FileHandle, &Chunk, sizeof(Chunk.ChunkHeader), &BytesRead, 0);
        ReadResult = fread(&Chunk, sizeof(Chunk.ChunkHeader), 1, File);
        if(ReadResult != 1) 
        {
            printf("Couldn't read file: %s\n", Filename);
            ErrorOut(-1);
        } 
        Result.StartOfSamplesInFile += sizeof(Chunk.ChunkHeader);
    }
    Result.SampleByteCount = Chunk.ChunkHeader.ckSize;
    fclose(File);
    
    return Result;
}
PLATFORM_DEBUG_READ_FILE(LinuxSDLDebugReadFile)
{
    u32 BytesRead = 0;
    FILE* File = fopen(Filename, "r");
    if (!File) 
    {
        printf("Couldn't open file: %s\n", Filename);
        return 0;
    }
    else {
        struct stat FileStatus;
        i32 StatResult = stat(Filename, &FileStatus);
        if (!StatResult) {
            off_t FileSize = FileStatus.st_size;
            u32 FileSize32 = (u32)FileSize;
            Assert(FileSize32 == FileSize);
            Assert(ReadSize <= FileSize32);
            Assert(Memory);

            size_t ReadResult = fread(Memory, ReadSize, 1, File);
            if(ReadResult != 1) { 
                printf("Trouble reading file: %s\n", Filename);
            } else {
                BytesRead = ReadSize;
            }
        }
        fclose(File);
        return BytesRead;
    }
}
PLATFORM_DEBUG_WRITE_FILE(LinuxSDLDebugWriteFile)
{
    // TODO: Implement
}
PLATFORM_FREE_FILE_MEMORY(LinuxSDLFreeFileMemory) 
{
    if (Memory) {
        free(Memory);
    }
}
PLATFORM_DEBUG_SAVE_FRAMEBUFFER_AS_BMP(LinuxSDLDebugSaveFramebufferAsBMP)
{
    // TODO: Implement this
}
PLATFORM_QUIT(LinuxSDLQuit)
{
    GlobalRunning = false;
}

PLATFORM_TOGGLE_FULLSCREEN(LinuxSDLToggleFullScreen)
{
    // TODO: Implement this!
}


void * LoadGameFunction(void *SO, const char *FunctionName) {
    void *Result = dlsym(SO, FunctionName);
    char *DLErrorOutput = dlerror();
    if (DLErrorOutput || !Result) {
        printf("Something went wrong loading %s\n", FunctionName);
        if (DLErrorOutput) {
            printf("dlerror returned: %s\n", DLErrorOutput);
        }
        ErrorOut(-1);
        // TODO: Something else to handle the error?
    }
    return Result;
}

linux_game_code LoadGameCode() {
    linux_game_code Result = {};
    char SOPath[] = "build/schmasteroids.so";
    Result.SO = dlopen(SOPath, RTLD_LAZY);
    if (Result.SO) {
        printf("SO Loaded\n");
        dlerror();
        Result.GetSoundOutput = (game_get_sound_output*)
            LoadGameFunction(Result.SO, "GameGetSoundOutput");
        Result.Initialize = (game_initialize*)
            LoadGameFunction(Result.SO, "GameInitialize");
        Result.UpdateAndRender = (game_update_and_render*)
            LoadGameFunction(Result.SO, "GameUpdateAndRender");
        Result.SeedRandom = (game_seed_prng*)
            LoadGameFunction(Result.SO, "SeedRandom");
        Result.IsValid = true;
    } else {
        printf("load failed\n");
        char *DLErrorOutput = dlerror();
        printf("dlerror returned: %s\n", DLErrorOutput);
        ErrorOut(-1);
    }
    return Result;
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
        // TODO: Possibly use SDL_AudioInit() to control what driver we use?
    {
        //TODO: SDL_Init didn't work!
        return -1;
    }

    SDL_Window *MainWindow;
    MainWindow = SDL_CreateWindow("Schmasteroids",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  DEFAULT_W,
                                  DEFAULT_H,
                                  SDL_WINDOW_RESIZABLE);
    SDL_Renderer *Renderer = SDL_CreateRenderer(MainWindow, -1, 0);

    output_data OutputData = {};
    ResizeBackbuffer(Renderer, &OutputData, DEFAULT_W, DEFAULT_H);

    SDL_AudioSpec AudioSpec = {};
    AudioSpec.freq = GLOBAL_SAMPLES_PER_SECOND;
    AudioSpec.format = AUDIO_S16LSB;
    AudioSpec.channels = 2;
    AudioSpec.samples = 32768;
    if(SDL_OpenAudio(&AudioSpec, 0)) {
        printf("Error opening audio device\n");
        // TODO: Do something about this error
    }

    
    // Timing set-up
    u64 PerfFrequency = SDL_GetPerformanceFrequency();
    float TargetSecondsPerFrame = 1.0f / (float)TARGETFPS;

    // Load game code!
    char SOPath[] = "./schmasteroids_main.so";
    linux_game_code Game = LoadGameCode();

    // Move into data directory
    if (chdir("data")) {
        printf("Couldn't change directory to data\n");
        ErrorOut(-1);
    }

    // Memory
#if DEBUG_BUILD
    void* GameMemoryBase = (void*)Terabytes(2);
#else
    void* GameMemoryBase = 0;
#endif
    game_memory GameMemory = {};
    GameMemory.PermanentStorageSize = PERMANENT_STORE_SIZE;
    GameMemory.TransientStorageSize = TRANSIENT_STORE_SIZE;
    size_t CombinedMemorySize = (size_t)(GameMemory.PermanentStorageSize +
                                GameMemory.TransientStorageSize);
    // TODO: Mac compatible?
    GameMemory.PermanentStorage = mmap(GameMemoryBase, CombinedMemorySize,
                                       PROT_READ | PROT_WRITE,
                                       MAP_ANON | MAP_PRIVATE,
                                       -1, 0);
    Assert(GameMemory.PermanentStorage);
    GameMemory.TransientStorage = (u8*)GameMemory.PermanentStorage + 
                                  GameMemory.PermanentStorageSize;

    // Create pointers to platform services
    GameMemory.PlatformLoadWav = &LinuxSDLLoadWav;
    GameMemory.PlatformGetWavLoadInfo = &LinuxSDLGetWavLoadInfo;
    GameMemory.PlatformFreeFileMemory = &LinuxSDLFreeFileMemory;
    GameMemory.PlatformDebugReadFile = &LinuxSDLDebugReadFile;
    GameMemory.PlatformDebugWriteFile = &LinuxSDLDebugWriteFile;
    GameMemory.PlatformDebugSaveFramebufferAsBMP = &LinuxSDLDebugSaveFramebufferAsBMP;
    GameMemory.PlatformQuit = &LinuxSDLQuit;
    GameMemory.PlatformToggleFullscreen = &LinuxSDLToggleFullScreen;

    if (Game.IsValid) {
        // Seed PRNG
        u64 RandomSeedCounter = SDL_GetPerformanceCounter();
        u64 U32Mask = (u64)UINT32_MAX;
        u32 RandomSeed = (u32)(RandomSeedCounter & U32Mask);
        Game.SeedRandom(RandomSeed);
        //Initialize game
        Game.Initialize(&OutputData.FrameBuffer, &GameMemory);
    }

    // Initialize input
    game_input GameInputs[2];
    for (int i=0;i<2;i++) {
        GameInputs[i] = {};
    }
    game_input *ThisInput = &GameInputs[0];
    game_input *LastInput = &GameInputs[1];
    Assert(&(ThisInput->Keyboard.Terminator) == &(ThisInput->Keyboard.Keys[ArrayCount(ThisInput->Keyboard.Keys)-1]));
#if DEBUG_BUILD
    Assert(&(ThisInput->DebugKeys.Terminator) == &(ThisInput->DebugKeys.Keys[ArrayCount(ThisInput->DebugKeys.Keys)-1]));
#endif

    GlobalRunning = true;
    while(GlobalRunning) {
        // TODO: IMPORTANT! Add hotloading of SO
        
        for(u32 KeyIndex = 0; KeyIndex < (u32)ArrayCount(ThisInput->Keyboard.Keys); KeyIndex++) {
            ThisInput->Keyboard.Keys[KeyIndex].IsDown = 
                ThisInput->Keyboard.Keys[KeyIndex].WasDown =
                LastInput->Keyboard.Keys[KeyIndex].IsDown;
        }

        SDL_Event Event;
        while(SDL_PollEvent(&Event)) {
            switch(Event.type) {
                case SDL_QUIT: {
                printf("SDL_QUIT\n");
                GlobalRunning = false;
                } break;
                case SDL_WINDOWEVENT: {
                    switch(Event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED: {
                        printf("SDL_WINDOWEVENT_RESIZED (%d x %d)\n", 
                                Event.window.data1,
                                Event.window.data2);
                        i32 Width, Height;
                        SDL_GetWindowSize(MainWindow, &Width, &Height);
                        ResizeBackbuffer(Renderer, &OutputData, Width, Height);
                        } break;
                        case SDL_WINDOWEVENT_EXPOSED: {
                        // TODO: Get window ID from the event in case we ever 
                        // have more than one window? Doesn't seem likely 
                        // that we would want to.
                        SDLDrawBackbufferToWindow(MainWindow, Renderer, &OutputData);
                        } break;
                    }
                } break;
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    // TODO: Debug keys?
                    SDL_Keycode KeyCode = Event.key.keysym.sym;
                    switch(KeyCode) {
                        case SDLK_UP: {
                            HANDLE_KEY_EVENT(Event.key, ThisInput->Keyboard.MoveUp);
                        } break;
                        case SDLK_DOWN: {
                            HANDLE_KEY_EVENT(Event.key, ThisInput->Keyboard.MoveDown);
                        } break;
                        case SDLK_LEFT: {
                            HANDLE_KEY_EVENT(Event.key, ThisInput->Keyboard.MoveLeft);
                        } break;
                        case SDLK_RIGHT: {
                            HANDLE_KEY_EVENT(Event.key, ThisInput->Keyboard.MoveRight);
                        } break;
                        case SDLK_SPACE: {
                            HANDLE_KEY_EVENT(Event.key, ThisInput->Keyboard.Fire);
                        } break;
                        case SDLK_LSHIFT: 
                        case SDLK_RSHIFT: {
                            HANDLE_KEY_EVENT(Event.key, ThisInput->Keyboard.Shift);
                        } break;
                    }
                } break;
            }
        }
        ThisInput->SecondsElapsed = TargetSecondsPerFrame;

        u64 PreUpdateCounter = SDL_GetPerformanceCounter();
        u64 PreUpdateCycles = _rdtsc();

        if (Game.IsValid) {
            printf("Updating game\n");
            Game.UpdateAndRender(&OutputData.FrameBuffer, ThisInput, &GameMemory);
        }

        game_input* tempInput = ThisInput;
        ThisInput = LastInput;
        LastInput = tempInput;

        SDLDrawBackbufferToWindow(MainWindow, Renderer, &OutputData);
        u64 PostUpdateCycles = _rdtsc();
        printf("Cycles in update: %lu\n", PostUpdateCycles - PreUpdateCycles);
        u64 LoopEndCounter = SDL_GetPerformanceCounter();
        // TODO: Is it better to do this in u64?
        float SecondsInUpdate = (float)(LoopEndCounter - PreUpdateCounter) /
                                (float)PerfFrequency;
        printf("Seconds in update: %f\n", SecondsInUpdate);
        if (SecondsInUpdate < TargetSecondsPerFrame) {
            // TODO: Sleep
        }
        // TODO: Use SDL_QueueAudio to queue some audio!
    }


    SDL_Quit();
    return 0; 
}
