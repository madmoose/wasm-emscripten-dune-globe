#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <array>

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

const int FRAMEBUFFER_WIDTH = 320;
const int FRAMEBUFFER_HEIGHT = 200;

SDL_Surface *screen = NULL;
std::array<uint8_t, FRAMEBUFFER_WIDTH* FRAMEBUFFER_HEIGHT> framebuffer;
int          frame = 0;

std::array<uint16_t, 396> globe_rotation_lookup_table;
std::array<uint16_t, 196> globe_tilt_lookup_table;

inline
int8_t uint8_as_int8(uint8_t u) {
	return u < INT8_MAX ? u : u - UINT8_MAX - 1;
}

inline
int16_t uint16_as_int16(uint16_t u) {
	return u < INT16_MAX ? u : u - UINT16_MAX - 1;
}

const uint16_t MAX_TILT = 98;

/*
 *  globe_rotation is value from 0x0000 - 0xffff.
 *  Think of it as the fractional part of a 16.16 fixed point number.
 */
void precalculate_globe_rotation_lookup_table(uint16_t globe_rotation) {
	constexpr uint32_t MAGIC_VALUE = 398;

	uint32_t dxax = MAGIC_VALUE * globe_rotation;

	// Floor the 16.16 fp value
	dxax &= ~0xffff;

	globe_rotation_lookup_table[2] = (dxax >> 16) & 0xffff;
	globe_rotation_lookup_table[3] = (dxax >>  0) & 0xffff;

	// Add 0.5 in 16.16 fixed point
	dxax += 0x8000;

	// Convert back to value from 0-1 in 16.16 fixed point
	uint16_t bx = dxax / MAGIC_VALUE;

	for (int i = 1; i != MAX_TILT+1; ++i) {
		uint32_t dxax = 2 * uint32_t(bx) * uint32_t(globe_rotation_lookup_table[4 * i + 1]);
		globe_rotation_lookup_table[4 * i + 2] = (dxax >> 16) & 0xffff;
		globe_rotation_lookup_table[4 * i + 3] = (dxax >>  0) & 0xffff;
	}
}

void precalculate_globe_tilt_lookup_table(int16_t globe_tilt) {
	int i = 0;

	if (globe_tilt < -MAX_TILT) {
		globe_tilt = -MAX_TILT;
	}
	if (globe_tilt > MAX_TILT) {
		globe_tilt = MAX_TILT;
	}

	// For stage 1, count down from globe_tilt-(MAX_TILT+1) to -MAX_TILT
	if (globe_tilt > 0) {
		int v = globe_tilt - MAX_TILT;
		do {
			globe_tilt_lookup_table[i++] = uint8_t(--v);
		} while (i != globe_tilt_lookup_table.size() && v > -MAX_TILT);
	}

	if (i == globe_tilt_lookup_table.size()) {
		return;
	}

	// For stage 2, count down from MAX_TILT to 0
	int v = globe_tilt + MAX_TILT - i;
	do {
		globe_tilt_lookup_table[i++] = uint8_t(v--);
	} while (i != globe_tilt_lookup_table.size() && v >= 0);

	if (i == globe_tilt_lookup_table.size()) {
		return;
	}

	// For stage 3, count up from 1 to MAX_TILT, binary-or'ed with 0xff00
	v = 1;
	do {
		globe_tilt_lookup_table[i++] = v++ | 0xff00;
	} while (i != globe_tilt_lookup_table.size() && v <= MAX_TILT);

	if (i == globe_tilt_lookup_table.size()) {
		return;
	}

	// For stage 4, count up from -MAX_TILT to 0, binary-or'ed with 0xff00
	v = -MAX_TILT;
	do {
		globe_tilt_lookup_table[i++] = v++ | 0xff00;
	} while (i != globe_tilt_lookup_table.size() && v <= 0);
}

inline
uint16_t frame_buffer_offset(int x, int y)
{
  return y * FRAMEBUFFER_WIDTH + x;
}

