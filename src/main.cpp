#include <cstdio>
#include <cmath>
#include <SDL2/SDL.h>

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

    #define MAX_ITER(preciseMode) (preciseMode ? (100 + zooms * 100) : (50 + zooms * 60))
    const floating DIVERGENCE_BOUNDARY = 2;
    double mandelbrotDiverge(bool preciseMode) const{
        floating curr = 0;
        floating curi = 0;
        floating r2 = 0;
        floating i2 = 0;
        for(int iter = 0; iter < MAX_ITER(preciseMode); iter++){
            curi = 2 * curi * curr + imag;
            curr = r2 - i2 + real;
            r2 = curr * curr;
            i2 = curi * curi;

            if(r2 + i2 > DIVERGENCE_BOUNDARY * DIVERGENCE_BOUNDARY){
                return 1 + iter - log(log(sqrt(r2 + i2)) / 2);
            }
        }
        return MAX_ITER(preciseMode);
    }
};

uint32_t hsvToRGB(floating h, floating s, floating v){
    if(h >= 360.) h = 0;
    floating hh = h / 60.;
    long i = hh;
    floating ff = hh - i;
    floating p = v * (1. - s);
    floating q = v * (1. - (s * ff));
    floating t = v * (1. - (s * (1 - ff)));

    #define intify(x) uint8_t x##_ = x * 0xff;
    intify(v);
    intify(p);
    intify(q);
    intify(t);
    #undef intify

    switch(i){
    case 0: return v_ << 16 | t_ << 8 | p_;
    case 1: return q_ << 16 | v_ << 8 | p_;
    case 2: return p_ << 16 | v_ << 8 | t_;
    case 3: return p_ << 16 | q_ << 8 | v_;
    case 4: return t_ << 16 | p_ << 8 | v_;
    case 5: return v_ << 16 | p_ << 8 | q_;
    default: return 0;
    }
}

static struct graphics{
    SDL_Window *window;
    int realw, realh;
    int scaleFactor;
    int width, height;
    int aspectRatio;
} G;

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

void draw(bool preciseMode, uint32_t *pixels, floating xpos, floating ypos, floating xsize){
    bool has = false;
    xsize /= 2;
    floating endx = xpos + xsize;
    floating endy = ypos + xsize * G.aspectRatio;
    floating startx = xpos - xsize;
    floating starty = ypos - xsize * G.aspectRatio;
    int width  = preciseMode ? G.realw : G.width;
    int height = preciseMode ? G.realh : G.height;

    for(int y = 0; y < height; y++){
        for(int x = 0; x < width; x++){
            double brightnessTotal = 0;
            for(int sam = 0; sam < supersamples; sam++){
                complex num(
                    lerp(x + supersampleMapX[sam], width,  startx, endx),
                    lerp(y + supersampleMapY[sam], height, starty, endy)
                );
                brightnessTotal += num.mandelbrotDiverge(preciseMode) / (MAX_ITER(preciseMode) + 1);
                if(!has) printf("brightness %f\n", brightnessTotal);
            }
            brightnessTotal /= supersamples;
            brightnessTotal *= brightnessTotal;
            uint32_t value = brightnessTotal * 0xff;
            if(!has) printf("vaoue %i\n", value);
            has = true;
            value = 0xFF000000 + value + (value << 8) + (value << 16);
            //uint32_t value = hsvToRGB(brightness * 360, 1, 1);
            //value += 0xFF000000;
            *pixels++ = value;
        }
    }
}

//.743421 + -.178880i is cool

int main(int argc, char *argv[]){
    (void) argc; (void) argv;
    SDL_Init(SDL_INIT_VIDEO);

    for(int i = 9; i < MAX_SUPERSAMPLES; i++){
        supersampleMapX[i] = rand() / (floating) RAND_MAX;
        supersampleMapY[i] = rand() / (floating) RAND_MAX;
    }

    for(int i = 0; i < MAX_SUPERSAMPLES; i++){
        printf("%f %f\n", supersampleMapX[i], supersampleMapY[i]);
    }

    G.realw = 1200; G.realh = 900;
    G.scaleFactor = 4;
    G.width = G.realw / G.scaleFactor;
    G.height = G.realh / G.scaleFactor;
    G.window = SDL_CreateWindow(
        "mandelbrot", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        G.realw, G.realh, 0
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(G.window, -1, 0);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, G.width, G.height);
    SDL_Texture *texturePrecise = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, G.realw, G.realh);

    G.aspectRatio = G.width/G.height;
    uint32_t *pixels = new uint32_t[G.width * G.height];
    uint32_t *pixelsPrecise = new uint32_t[G.realw * G.realh];

    floating x = -.743421;
    floating y = -.178880;
    floating size = .00000001;
    zooms = 29;

    bool preciseMode = false;
    bool singlePrecise = false;
    bool isRendered = false;

    SDL_Event event;
    event.type = -1;
    for(;;) {
        switch(event.type){
        case SDL_MOUSEBUTTONDOWN:
            if(event.button.button == SDL_BUTTON_LEFT){
                x = lerp(event.button.x, G.realw, x - size/2, x + size / 2);
                y = lerp(event.button.y, G.realh, y - size/2, y + size * G.aspectRatio / 2);
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
            printf("We are at (%f + %fi) with size %f (zooms %i)\n", (float) x, (float) y, (float) size, zooms);
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
        case SDL_QUIT: goto CLEANUP; break;
        }

        if(!isRendered){
            SDL_RenderClear(renderer);

            if(preciseMode || singlePrecise){
                draw(true, pixelsPrecise, x, y, size);
                SDL_UpdateTexture(texturePrecise, NULL, pixelsPrecise, G.realw * sizeof(uint32_t));
                SDL_RenderCopy(renderer, texturePrecise, NULL, NULL);
                singlePrecise = false;
            }else{
                draw(false, pixels, x, y, size);
                SDL_UpdateTexture(texture, NULL, pixels, G.width * sizeof(uint32_t));
                SDL_RenderCopy(renderer, texture, NULL, NULL);
            }

            SDL_RenderPresent(renderer);
            isRendered = true;
        }
        fflush(stdout);

        SDL_WaitEvent(&event);
    }

CLEANUP:

    delete[] pixelsPrecise;
    delete[] pixels;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(G.window);
    SDL_Quit();
    return 0;
}
