#pragma once
#include "global.h"

struct graphics{
    SDL_Window *window;
    int realw, realh;
    int scaleFactor;
    int width, height;
    floating aspectRatio;
    bool fullscreen = false;

    bool initialized;

    SDL_Texture *texture, *texturePrecise;
    SDL_Renderer *renderer;

    graphics(){
        initialized = false;
        SDL_Init(SDL_INIT_VIDEO);

        scaleFactor = 4;

        window = SDL_CreateWindow(
            "mandelbrot", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            20, 20, SDL_WINDOW_RESIZABLE
        );

        renderer = SDL_CreateRenderer(window, -1, 0);

        resize(1200, 800);

        initialized = true;
    }

    ~graphics(){
        SDL_DestroyTexture(texture);
        SDL_DestroyTexture(texturePrecise);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool resize(int w, int h){
        if(w == realw && h == realh){
            printf("tried resize %i %i\n", w, h);
            return true;
        }

        SDL_SetWindowSize(window, w, h);
        realw = w; realh = h;
        width = realw / scaleFactor;
        height = realh / scaleFactor;

        if(initialized){
            printf("remade\n");
            SDL_DestroyTexture(texture);
            SDL_DestroyTexture(texturePrecise);
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
        texturePrecise = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, realw, realh);

        aspectRatio = realh/(double) realw;
        printf("Resized to %i %i, aspect %f\n", w, h, aspectRatio);
        return false;
    }

};
