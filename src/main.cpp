#include <cstdio>
#include <cmath>
#include <SDL2/SDL.h>
#include <cstring>

typedef double floating;

static int zooms = 0;

struct complex{
    floating real, imag;

    complex() : real(0), imag(0){}
    complex(floating re, floating im) : real(re), imag(im){}
    ~complex(){}

    void print() const{
        printf("(%f + %fi)", (float) real, (float) imag);
    }

    bool checkCardoid() const{
        floating a1 = real - 1/4;
        floating p = sqrt(a1 * a1 + imag * imag);
        if(real < p - 2 * p * p + 1/4){
            if((real + 1) * (real + 1) + imag * imag < 1/16){
                return true;
            }
        }
        return false;
    }

    #define MAX_ITERZ(preciseMode, zooms) (preciseMode ? (100 + zooms * 100) : (50 + zooms * 60))
    #define MAX_ITER(preciseMode) MAX_ITERZ(preciseMode, zooms)
    const floating DIVERGENCE_BOUNDARY = 2;
    double mandelbrotDiverge(bool preciseMode) const{
        if(checkCardoid()) return MAX_ITER(preciseMode);
        floating curr = 0;
        floating curi = 0;
        floating r2 = 0;
        floating i2 = 0;

        floating lastr = 0;
        floating lasti = 0;
        for(int iter = 0; iter < MAX_ITER(preciseMode); iter++){
            curi = 2 * curi * curr + imag;
            curr = r2 - i2 + real;
            r2 = curr * curr;
            i2 = curi * curi;
            if(r2 == lastr && i2 == lasti) break;

            lastr = r2;
            lasti = i2;

            if(r2 + i2 > DIVERGENCE_BOUNDARY * DIVERGENCE_BOUNDARY){
                return 1 + iter - log(log(r2 + i2)) - log(4);
            }
        }
        return MAX_ITER(preciseMode);
    }
};

struct graphics{
    SDL_Window *window;
    int realw, realh;
    int scaleFactor;
    int width, height;
    floating aspectRatio;

    bool initialized;

    SDL_Texture *texture, *texturePrecise;
    SDL_Renderer *renderer;

    graphics(){
        initialized = false;
        SDL_Init(SDL_INIT_VIDEO);

        scaleFactor = 4;
        realw = 1200;
        realh = 800;

        window = SDL_CreateWindow(
            "mandelbrot", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            realw, realh, 0
        );

        renderer = SDL_CreateRenderer(window, -1, 0);

        resize(realw, realh);

        initialized = true;
    }

    ~graphics(){
        SDL_DestroyTexture(texture);
        SDL_DestroyTexture(texturePrecise);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void resize(int w, int h){
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

    }

};

floating lerp(floating num, int max, floating start, floating end){
    return (num / (floating) max) * (end - start) + start;
}

// 183
// 657
// 492

#define MAX_SUPERSAMPLES 100
static floating supersampleMapX[MAX_SUPERSAMPLES] = {.17, .84, .17, .84, .50, .50, .50, .17, .84};
static floating supersampleMapY[MAX_SUPERSAMPLES] = {.17, .84, .84, .17, .50, .17, .84, .50, .50};

static int supersamples = 1;

struct renderScene{
    const graphics &G;
    bool preciseMode;
    uint32_t *pixels;
    floating *data;
    floating xpos, ypos, xsize;

    floating endx, endy, startx, starty;
    int width, height;

    renderScene(const graphics &G, floating x, floating y, floating size):
        G(G), xpos(x), ypos(y), xsize(size){

        pixels = nullptr;
        data = nullptr;
    }

    ~renderScene(){
        delete []data;
        delete []pixels;
    }

    void draw(bool precise){
        printf("We are at (%f + %fi) with size %f (zooms %i)\n", (float) xpos, (float) ypos, (float) xsize, zooms);

        preciseMode = precise;

        xsize /= 2;
        endx = xpos + xsize;
        endy = ypos + xsize * G.aspectRatio;
        startx = xpos - xsize;
        starty = ypos - xsize * G.aspectRatio;
        xsize *= 2;

        width  = preciseMode ? G.realw : G.width;
        height = preciseMode ? G.realh : G.height;

        data = new floating[width * height];

        for(int y = 0; y < height; y++){
            drawline(y, data + width * y);
        }

        render();
    }

