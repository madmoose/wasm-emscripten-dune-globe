#include <cstdio>
#include <cstdint>
#include <cassert>

#include <SDL/SDL.h>
#undef main

unsigned resolution_factor = 3; // 1=320x200, 2=640x40, 3=1280x800, ...
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "GLOBDATA.BIN.inc"
#include "MAP.BIN.inc"
#include "PAL.BIN.inc"
#include "TABLAT.BIN.inc"

SDL_Surface *screen = NULL;
uint8_t      framebuffer[320*200];
int          frame = 0;

inline
int8_t uint8_as_int8(uint8_t u) {
	return u < INT8_MAX ? u : u - UINT8_MAX - 1;
}

inline
int16_t uint16_as_int16(uint16_t u) {
	return u < INT16_MAX ? u : u - UINT16_MAX - 1;
}

void draw_globe(uint8_t *framebuffer) {
	const uint8_t  *globdata = GLOBDATA_BIN;
	const uint8_t  *map      = MAP_BIN;
	uint16_t tablat[792/2];

	for (int i = 0; i != 792; i += 2) {
		uint16_t u = (TABLAT_BIN[i+0] << 0) + (TABLAT_BIN[i+1] << 8);
		tablat[i/2] = u;
	}

	int16_t  word_21910 = -0 - (frame % 96);
	int16_t  unknown_table_5[196];
	int16_t  idx = 0;
	{
		int16_t bx = 98;
		int16_t ax = word_21910;
		ax += bx;
		if (ax > bx) {
			assert(0 && "TODO");
		}

		do {
			unknown_table_5[idx++] = ax--;
		} while (ax >= 0 && idx != 196);
		if (idx == 196) {
			goto end;
		}

		ax = 1;
		do {
			unknown_table_5[idx++] = 0xff00 | ax;
			ax++;
		} while (ax <= bx && idx != 196);
		if (idx == 196) {
			goto end;
		}

		ax = 0x100 - bx;

		do {
			unknown_table_5[idx++] = 0xff00 | ax;
			ax++;
		} while (idx != 196);

end:;
	}

	uint16_t cs_1CA8 = 1;    // offset into globdata
	uint16_t cs_1CA6 = 1;    // offset into globdata
	uint16_t cs_1CAA = 3290; // offset into globdata
	uint16_t cs_1CAC = 98;   // offset into unknown_table_5

	uint16_t cs_1CB4 = -320; // screen width

	uint16_t cs_1CB0 = 0x6360;
	uint16_t cs_1CB2 = 0x6360;
	uint16_t cs_1CAE = 0x6360 - 1;

	bool drawing_southern_hemisphere;

	drawing_southern_hemisphere = false;

	do {
		uint16_t di = cs_1CA6; // offset into globdata

		uint16_t ax = globdata[di++];
		uint16_t bx, cx, dx;

		if (uint8_as_int8(ax & 0xff) < 0) {
			drawing_southern_hemisphere = true;
			di = cs_1CA8;

			cs_1CB4 = -cs_1CB4;
			if (uint16_as_int16(cs_1CB4) < 0) {
				return;
			}
			cs_1CB0 = 0x64A0;
			cs_1CB2 = 0x64A0;
			cs_1CAE = 0x64A0 - 1;

			ax = globdata[di - 1];
			di -= uint8_as_int8(ax & 0xff);
			ax = globdata[di++];
		}

		uint16_t si = cs_1CAA; // offset into globdata

		do {
			if (drawing_southern_hemisphere) {
				ax = -ax;
			}

			uint16_t ax_ = ax;
			ax = unknown_table_5[cs_1CAC + uint8_as_int8(ax & 0xff)];

			if (uint16_as_int16(ax) >= 0) {
				ax = uint8_as_int8(ax & 0xff);
				if (uint16_as_int16(ax) < 0) {
					ax = -ax;
					uint16_t bp = ax;
					bx = globdata[bp + si];
					ax = globdata[bp + si + 0x64];

					bp = bx;
					bx = tablat[2 * bp + 0];
					cx = tablat[2 * bp + 1];
					dx = tablat[2 * bp + 2];

					ax = cx - ax;
				} else {
					uint16_t bp = ax;
					bx = globdata[bp + si];
					ax = globdata[bp + si + 0x64];

					bp = bx;
					bx = tablat[2 * bp + 0];
					cx = tablat[2 * bp + 1];
					dx = tablat[2 * bp + 2];
				}
			} else {
				ax = uint8_as_int8(ax & 0xff);
				if (uint16_as_int16(ax) < 0) {
					ax = -ax;
					uint16_t bp = ax;
					bx = globdata[bp + si];
					ax = globdata[bp + si + 0x64];

					bp = bx;
					bx = tablat[2 * bp + 0];
					cx = tablat[2 * bp + 1];
					dx = tablat[2 * bp + 2];

					ax = cx - ax;
					bx = -bx;
				} else {
					uint16_t bp = ax;
					bx = globdata[bp + si];
					ax = globdata[bp + si + 0x64];

					bp = bx;
					bx = tablat[2 * bp + 0];
					cx = tablat[2 * bp + 1];
					dx = tablat[2 * bp + 2];

					bx = -bx;
				}
			}

			cx *= 2;

			uint16_t bp = dx - ax;
			if (uint16_as_int16(bp) < 0) {
				bp += cx;
			}
			bp += bx;
			dx += ax;

			ax = map[0x62FC + uint16_as_int16(bp)];
			{
				uint8_t al = ax & 0x0f;
				if ((ax & 0x30) == 0x10) {
					if (al < 8) {
						al += 12;
					}
				}
				al += 0x10;

				framebuffer[cs_1CAE--] = al;
			}

			bp = dx - cx;
			if (uint16_as_int16(bp) < 0) {
				bp += cx;
			}
			bp += bx;

			ax = map[0x62FC + uint16_as_int16(bp)];
			{
				uint8_t al = ax & 0x0f;
				if ((ax & 0x30) == 0x10) {
					if (al < 8) {
						al += 12;
					}
				}
				al += 0x10;

				framebuffer[cs_1CB0++] = al;
			}

			si += 200;
			ax = globdata[di++];
			// al = ax & 0x00ff;
		} while (uint8_as_int8(ax) >= 0);

		cs_1CA6 = di;
		ax = cs_1CB4 + cs_1CB2;
		cs_1CB2 = ax;
		cs_1CB0 = ax;
		cs_1CAE = ax - 1;
	} while (true);
}

