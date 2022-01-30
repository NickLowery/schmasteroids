#include <SDL.h>
#include <schmasteroids_main.h>
#define DEFAULT_W 640
#define DEFAULT_H 480

typedef struct _output_data {
    SDL_Texture *Texture;
    void *Pixels;
    i32 Pitch;
} output_data;

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
    // TODO: Use mmap? Might not be Mac-compatible
    OutputData->Pixels = malloc(Width * Height * 4);
    OutputData->Pitch = Width * 4;
}


internal void SDLDrawBackbufferToWindow(SDL_Window *Window, SDL_Renderer *Renderer, output_data *OutputData)
{
    // TODO: Use SDL_LockTexture? Supposedly faster.
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
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
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

    // TODO: Sound!
    // TODO: Clocks!
    // TODO: Memory!
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
        for(int KeyIndex = 0; KeyIndex < ArrayCount(ThisInput->Keyboard.Keys); KeyIndex++) {
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
        SDLDrawBackbufferToWindow(MainWindow, Renderer, &OutputData);
    }


    SDL_Quit();
    return 0; 
}
