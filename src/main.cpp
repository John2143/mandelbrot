#include "global.h"
#include "graphics.h"
#include "renderScene.h"
#include "input.h"

int main(int argc, char *argv[]){
    (void) argc;
    (void) argv;

    graphics G;

    renderScene s(G);
    s.set.pctscale = 1;
    s.set.square = true;

    s.needsFullRender = true;

    SDL_Event event;
    event.type = -1;
    for(;;) {
        int inputType = manageInput(event, s);
        if(inputType == INPUT_QUIT) break;

        if(s.needsFullRender && !s.isRendering){
            s.isRendering = true;
            s.needsFullRender = false;

            s.renderThread = std::thread([&]{
                printf("Thread started \n");
                s.draw();
                printf("Thread  drawn\n");
                s.v.singlePrecise = false;
                s.needsBlit = true;
            });

            s.renderThread.detach();
        }
        if(s.needsBlit){
            s.blit();
            s.needsBlit = false;
        }

        fflush(stdout);

        if(s.isRendering){
            int hasEv = SDL_PollEvent(&event);
            if(!hasEv){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                event.type = -1;
            }
        }else{
            SDL_WaitEvent(&event);
        }
    }

    return 0;
}