void draw_frame(void *user_data) {
	if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);

	draw_globe(framebuffer);

	uint8_t *screenbuffer = (uint8_t*)screen->pixels;
	for (int i = 0; i != 64000; ++i) {
		int c = framebuffer[i];
#if 1 // for my windows
		unsigned char red = PAL_BIN[3 * c + 2];
		unsigned char green = PAL_BIN[3 * c + 1];
		unsigned char blue = PAL_BIN[3 * c + 0];
#else
		unsigned char red = PAL_BIN[3 * c + 0];
		unsigned char green = PAL_BIN[3 * c + 1];
		unsigned char blue = PAL_BIN[3 * c + 2];
#endif
#if 1
		unsigned x = i / 320;
		unsigned y = i % 320;
		for (unsigned w = 0; w < resolution_factor; ++w)
		{
			for (unsigned h = 0; h < resolution_factor; ++h)
			{
				const unsigned pixel_offset = ((x * resolution_factor + w) * (320 * resolution_factor) + (y * resolution_factor + h)) * 4;
				screenbuffer[pixel_offset + 0] = red;
				screenbuffer[pixel_offset + 1] = green;
				screenbuffer[pixel_offset + 2] = blue;
				screenbuffer[pixel_offset + 3] = 255;
			}
		}
#else
		const unsigned pixel_offset = 4 * i;
		screenbuffer[pixel_offset + 0] = red;
		screenbuffer[pixel_offset + 1] = green;
		screenbuffer[pixel_offset + 2] = blue;
		screenbuffer[pixel_offset + 3] = 255;
#endif
	}

	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

	SDL_Flip(screen);

	frame++;
}

extern "C"
int main() {
	SDL_Init(SDL_INIT_VIDEO);

	screen = SDL_SetVideoMode(320*resolution_factor, 200*resolution_factor, 32, SDL_SWSURFACE);

#ifdef TEST_SDL_LOCK_OPTS
	EM_ASM("SDL.defaults.copyOnLock = false; SDL.defaults.discardOnLock = true; SDL.defaults.opaqueFrontBuffer = false;");
#endif

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(draw_frame, NULL, -1, 1);
#else
	bool run = true;
	while (run) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {  // poll until all events are handled!
			if (event.type == SDL_QUIT)
			{
				run = false;
				break;
			}
		}

		draw_frame(nullptr);
		SDL_Delay(10);
	}
#endif
	SDL_Quit();

	return 0;
}