inline
void draw_pixel(uint8_t* screen, uint32_t offset, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
	screen[offset + 0] = red;
	screen[offset + 1] = green;
	screen[offset + 2] = blue;
	screen[offset + 3] = 255;
}

void draw_globe(uint8_t *framebuffer) {
	const uint8_t  *globdata = GLOBDATA_BIN;
	const uint8_t  *map      = MAP_BIN;

	uint16_t cs_1CA8 = 1;    // offset into globdata
	uint16_t cs_1CA6 = 1;    // offset into globdata
	uint16_t cs_1CAA = 3290; // offset into globdata
	uint16_t cs_1CAC = MAX_TILT;   // offset into globe_tilt_lookup_table

	uint16_t cs_1CB4 = -FRAMEBUFFER_WIDTH; // screen width

	const uint16_t globe_center_xy_offset1 = frame_buffer_offset(160, 79);
	uint16_t cs_1CB0 = globe_center_xy_offset1;
	uint16_t cs_1CB2 = globe_center_xy_offset1;
	uint16_t cs_1CAE = globe_center_xy_offset1 - 1;

	bool drawing_southern_hemisphere = false;

	do {
		uint16_t di = cs_1CA6; // offset into globdata

		uint16_t ax = globdata[di++];

		if (uint8_as_int8(ax & 0xff) < 0) {
			drawing_southern_hemisphere = true;
			di = cs_1CA8;

			cs_1CB4 = -cs_1CB4;
			if (uint16_as_int16(cs_1CB4) < 0) {
				return;
			}
			
			const uint16_t globe_center_xy_offset = frame_buffer_offset(160, 80);
			cs_1CB0 = globe_center_xy_offset;
			cs_1CB2 = globe_center_xy_offset;
			cs_1CAE = globe_center_xy_offset - 1;

			ax = globdata[di - 1];
			di -= uint8_as_int8(ax & 0xff);
			ax = globdata[di++];
		}

		uint16_t si = cs_1CAA; // offset into globdata

		do {
			if (drawing_southern_hemisphere) {
				ax = -ax;
			}

			uint16_t ax_ = ax; // not in use???
			ax = globe_tilt_lookup_table[cs_1CAC + uint8_as_int8(ax & 0xff)];

			struct result_t
			{
				uint16_t gd{}; // ax
				uint16_t grlt_0{}; // bx
				uint16_t grlt_1{}; // cx
				uint16_t grlt_2{}; // dx
			};

			auto func1 = [](const uint8_t* globdata_, uint16_t ofs1/*ax*/, uint16_t base_ofs/*si*/)
			{
				const bool neg_bx = uint16_as_int16(ofs1) < 0;
				
				uint16_t offset1 = uint8_as_int8(ofs1 & 0xff);
				const bool neg_ax = uint16_as_int16(offset1) < 0;
				if (neg_ax) {
					offset1 = -offset1;
				}

				result_t result;

				// 1 result
				uint16_t gd = globdata_[base_ofs + offset1 + 100];
				const uint16_t offset2 = globdata_[base_ofs + offset1] * 2;
				// 3 results from globe_rotation_lookup_table
				uint16_t grlt_0 = globe_rotation_lookup_table[offset2 + 0]; // bx
				uint16_t grlt_1 = globe_rotation_lookup_table[offset2 + 1]; // cx
				uint16_t grlt_2 = globe_rotation_lookup_table[offset2 + 2]; // dx

				if (neg_ax) {
					gd = grlt_1 - gd;
				}
				if (neg_bx)
				{
					grlt_0 = -grlt_0;
				}

				grlt_1 *= 2;

				return result_t{gd, grlt_0, grlt_1, grlt_2 };
			};

			auto some_offset = [](uint16_t value, uint16_t adjust1, uint16_t adjust2)
			{
				if (uint16_as_int16(value) < 0) {
					value += adjust1;
				}
				value += adjust2;
				return value;
			};

			auto pixel_color = [](uint16_t value)
			{
				//base color?
				uint8_t color = value & 0x0f;

				// this code is currently not in use
				if ((value & 0x30) == 0x10) {
					// overlay color?
					if (color < 8) {
						color += 12;
					}
				}

				color += 0x10;
				return color;
			};

			constexpr uint16_t MAGIC_OFS1 = 0x62FC;
			const uint8_t* sub_map = &map[MAGIC_OFS1];

			const result_t res = func1(globdata, ax, si);

			const int16_t ofs1 = uint16_as_int16(some_offset(
				res.grlt_2 - res.gd,
				res.grlt_1,
				res.grlt_0));
			framebuffer[cs_1CAE--] = pixel_color(sub_map[ofs1]);

			const int16_t ofs2 = uint16_as_int16(some_offset(
				res.grlt_2 + res.gd - res.grlt_1,
				res.grlt_1,
				res.grlt_0));
			framebuffer[cs_1CB0++] = pixel_color(sub_map[ofs2]);

			si += 200; // FRAMEBUFFER_WIDTH?
			ax = globdata[di++];
			// al = ax & 0x00ff;
		} while (uint8_as_int8(ax) >= 0);

		cs_1CA6 = di;
		cs_1CB2 = cs_1CB4 + cs_1CB2;
		cs_1CB0 = cs_1CB2;
		cs_1CAE = cs_1CB2 - 1;
	} while (true);
}

