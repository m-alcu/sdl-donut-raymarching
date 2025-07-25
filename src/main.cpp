#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <algorithm>
#include <sstream>
#include <SDL3/SDL.h>

const int dz = 5, r1 = 1, r2 = 2;
#define R(s,x,y) x-=(y>>s); y+=(x>>s)

// SDL screen dimensions
const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 240;

// CORDIC algorithm (same as your original code)
int length_cordic(int16_t x, int16_t y, int16_t *x2_, int16_t y2, int16_t iterations = 8) {
    int x2 = *x2_;
    if (x < 0) {
        x = -x;
        x2 = -x2;
    }
    for (int i = 0; i < iterations; i++) {
        int t = x;
        int t2 = x2;
        if (y < 0) {
            x -= y >> i;
            y += t >> i;
            x2 -= y2 >> i;
            y2 += t2 >> i;
        } else {
            x += y >> i;
            y -= t >> i;
            x2 += y2 >> i;
            y2 -= t2 >> i;
        }
    }
    *x2_ = (x2 >> 1) + (x2 >> 3);
    return (x >> 1) + (x >> 3);
}

uint32_t toBgra(int x,int y,int z) {

    float maxComponent = std::max({x, y, z});
    if (maxComponent > 255.0f) {
        float scale = 255.0f / maxComponent;
        return 0xff000000 | (static_cast<int>(x * scale)) << 16 |
                            (static_cast<int>(y * scale)) << 8 |
                            (static_cast<int>(z * scale));   
    }

    return 0xff000000 |
            (static_cast<int>(x)) << 16 |
            (static_cast<int>(y)) << 8 |
            (static_cast<int>(z));
}

// animated checkerboard pattern
void backgroundChecker(int sA, int sB, uint32_t* pixels)
{
  int x,y,xx,yy, xo,yo;
  xo = (25*sB >> 14)+50;
  yo = (25*sA >> 14)+50;

  for(y=0;y<SCREEN_HEIGHT;y++) {
    yy = (y+yo) % 32;
    for(x=0;x<SCREEN_WIDTH;x++) {
      xx = (x+xo) % 32;
      pixels[SCREEN_WIDTH*y+x] = ((xx<16 && yy<16) || (xx>16 && yy>16)) ? toBgra(0,86,253) : toBgra(170,170,170);
    }
  }
}

