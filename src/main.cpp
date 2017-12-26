#include <cstdio>
#include <cmath>
#include <SDL2/SDL.h>
#include <cstring>
#include <thread>
#include <mingw.thread.h>
#include <mutex>
#include <mingw.mutex.h>
#include <condition_variable>
#include <mingw.condition_variable.h>
#include <atomic>

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

    #define MAX_ITERZ(preciseMode, zooms) (preciseMode ? (100 + zooms * 200) : (10 + zooms * 50))
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
        for(int iter = 0; iter < MAX_ITER(preciseMode) - 1; iter++){
            curi = 2 * curi * curr + imag;
            curr = r2 - i2 + real;
            r2 = curr * curr;
            i2 = curi * curi;
            if(r2 == lastr && i2 == lasti) break;

            lastr = r2;
            lasti = i2;

            if(r2 + i2 > DIVERGENCE_BOUNDARY * DIVERGENCE_BOUNDARY){
                return 1 + iter - log(log(r2 + i2)) / log(2);
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

    void resize(int w, int h){
        if(w == realw && h == realh) return;

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

    }

};

struct renderSettings{
    floating pctscale;
    bool square;
};

#define NUM_THREADS 8

floating lerp(floating num, int max, floating start, floating end){
    return (num / (floating) max) * (end - start) + start;
}

#define MAX_SUPERSAMPLES 100
static floating supersampleMapX[MAX_SUPERSAMPLES] = {.17, .84, .17, .84, .50, .50, .50, .17, .84};
static floating supersampleMapY[MAX_SUPERSAMPLES] = {.17, .84, .84, .17, .50, .17, .84, .50, .50};

static int supersamples = 1;

struct renderScene{
    renderSettings set;
    const graphics &G;
    bool preciseMode;
    floating *data;
    floating xpos, ypos, xsize;

    floating endx, endy, startx, starty;
    int width, height;

    renderScene(const graphics &G, floating x, floating y, floating size):
        G(G), xpos(x), ypos(y), xsize(size){

        data = nullptr;
    }

    ~renderScene(){
        delete []data;
    }

    renderScene &operator=(const renderScene &rhs){
        xpos = rhs.xpos;
        ypos = rhs.ypos;
        xsize = rhs.xsize;
        return *this;
    }

#define MARK() printf("%i\n", __LINE__); fflush(stdout);
#define BOOL(b) ((b) ? "true" : "false")

    std::atomic_int threadsDone;
    void draw(bool precise){
        printf("We are at (%f + %fi) with size %f (zooms %i)\n", (float) xpos, (float) ypos, (float) xsize, zooms);

        if(preciseMode != precise){
            delete []data;
            data = nullptr;
        }
        printf("preciseMode = %s, passed %s\n", BOOL(preciseMode), BOOL(precise));
        preciseMode = precise;

        xsize /= 2;
        endx = xpos + xsize;
        endy = ypos + xsize * G.aspectRatio;
        startx = xpos - xsize;
        starty = ypos - xsize * G.aspectRatio;
        xsize *= 2;

        width  = preciseMode ? G.realw : G.width;
        height = preciseMode ? G.realh : G.height;

        if(data == nullptr){
            data = new floating[width * height * 3];
        }

        threadsDone.store(0);
        std::thread threads[NUM_THREADS];
        int numlines = height/NUM_THREADS;
        for(int i = 0; i < NUM_THREADS; i++){
            //make the last thread handle the remaining lines
            int linesLeft = i == NUM_THREADS - 1 ? numlines + (height % NUM_THREADS) : numlines;
            threads[i] = std::thread([=](){
                drawThread(i * numlines, linesLeft);
                threadsDone++;
            });
        }

        while(threadsDone.load() != NUM_THREADS){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            blit();
        }

        for(int i = 0; i < NUM_THREADS; i++){
            threads[i].join();
        }
    }

    std::mutex copyMutex;

    void drawThread(int linesoffset, int numlines){
        printf("Starting at y=%i, len=%i, width=%i\n", linesoffset, numlines, width);
        const int bufferSize = numlines * width;
        floating *mem = new floating[bufferSize];

        for(int y = 0; y < numlines; y++){
            drawline(y + linesoffset, &mem[y * width]);
            copyMutex.lock();
            for(int i = 0; i < width; i++){
                data[(linesoffset + y) * width + i] = mem[i + y * width];
            }
            copyMutex.unlock();
        }

        delete []mem;
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

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define clamp(n, a, b) min(max(n, a), b)

    void shader(uint32_t *pixels){
        for(int i = 0; i < width * height; i++){
            float f = data[i];
            f /= ((MAX_ITER(preciseMode) + 1) * set.pctscale);
            f = clamp(f, 0, 1);
            if(set.square){
                f *= f;
            }
            uint32_t value = f * 0xff;
            value = 0xFF000000 + value + (value << 8) + (value << 16);
            pixels[i] = value;
        }
    }

    void blit(){
        uint32_t *pixels = new uint32_t[width * height];
        shader(pixels);

        SDL_Texture *tex = preciseMode ? G.texturePrecise : G.texture;
        size_t lineLength = preciseMode ? G.realw : G.width;

        SDL_UpdateTexture(tex, NULL, pixels, lineLength * sizeof(uint32_t));
        SDL_RenderClear(G.renderer);
        SDL_RenderCopy(G.renderer, tex, NULL, NULL);
        SDL_RenderPresent(G.renderer);
        delete []pixels;
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

    //floating x = -0.763833;
    //floating y = -0.094968;
    //floating size = .0000000001;
    //zooms = 27;
    //
    //floating x = -.743421;
    //floating y = -.178880;
    //floating size = .00000001;
    //zooms = 29;

    bool preciseMode = false;
    bool singlePrecise = false;

    bool isCalculated = false;
    bool isRendered = true;

    renderScene s(G, x, y, size);
    s.set.pctscale = 1;
    s.set.square = true;

    bool fullscreen = false;

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
                isCalculated = false;
                zooms++;
                s = renderScene(G, x, y, size);
            }else if(event.button.button == SDL_BUTTON_RIGHT){
                if(zooms > 0){
                    size *= 2;
                    isCalculated = false;
                    zooms--;
                }
                s = renderScene(G, x, y, size);
            }
        break;
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_f){
                preciseMode = !preciseMode;
                printf("Precise mode %s\n", preciseMode ? "yes" : "no");
            }else if(event.key.keysym.sym == SDLK_r){
                singlePrecise = true;
                isCalculated = false;
                printf("Claculating a single frame\n");
            }else if(event.key.keysym.sym == SDLK_e){
                isRendered = false;
                printf("Rendering a single frame\n");
            }else if(event.key.keysym.sym == SDLK_RIGHT){
                if((size_t) supersamples < sizeof(supersampleMapX) / sizeof(supersampleMapX[0])) supersamples++;
                printf("Supersamples is %i\n", supersamples);
            }else if(event.key.keysym.sym == SDLK_LEFT){
                if(supersamples > 1) supersamples--;
                printf("Supersamples is %i\n", supersamples);
            }else if(event.key.keysym.sym == SDLK_UP){
                if(s.set.pctscale < 1) s.set.pctscale += .025;
                isRendered = false;
                printf("Gamma scaling is %f\n", s.set.pctscale);
            }else if(event.key.keysym.sym == SDLK_DOWN){
                if(s.set.pctscale > 0) s.set.pctscale -= .025;
                isRendered = false;
                printf("Gamma scaling is %f\n", s.set.pctscale);
            }else if(event.key.keysym.sym == SDLK_s){
                s.set.square = !s.set.square;
                printf("Square mode %s\n", BOOL(s.set.square));
            }else if(event.key.keysym.sym == SDLK_b){
                fullscreen = !fullscreen;
                printf("fullscreen mode %s\n", BOOL(fullscreen));
                SDL_SetWindowFullscreen(G.window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                isCalculated = false;
            }else if(event.key.keysym.sym == SDLK_q){
                goto CLEANUP;
            }
        break;
        case SDL_WINDOWEVENT:
            switch(event.window.event){
            //case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                G.resize(event.window.data1, event.window.data2);
                isCalculated = false;
            break;
            }
        break;
        case SDL_QUIT: goto CLEANUP; break;
        }

        try{
            if(!isCalculated){
                s.draw(preciseMode || singlePrecise);
                s.blit();
                singlePrecise = false;
                isCalculated = true;
                isRendered = true;
            }else if(!isRendered){
                s.blit();
                isRendered = true;
            }
        }catch(std::bad_alloc& exc){
            printf("OOM\n");
            fflush(stdout);
        }
        fflush(stdout);

        SDL_WaitEvent(&event);
    }

CLEANUP:

    return 0;
}