void draw_frame(void *user_data) {
	if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);

	float f = sinf(frame / 200.0);

	int16_t tilt = -MAX_TILT * f;
	int16_t rotation = 150 * frame;

	precalculate_globe_rotation_lookup_table(rotation);
	precalculate_globe_tilt_lookup_table(tilt);

	draw_globe(framebuffer.data());

	uint8_t *screenbuffer = (uint8_t*)screen->pixels;
	for (int i = 0; i != framebuffer.size(); ++i) {
		int c = framebuffer[i];
		unsigned c_offset = 3 * c;

#ifdef _WIN32
		unsigned char red = PAL_BIN[c_offset + 2];
		unsigned char green = PAL_BIN[c_offset + 1];
		unsigned char blue = PAL_BIN[c_offset + 0];
#else
		unsigned char red = PAL_BIN[c_offset + 0];
		unsigned char green = PAL_BIN[c_offset + 1];
		unsigned char blue = PAL_BIN[c_offset + 2];
#endif

#if 1
		unsigned x = i / FRAMEBUFFER_WIDTH;
		unsigned y = i % FRAMEBUFFER_WIDTH;
		for (unsigned w = 0; w < resolution_factor; ++w)
		{
			for (unsigned h = 0; h < resolution_factor; ++h)
			{
				const unsigned pixel_offset = ((x * resolution_factor + w) 
					                           * (FRAMEBUFFER_WIDTH * resolution_factor) 
					                           + (y * resolution_factor + h)) * 4;
				draw_pixel(screenbuffer,pixel_offset,red,green,blue,255); 
			}
		}
#else
		const unsigned pixel_offset = 4 * i;
        draw_pixel(screenbuffer,pixel_offset,red,green,blue,255); 
#endif
	}

	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

	SDL_Flip(screen);

	frame++;
}

extern "C"
int main() {
	SDL_Init(SDL_INIT_VIDEO);

	for (int i = 0; i != globe_rotation_lookup_table.size(); i++) {
		uint16_t u = (TABLAT_BIN[2*i + 0] << 0) + (TABLAT_BIN[2*i+ 1 ] << 8);
		globe_rotation_lookup_table[i] = u;
	}

	screen = SDL_SetVideoMode(FRAMEBUFFER_WIDTH*resolution_factor, FRAMEBUFFER_HEIGHT*resolution_factor, 32, SDL_SWSURFACE);

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