void drawTorus(SDL_Window* window, SDL_Renderer* renderer, SDL_Surface* sdlSurface) {
    int16_t sB = 0, cB = 16384;
    int16_t sA = 11583, cA = 11583;
    int16_t sAsB = 0, cAsB = 0;
    int16_t sAcB = 11583, cAcB = 11583;

    bool isRunning = true;
    SDL_Event event;
    auto* pixels = static_cast<uint32_t*>(sdlSurface->pixels);

    int iter1 = 8, iter2 = 8;
    int t=0;

    while (isRunning){

        if(t++>360) t-=360;

        //std::fill_n(pixels, SCREEN_WIDTH * SCREEN_HEIGHT, 0);
        backgroundChecker(sA, sB, pixels);

        while (SDL_PollEvent(&event))
            {
                SDL_KeyboardEvent* key_event = (SDL_KeyboardEvent*)&event;
                SDL_Keycode keycode = key_event->key;
                if (event.type == SDL_EVENT_QUIT) {
                    isRunning = false;
                } else if (event.type == SDL_EVENT_KEY_DOWN && keycode == SDLK_ESCAPE) {
                    isRunning = false;
                } else if (event.type == SDL_EVENT_KEY_DOWN && keycode == SDLK_Q) {
                    iter1++;;
                } else if (event.type == SDL_EVENT_KEY_DOWN && keycode == SDLK_A) {
                    iter1--;
                } else if (event.type == SDL_EVENT_KEY_DOWN && keycode == SDLK_W) {
                    iter2++;
                } else if (event.type == SDL_EVENT_KEY_DOWN && keycode == SDLK_S) {
                    iter2--;
                } 
            }

        // yes this is a multiply but dz is 5 so it's (sb + (sb<<2)) >> 6 effectively    
        int p0x = dz * sB >> 6;
        int p0y = dz * sAcB >> 6;
        int p0z = -dz * cAcB >> 6;

        const int r1i = r1 * 256;
        const int r2i = r2 * 256;

        int niters = 0;
        int nnormals = 0;
        int16_t yincC = (cA >> 6);
        int16_t yincS = (sA >> 6);

        int16_t xincX = (cB >> 7);
        int16_t xincY = (sAsB >> 7);
        int16_t xincZ = (cAsB >> 7);
        
        int16_t ycA = -(cA << 1);
        int16_t ysA = -(sA << 1);

        for (int j = 0; j < 240; j++, ycA += yincC, ysA += yincS) {
            int xsAsB = (sAsB >> 4) - sAsB;
            int xcAsB = (cAsB >> 4) - cAsB;

            int16_t vxi14 = (cB >> 4) - cB - sB;
            int16_t vyi14 = ycA - xsAsB - sAcB;
            int16_t vzi14 = ysA + xcAsB + cAcB;

            for (int i = 0; i < 240; i++, vxi14 += xincX, vyi14 -= xincY, vzi14 += xincZ) {
                int t = 512; // (256 * dz) - r2i - r1i;

                int16_t px = p0x + (vxi14 >> 5); // assuming t = 512, t*vxi>>8 == vxi<<1
                int16_t py = p0y + (vyi14 >> 5);
                int16_t pz = p0z + (vzi14 >> 5);

                int16_t lx0 = sB >> 2;
                int16_t ly0 = sAcB - cA >> 2;
                int16_t lz0 = -cAcB - sA >> 2;

                for (;;) {
                    int t0, t1, t2, d;
                    int16_t lx = lx0, ly = ly0, lz = lz0;
                    t0 = length_cordic(px, py, &lx, ly, iter1);
                    t1 = t0 - r2i;
                    t2 = length_cordic(pz, t1, &lz, lx, iter2);
                    d = t2 - r1i;
                    t += d;

                    if (t > 8 * 256) {
                        break;
                    } else if (d < 3) {
                        int N = lz >> 9;
                        // Draw to the SDL canvas (mapping ASCII characters to pixel brightness)
                        int color = (N > 0 ? N < 12 ? N : 11 : 0);
                        int index = j * SCREEN_WIDTH + i;
                        pixels[index] = toBgra(color * 25, color * 20, color * 5);
                        nnormals++;
                        break;
                    }

                    px += d * vxi14 >> 14;
                    py += d * vyi14 >> 14;
                    pz += d * vzi14 >> 14;
                    niters++;
                }
            }
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, sdlSurface);
        if (!tex) {
            SDL_Log("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        }

        SDL_RenderTexture(renderer, tex, nullptr, nullptr);

        std::ostringstream oss;
        oss << "r1: " << std::fixed << iter1
            << " r2: " << std::fixed << iter2;
        std::string title = oss.str();
        SDL_SetWindowTitle(window, title.c_str());      


        SDL_RenderPresent(renderer);
        SDL_DestroyTexture(tex);

        // rotate sines, cosines, and products thereof
        // this animates the torus rotation about two axes
        R(5, cA, sA);
        R(5, cAsB, sAsB);
        R(5, cAcB, sAcB);
        R(6, cB, sB);
        R(6, cAcB, cAsB);
        R(6, sAcB, sAsB);
        usleep(15000);

    }
}

int main() {
    // SDL initialization
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create window with SDL_Renderer graphics context
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow("Torus Animation", SCREEN_WIDTH, SCREEN_HEIGHT, window_flags);

    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }

    // Create the renderer with SDL3 renderer flags
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return -1;
    }

    SDL_Surface* sdlSurface = SDL_CreateSurface(SCREEN_WIDTH, SCREEN_HEIGHT, SDL_PIXELFORMAT_BGRA32);
    if (!sdlSurface) {
        SDL_Log("SDL_CreateSurface failed: %s", SDL_GetError());
    }

    SDL_SetSurfaceBlendMode(sdlSurface, SDL_BLENDMODE_NONE);



    SDL_ShowWindow(window);

    drawTorus(window, renderer, sdlSurface);

    // Cleanup
    SDL_DestroySurface(sdlSurface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
