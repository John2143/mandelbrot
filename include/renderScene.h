#pragma once
#include "global.h"

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
    static double mandelbrotDiverge(int zooms, bool preciseMode, floating real, floating imag){
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

struct view{
    floating x, y, size;
    int zooms;
    bool preciseMode = false;
    bool singlePrecise = false;

    view(){
        //x = 0;
        //y = 0;
        //size = 3;
        //= 0;

        x = -.748930;
        y = -.056503;
        size = .001465;
        zooms = 13;

        //x = -0.763833;
        //y = -0.094968;
        //size = .0000000001;
        //= 27;
        //
        //x = -.743421;
        //y = -.178880;
        //size = .00000001;
        //= 29;
    }
    view(floating x, floating y, floating size, int zooms):
        x(x), y(y), size(size), zooms(zooms){}
};

struct renderSettings{
    floating pctscale;
    bool square;
    int supersamples = 1;
    bool viewProgress = true;

    floating supersampleMapX[MAX_SUPERSAMPLES];
    floating supersampleMapY[MAX_SUPERSAMPLES];

    renderSettings():
        supersampleMapX{.17, .84, .17, .84, .50, .50, .50, .17, .84},
        supersampleMapY{.17, .84, .84, .17, .50, .17, .84, .50, .50}
    {
        for(size_t i = 9; i < MAX_SUPERSAMPLES; i++){
            supersampleMapX[i] = rand() / (floating) RAND_MAX;
            supersampleMapY[i] = rand() / (floating) RAND_MAX;
        }
    }

};

const int NUM_THREADS = std::thread::hardware_concurrency();

constexpr floating lerp(floating num, int max, floating start, floating end){
    return (num / (floating) max) * (end - start) + start;
}

struct renderScene{
    renderSettings set;
    view v;

    graphics &G;
    bool preciseMode;
    floating *data;

    floating endx, endy, startx, starty;
    int width, height;

    std::thread renderThread;

    std::atomic_bool isRendering;
    std::atomic_bool needsBlit, needsFullRender;

    renderScene(graphics &G): G(G){
        isRendering = false;
        needsBlit = false;
        needsFullRender = false;
        data = nullptr;
    }

    ~renderScene(){
        delete []data;
    }

    void removeData(){
        delete []data;
        data = nullptr;
    }

    std::atomic_int threadsDone;
    void draw(){
        printf("We are at (%f + %fi) with size %f and zooms %i\n", (float) v.x, (float) v.y, (float) v.size, v.zooms);
        bool precise = v.preciseMode || v.singlePrecise;

        if(preciseMode != precise) removeData();

        printf("preciseMode = %s, passed %s\n", BOOL(preciseMode), BOOL(precise));
        preciseMode = precise;

        //Transfer center coordinate and size to the coordinates of the corners
        //  of what we need to render.
        endx = v.x + v.size / 2;
        endy = v.y + v.size / 2 * G.aspectRatio;
        startx = v.x - v.size / 2;
        starty = v.y - v.size / 2 * G.aspectRatio;

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
            //Draw ending line
            for(int i = 0; i < width; i++){
                data[(linesoffset + numlines - 1) * width + i] = 0xff0f0f0f;
            }
            for(int y = 0; y < numlines; y++){
                drawline(y + linesoffset, &mem[y * width]);

                //Although we lock for a bit to copy data out, hyperthreading
                //should mean this is actually a free operation in reality

                std::lock_guard<std::mutex> lock(copyMutex);
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
                brightnessTotal += complex::mandelbrotDiverge(v.zooms, preciseMode,
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
            f /= ((MAX_ITERZ(preciseMode, v.zooms) + 1) * set.pctscale);
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