    void drawline(int y, floating *memory){
        for(int x = 0; x < width; x++){
            floating brightnessTotal = 0;
            for(int sam = 0; sam < supersamples; sam++){
                complex num(
                    lerp(x + supersampleMapX[sam], width,  startx, endx),
                    lerp(y + supersampleMapY[sam], height, starty, endy)
                );
                brightnessTotal += num.mandelbrotDiverge(preciseMode);
            }
            brightnessTotal /= supersamples;
            *memory++ = brightnessTotal;
        }
    }

    void render(){
        delete []pixels;
        pixels = new uint32_t[width * height];
        for(int i = 0; i < width * height; i++){
            data[i] = data[i] / (MAX_ITER(preciseMode) + 1);
            data[i] *= data[i];
            data[i] *= data[i];
            uint32_t value = data[i] * 0xff;
            value = 0xFF000000 + value + (value << 8) + (value << 16);
            pixels[i] = value;
        }
    }
};


int main(int argc, char *argv[]){
    (void) argc; (void) argv;

    for(int i = 9; i < MAX_SUPERSAMPLES; i++){
        supersampleMapX[i] = rand() / (floating) RAND_MAX;
        supersampleMapY[i] = rand() / (floating) RAND_MAX;
    }

    graphics G;

    //floating x = 0;
    //floating y = 0;
    //floating size = 3;
    //zooms = 0;
    //
    floating x = -.748930;
    floating y = -.056503;
    floating size = .001465;
    zooms = 13;
    //
    //floating x = -.743421;
    //floating y = -.178880;
    //floating size = .00000001;
    //zooms = 29;

    bool preciseMode = false;
    bool singlePrecise = false;
    bool isRendered = false;

    SDL_Event event;
    event.type = -1;
    for(;;) {
        switch(event.type){
        case SDL_MOUSEBUTTONDOWN:
            if(event.button.button == SDL_BUTTON_LEFT){
                printf("clicked %i %i\n", event.button.x, event.button.y);
                x = lerp(event.button.x, G.realw, x - size/2, x + size / 2);
                y = lerp(event.button.y, G.realh, y - size * G.aspectRatio / 2, y + size * G.aspectRatio / 2);
                size /= 2;
                isRendered = false;
                zooms++;
            }else if(event.button.button == SDL_BUTTON_RIGHT){
                if(zooms > 0){
                    size *= 2;
                    isRendered = false;
                    zooms--;
                }
            }
        break;
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_f){
                preciseMode = !preciseMode;
                printf("Precise mode %s\n", preciseMode ? "yes" : "no");
            }else if(event.key.keysym.sym == SDLK_r){
                singlePrecise = true;
                isRendered = false;
                printf("Rendering a single frame\n");
            }else if(event.key.keysym.sym == SDLK_RIGHT){
                if((size_t) supersamples < sizeof(supersampleMapX) / sizeof(supersampleMapX[0])) supersamples++;
                printf("Supersamples is %i\n", supersamples);
            }else if(event.key.keysym.sym == SDLK_LEFT){
                if(supersamples > 1) supersamples--;
                printf("Supersamples is %i\n", supersamples);
            }else if(event.key.keysym.sym == SDLK_q){
                goto CLEANUP;
            }
        break;
        case SDL_WINDOWEVENT:
            switch(event.window.event){
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                G.resize(event.window.data1, event.window.data2);
                isRendered = false;
            break;
            }
        break;
        case SDL_QUIT: goto CLEANUP; break;
        }

        if(!isRendered){
            renderScene s(G, x, y, size);
            if(preciseMode || singlePrecise){
                s.draw(true);
                SDL_UpdateTexture(G.texturePrecise, NULL, s.pixels, G.realw * sizeof(uint32_t));
                SDL_RenderClear(G.renderer);
                SDL_RenderCopy(G.renderer, G.texturePrecise, NULL, NULL);
            }else{
                s.draw(false);
                SDL_UpdateTexture(G.texture, NULL, s.pixels, G.width * sizeof(uint32_t));
                SDL_RenderClear(G.renderer);
                SDL_RenderCopy(G.renderer, G.texture, NULL, NULL);
            }

            SDL_RenderPresent(G.renderer);

            singlePrecise = false;
            isRendered = true;
        }
        fflush(stdout);

        SDL_WaitEvent(&event);
    }

CLEANUP:

    return 0;
}
