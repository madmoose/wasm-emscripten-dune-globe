//http://www.youtube.com/user/thecplusplusguy
//Thanks for the typed in code to Tapit85
#include <SDL/SDL.h>

int drag_test_main()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Surface* screen{};
    screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);
    bool running = true;
    const int FPS = 30;
    Uint32 start{};
    bool b[4] = { 0,0,0,0 };
    SDL_Rect rect;
    rect.x = 100;
    rect.y = 100;
    rect.w = 200;
    rect.h = 200;
    int x{};
    int y{};
    Uint32 color2 = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff);
    Uint32 color = SDL_MapRGB(screen->format, 0, 0, 0);
    while (running) {
        start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_MOUSEMOTION:
                x = event.motion.x;
                y = event.motion.y;
                printf("SDL_MOUSEMOTION event.motion.x: %u, event.motion.y: %u\n", x, y);
                if (x > rect.x && x<rect.x + rect.w && y>rect.y && y < rect.y + rect.h) {
                    color2 = SDL_MapRGB(screen->format, 0x00, 0x00, 0xff);
                }
                else {
                    color2 = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                x = event.button.x;
                y = event.button.y;
                printf("SDL_MOUSEBUTTONDOWN event.button.x: %u, event.button.y: %u\n", x, y);
                if (x > rect.x && x<rect.x + rect.w && y>rect.y && y < rect.y + rect.h && event.button.button == SDL_BUTTON_LEFT) {
                    color2 = SDL_MapRGB(screen->format, 0xff, 0x00, 0xff);
                }
                break;
            case SDL_MOUSEBUTTONUP:
                x = event.button.x;
                y = event.button.y;
                printf("SDL_MOUSEBUTTONUP event.button.x: %u, event.button.y: %u\n", x, y);
                if (x > rect.x && x<rect.x + rect.w && y>rect.y && y < rect.y + rect.h && event.button.button == SDL_BUTTON_LEFT) {
                    color2 = SDL_MapRGB(screen->format, 0xff, 0xff, 0x00);
                }
                break;
            }
        }

        SDL_FillRect(screen, &rect, color);

        //logic
        if (b[0])
            rect.y--;
        if (b[1])
            rect.x--;
        if (b[2])
            rect.y++;
        if (b[3])
            rect.x++;

        //render
        SDL_FillRect(screen, &rect, color2);
        SDL_Flip(screen);

        if (1000 / FPS > SDL_GetTicks() - start) {
            SDL_Delay(1000 / FPS - (SDL_GetTicks() - start));
        }
    }
    SDL_Quit();
    return 0;
}

using uchar = unsigned char;

#include <cmath>

void ppixel(SDL_Surface* screen, int x, int y, uchar r, uchar g, uchar b);

int plasma_main()
{
    SDL_Event event;
    Uint32 last_time;
    int cont = 1, x, y;
    uchar r[256], g[256], b[256];
    double f = 0;
    SDL_Surface* screen;
    SDL_Init(SDL_INIT_VIDEO);
    screen = SDL_SetVideoMode(320, 200, 32, SDL_HWSURFACE);

    for (x = 0; x < 256; x++)
    {
        r[x] = 255 - ceil((sin(3.14 * 2 * x / 255) + 1) * 127);
        g[x] = ceil((sin(3.14 * 2 * x / 127.0) + 1) * 64);
        b[x] = 255 - r[x];
    }

    last_time = SDL_GetTicks();
    while (cont == 1)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                cont = 0;
                break;
            }
        }

        for (y = 0; y < 200; y++)
        {
            for (x = 0; x < 320; x++)
            {
                float c1 = sin(x / 50.0 + f + y / 200.0);
                float c2 = sqrt((sin(0.8 * f) * 160 - x + 160) * (sin(0.8 * f) * 160 - x + 160) + (cos(1.2 * f) * 100 - y + 100) * (cos(1.2 * f) * 100 - y + 100));
                c2 = sin(c2 / 50.0);
                float c3 = (c1 + c2) / 2;

                int res = ceil((c3 + 1) * 127);

                ppixel(screen, x, y, r[res], g[res], b[res]);
            }
        }

        SDL_Flip(screen);

        f += 0.1;
        if (SDL_GetTicks() - last_time < 40)
        {
            SDL_Delay(40 - (SDL_GetTicks() - last_time));
        }

        last_time = SDL_GetTicks();
    }


    SDL_Quit();

    return 0;
}

void ppixel(SDL_Surface* screen, int x, int y, uchar r, uchar g, uchar b)
{
    unsigned long color = SDL_MapRGB(screen->format, r, g, b);
    unsigned long* bufp;

    bufp = (unsigned long*)screen->pixels + y * screen->pitch / 4 + x;
    *bufp = color;
}