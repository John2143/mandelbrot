#pragma once
#include "global.h"

enum {
    INPUT_OK = 0,
    INPUT_QUIT = 1,
};

int manageInput(SDL_Event &event, renderScene &s){
    graphics &G = s.G;
    bool isRendering = s.isRendering;

    switch(event.type){
    case -1:
        return INPUT_OK;
    case SDL_MOUSEBUTTONDOWN:
        if(event.button.button == SDL_BUTTON_LEFT){
            if(isRendering) return INPUT_OK;
            printf("clicked %i %i\n", event.button.x, event.button.y);
            s.v.x = lerp(event.button.x, G.realw, s.v.x - s.v.size/2, s.v.x + s.v.size / 2);
            s.v.y = lerp(event.button.y, G.realh, s.v.y - s.v.size * G.aspectRatio / 2, s.v.y + s.v.size * G.aspectRatio / 2);
            s.v.size /= 2;
            s.v.zooms++;
            s.needsFullRender = true;
            return INPUT_OK;
        }else if(event.button.button == SDL_BUTTON_RIGHT){
            if(isRendering) return INPUT_OK;
            if(s.v.zooms > 0){
                s.v.size *= 2;
                s.v.zooms--;
                s.needsFullRender = true;
            }
            return INPUT_OK;
        }
    break;
    case SDL_KEYDOWN:
        if(event.key.keysym.sym == SDLK_f){
            if(isRendering) return INPUT_OK;
            s.v.preciseMode = !s.v.preciseMode;
            printf("Precise mode %s\n", s.v.preciseMode ? "yes" : "no");
        }else if(event.key.keysym.sym == SDLK_r){
            if(isRendering) return INPUT_OK;
            s.v.singlePrecise = true;
            s.needsFullRender = true;
            printf("Claculating a single frame\n");
        }else if(event.key.keysym.sym == SDLK_z){
            if(isRendering) return INPUT_OK;
            s.renderToFile();
            printf("Rendering to file\n");
        }else if(event.key.keysym.sym == SDLK_e){
            if(isRendering) return INPUT_OK;
            s.needsBlit = true;
            printf("Rendering a single frame\n");
        }else if(event.key.keysym.sym == SDLK_RIGHT){
            if(isRendering) return INPUT_OK;
            if((size_t) s.set.supersamples < MAX_SUPERSAMPLES) s.set.supersamples++;
            printf("Supersamples is %i\n", s.set.supersamples);
        }else if(event.key.keysym.sym == SDLK_LEFT){
            if(isRendering) return INPUT_OK;
            if(s.set.supersamples > 1) s.set.supersamples--;
            printf("Supersamples is %i\n", s.set.supersamples);
        }else if(event.key.keysym.sym == SDLK_UP){
            if(isRendering) return INPUT_OK;
            if(s.set.pctscale < 1) s.set.pctscale += .025;
            s.needsBlit = true;
            printf("Gamma scaling is %f\n", s.set.pctscale);
        }else if(event.key.keysym.sym == SDLK_DOWN){
            if(isRendering) return INPUT_OK;
            if(s.set.pctscale > 0) s.set.pctscale -= .025;
            s.needsBlit = true;
            printf("Gamma scaling is %f\n", s.set.pctscale);
        }else if(event.key.keysym.sym == SDLK_s){
            if(isRendering) return INPUT_OK;
            s.set.square = !s.set.square;
            printf("Square mode %s\n", BOOL(s.set.square));
        }else if(event.key.keysym.sym == SDLK_b){
            if(isRendering) return INPUT_OK;
            G.fullscreen = !G.fullscreen;
            printf("fullscreen mode %s\n", BOOL(G.fullscreen));
            SDL_SetWindowFullscreen(G.window, G.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }else if(event.key.keysym.sym == SDLK_p){
            if(isRendering) return INPUT_OK;
            s.set.viewProgress = !s.set.viewProgress;
            printf("view progress mode %s\n", BOOL(s.set.viewProgress));
        }else if(event.key.keysym.sym == SDLK_q){
            return INPUT_QUIT;
        }
        return INPUT_OK;
    break;
    case SDL_WINDOWEVENT:
        switch(event.window.event){
        //case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            if(isRendering) return INPUT_OK;
            if(s.G.resize(event.window.data1, event.window.data2)){
                s.removeData();
            }
        break;
        }
    break;
    case SDL_QUIT: return INPUT_QUIT;
    }
    return INPUT_OK;
}
