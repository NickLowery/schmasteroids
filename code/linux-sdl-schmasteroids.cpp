#include <SDL.h>
#include <schmasteroids_main.h>
#include <sys/mman.h>
#define DEFAULT_W 640
#define DEFAULT_H 480
#define GLOBAL_SAMPLES_PER_SECOND 48000
#define TARGETFPS 30


typedef struct _output_data {
    SDL_Texture *Texture;
    void *Pixels;
    i32 Pitch;
} output_data;

typedef struct _linux_game_code {

} linux_game_code;

#define HANDLE_KEY_EVENT(KeyEvent, Button) \
    (Button).IsDown = ((KeyEvent).state == SDL_PRESSED);

internal void ResizeBackbuffer(SDL_Renderer *Renderer, output_data *OutputData, i32 Width, i32 Height)
{
    if (OutputData->Texture) {
        SDL_DestroyTexture(OutputData->Texture);
    }
    if (OutputData->Pixels) {
        free(OutputData->Pixels);
    }
    OutputData->Texture = SDL_CreateTexture(Renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                Width,
                                Height);
    // TODO: Use mmap? Might not be Mac-compatible.
    OutputData->Pixels = malloc(Width * Height * 4);
    OutputData->Pitch = Width * 4;
}

internal void SDLDrawBackbufferToWindow(SDL_Window *Window, SDL_Renderer *Renderer, output_data *OutputData)
{
    // TODO: Use SDL_LockTexture? Supposedly faster. Just write directly to
    // what it returns?
    if (SDL_UpdateTexture(OutputData->Texture, 0, 
                      OutputData->Pixels, OutputData->Pitch))
    {
        //TODO: Do something about this error
    }
    SDL_RenderCopy(Renderer, OutputData->Texture, 0, 0);
    SDL_RenderPresent(Renderer);
}

int main(int argc, char *argv[])
{
    //SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "SDL Schmasteroids", "TEST MESSAGE", 0);
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
    //u64 LastPreUpdateCounter = SDL_GetPerformanceCounter();

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

    // TODO: Load game code!

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

    bool32 ShouldQuit = false;
    while(!ShouldQuit) {
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
                ShouldQuit = true;
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
        u64 PreUpdateCounter = SDL_GetPerformanceCounter();
        u64 PreUpdateCycles = _rdtsc();
        // TODO: Update the game when we have game code!!!
        SDLDrawBackbufferToWindow(MainWindow, Renderer, &OutputData);
        u64 PostUpdateCycles = _rdtsc();
        printf("Cycles drawing backbuffer: %lu\n", PostUpdateCycles - PreUpdateCycles);
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
