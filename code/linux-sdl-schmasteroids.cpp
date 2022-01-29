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
    SDL_Renderer *Renderer = SDL_CreateRenderer(Window, -1, 0);

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
                }
            } break;
        }
    }


    SDL_Quit();
    return 0; 
}
