#include <cstdio>
#include <cmath>
#include <SDL2/SDL.h>
#include <cstring>
#include <thread>

#ifdef __MINGW32__
#    include <mingw.thread.h>
#    include <mingw.mutex.h>
#    include <mingw.condition_variable.h>
#endif
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

typedef double floating;

static int zooms = 0;

struct complex{
    static void print(floating real, floating imag){
        printf("(%f + %fi)", (float) real, (float) imag);
    }

    static bool checkCardoid(floating real, floating imag){
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
    static constexpr floating DIVERGENCE_BOUNDARY = 2.;
    static double mandelbrotDiverge(bool preciseMode, floating real, floating imag){
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

struct renderSettings{
    floating pctscale;
    bool square;
    int supersamples = 4;
    bool viewProgress = false;

#define MAX_SUPERSAMPLES 100
    floating supersampleMapX[MAX_SUPERSAMPLES];
    floating supersampleMapY[MAX_SUPERSAMPLES];

    renderSettings():
        supersampleMapX{.17, .84, .17, .84, .50, .50, .50, .17, .84},
        supersampleMapY{.17, .84, .84, .17, .50, .17, .84, .50, .50}
    {
        for(int i = 9; i < MAX_SUPERSAMPLES; i++){
            supersampleMapX[i] = rand() / (floating) RAND_MAX;
            supersampleMapY[i] = rand() / (floating) RAND_MAX;
        }
    }

};

static int NUM_THREADS = std::thread::hardware_concurrency();

inline floating lerp(floating num, int max, floating start, floating end){
    return (num / (floating) max) * (end - start) + start;
}

struct renderScene{
    renderSettings set;
    graphics &G;
    bool preciseMode;
    floating *data;
    floating xpos, ypos, xsize;

    floating endx, endy, startx, starty;
    int width, height;

    std::thread renderThread;

    std::atomic_bool isRendering;
    std::atomic_bool needsBlit;

    renderScene(graphics &G, floating x, floating y, floating size):
        G(G), xpos(x), ypos(y), xsize(size){

        isRendering = false;
        needsBlit = false;
        data = nullptr;
    }

    ~renderScene(){
        delete []data;
    }

    renderScene &operator=(const renderScene &rhs){
        xpos = rhs.xpos;
        ypos = rhs.ypos;
        xsize = rhs.xsize;
        delete []data;
        data = nullptr;
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
            data = new floating[width * height];
        }

        threadsDone = 0;
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

        if(set.viewProgress){
            while(threadsDone.load() != NUM_THREADS){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                needsBlit = true;
            }
        }

        for(int i = 0; i < NUM_THREADS; i++){
            threads[i].join();
        }

        isRendering = false;
    }

    std::mutex copyMutex;

    void drawThread(int linesoffset, int numlines){
        printf("Starting at y=%i, len=%i, width=%i\n", linesoffset, numlines, width);
        const int bufferSize = numlines * width;
        floating *mem = new floating[bufferSize];

        if(set.viewProgress){
            for(int i = 0; i < width; i++){
                data[(linesoffset + numlines - 1) * width + i] = 0xff0f0f0f;
            }
            for(int y = 0; y < numlines; y++){
                drawline(y + linesoffset, &mem[y * width]);
                std::lock_guard<std::mutex> lock(copyMutex);
                //set line in memory
                for(int i = 0; i < width; i++){
                    data[(linesoffset + y) * width + i] = mem[i + y * width];
                }
                //draw "leading line"
                if(y != numlines - 1){
                    for(int i = 0; i < width; i++){
                        data[(linesoffset + y + 1) * width + i] = 0xffff0ff0;
                    }
                }
            }
        }else{
            for(int y = 0; y < numlines; y++){
                drawline(y + linesoffset, &mem[y * width]);
            }
            std::lock_guard<std::mutex> lock(copyMutex);
            memcpy(&data[linesoffset * width], mem, bufferSize * sizeof(floating));
        }

        delete []mem;
    }

    void drawline(int y, floating *memory){
        for(int x = 0; x < width; x++){
            floating brightnessTotal = 0;
            for(int sam = 0; sam < set.supersamples; sam++){
                brightnessTotal += complex::mandelbrotDiverge(preciseMode,
                    lerp(x + set.supersampleMapX[sam], width,  startx, endx),
                    lerp(y + set.supersampleMapY[sam], height, starty, endy)
                );
            }
            brightnessTotal /= set.supersamples;
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
        if(data == nullptr) return;
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



bool testmode;
int main(int argc, char *argv[]){
    if(argc == 2){
        if(strcmp(argv[1], "test")) testmode = true;
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
    bool singlePrecise = testmode;

    bool isCalculated = true;
    bool isRendered = true;

    renderScene s(G, x, y, size);
    s.set.pctscale = 1;
    s.set.square = true;

    bool fullscreen = false;

    SDL_Event event;
    event.type = -1;
    for(;;) {
        if(!s.isRendering){
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
                    if((size_t) s.set.supersamples < sizeof(s.set.supersampleMapX) / sizeof(s.set.supersampleMapX[0])) s.set.supersamples++;
                    printf("Supersamples is %i\n", s.set.supersamples);
                }else if(event.key.keysym.sym == SDLK_LEFT){
                    if(s.set.supersamples > 1) s.set.supersamples--;
                    printf("Supersamples is %i\n", s.set.supersamples);
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
                }else if(event.key.keysym.sym == SDLK_p){
                    s.set.viewProgress = !s.set.viewProgress;
                    printf("view progress mode %s\n", BOOL(s.set.viewProgress));
                }else if(event.key.keysym.sym == SDLK_q){
                    goto CLEANUP;
                }
            break;
            case SDL_WINDOWEVENT:
                switch(event.window.event){
                //case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    isCalculated = !G.resize(event.window.data1, event.window.data2);
                break;
                }
            break;
            case SDL_QUIT: goto CLEANUP; break;
            }
        }

        if(!isCalculated && !s.isRendering){
            s.isRendering = true;

            s.renderThread = std::thread([&]{
                printf("Thread started \n");
                s.draw(preciseMode || singlePrecise);
                printf("Thread  drawn\n");
                singlePrecise = false;
                isCalculated = true;
                isRendered = true;
            });

            s.renderThread.detach();

            //if(testmode) goto CLEANUP;
        }
        if(s.needsBlit || !isRendered){
            s.blit();
            printf("Thread blit\n");
            s.needsBlit = false;
            isRendered = true;
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

CLEANUP:

    return 0;
}
