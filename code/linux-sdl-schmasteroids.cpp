#include <SDL.h>
#include <schmasteroids_main.h>
#define DEFAULT_W 640
#define DEFAULT_H 480
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

    bool32 ShouldQuit = false;
    while(!ShouldQuit) {
        SDL_Event Event;
        SDL_WaitEvent(&Event);
        switch(Event.type) {
            case SDL_QUIT: {
                printf("SDL_QUIT\n");
                ShouldQuit = true;
            } break;
            case SDL_WINDOWEVENT: {
                switch(Event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED: {
                        printf("SDL_WINDOWEVENT_RESIZED (%d x %d)", 
                               Event.window.data1,
                               Event.window.data2);
                    } break;
                    case SDL_WINDOWEVENT_EXPOSED: {
                        // TODO: Get window ID from the event in case we ever have more
                        // than one window? Doesn't seem likely that we would want to
                        static bool IsWhite = true;
                        if (IsWhite == true)
                        {
                            SDL_SetRenderDrawColor(Renderer, 255, 255, 255, 255);
                            IsWhite = false;
                        }
                        else
                        {
                            SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
                            IsWhite = true;
                        }
                        SDL_RenderClear(Renderer);
                        SDL_RenderPresent(Renderer);
                    } break;
                }
            } break;
        }
    }


    SDL_Quit();
    return 0; 
}
