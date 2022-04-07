#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <array>
#include <string>
#include <fstream>
#include <iostream>

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

constexpr int FRAMEBUFFER_WIDTH = 320;
constexpr int FRAMEBUFFER_HEIGHT = 200;

SDL_Surface *screen = NULL;
std::array<uint8_t, FRAMEBUFFER_WIDTH* FRAMEBUFFER_HEIGHT> framebuffer;
int          frame = 0;

struct rotation_lookup_table_entry_t {
	uint16_t unk0;
	uint16_t unk1;
	uint16_t fp_hi;
	uint16_t fp_lo;
};
static_assert(sizeof(rotation_lookup_table_entry_t) == 8, "wrong size");

constexpr int MAX_TILT = 98; // 200/2 - 2?
using globe_rotation_lookup_table_t = std::array<rotation_lookup_table_entry_t, MAX_TILT+1>; // MAX_TILT related see precalculate_globe_rotation_lookup_table
globe_rotation_lookup_table_t globe_rotation_lookup_table;
std::array<uint16_t, MAX_TILT*2> globe_tilt_lookup_table;

inline
uint16_t hi(uint32_t v) {
	return (v >> 16) & 0xffff;
}

inline
uint16_t lo(uint32_t v) {
	return (v >> 0) & 0xffff;
}

inline
int8_t hi(int16_t v) {
	return v >> 8;
}

inline
int8_t lo(int16_t v) {
	return v & 0xff;
}

/*
 *  globe_rotation is value from 0x0000 - 0xffff.
 *  Think of it as the fractional part of a 16.16 fixed point number.
 */
void precalculate_globe_rotation_lookup_table(uint16_t globe_rotation) {
	constexpr uint32_t MAGIC_VALUE = 398; // (100-1)*4?

	uint32_t dxax = MAGIC_VALUE * globe_rotation;

	// Floor the 16.16 fp value
	dxax &= ~0xffff;

	auto& first = globe_rotation_lookup_table[0];

	assert(first.unk0 == 0);
	assert(first.unk1 != 0);
	first.fp_hi = hi(dxax);
	first.fp_lo = lo(dxax);

	// Add 0.5 in 16.16 fixed point
	dxax += 0x8000;

	// Convert back to value from 0-1 in 16.16 fixed point
	uint16_t bx = dxax / MAGIC_VALUE;

	for (int i = 1; i != MAX_TILT+1; ++i) {
		auto& entry = globe_rotation_lookup_table[i];

		assert(entry.unk0 != 0); // meaning?
		uint32_t dxax = 2 * uint32_t(bx) * uint32_t(entry.unk1);
		entry.fp_hi = hi(dxax);
		entry.fp_lo = lo(dxax);
	}
}

inline 
uint16_t clamp(int16_t value, int16_t min, int16_t max)
{
	// with C++17: std::clamp
	if (value < min) {
		return min;
	}
	if (value > max) {
		return max;
	}
	return value;
}

void precalculate_globe_tilt_lookup_table(int16_t globe_tilt) {
	globe_tilt = clamp(globe_tilt, -MAX_TILT, MAX_TILT);

	int i = 0;

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
int frame_buffer_offset(int x, int y) {
	return y * FRAMEBUFFER_WIDTH + x;
}

inline
void draw_pixel(uint8_t* screen, uint32_t offset, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
	screen[offset + 0] = red;
	screen[offset + 1] = green;
	screen[offset + 2] = blue;
	screen[offset + 3] = alpha;
}

constexpr int MAGIC_200 = 200;  // FRAMEBUFFER_HEIGHT?

template<typename ValueType>
struct biggest_smallest_t
{
	ValueType smallest = std::numeric_limits<ValueType>::max();
	ValueType biggest = std::numeric_limits<ValueType>::min();

	void operator()(ValueType value)
	{
		if (value < smallest)
		{
			smallest = value;
			std::cout << "smallest: " << smallest << std::endl;
		}
		if (value > biggest)
		{
			biggest = value;
			std::cout << "biggest: " << biggest << std::endl;
		}
	}
};

#define WRITE_OUTPUT() (false)

struct writer_t
{
	std::string& output;

	writer_t(std::string& output_) :output(output_)
	{
	}
	~writer_t()
	{
		std::ofstream outfile("d:/temp/output.txt", std::ios_base::app);
		outfile << output;
	}
};

std::string output;

void draw_globe(uint8_t *framebuffer) {
	const uint8_t  *globdata = GLOBDATA_BIN;
	const uint8_t  *map      = MAP_BIN;

	int cs_1CA6 = 1;    // offset into globdata
	int cs_1CB4 = -FRAMEBUFFER_WIDTH; // screen width

	const int globe_center_xy_offset1 = frame_buffer_offset(160, 79);
	int cs_1CB0 = globe_center_xy_offset1;
	int cs_1CB2 = globe_center_xy_offset1;
	int cs_1CAE = globe_center_xy_offset1 - 1;

	bool drawing_southern_hemisphere = false;

#if WRITE_OUTPUT()
	output.reserve(1000 * 1024);
	writer_t w(output); // write on scope exit
#endif

	do {
		int di = cs_1CA6; // offset into globdata
		assert((di >= 1) && (di <= 2867));

		{
			const uint8_t globdata_at_di = globdata[di];
			assert(((globdata_at_di >= 0) && (globdata_at_di <= 86) || globdata_at_di == 255)); // 255(-1) as a flag?
		}

		int8_t gd_val = globdata[di++];

		if (gd_val < 0) { // as uint8_t == 255
			drawing_southern_hemisphere = true;

			cs_1CB4 = -cs_1CB4;
			if (cs_1CB4 < 0) {
				return;
			}

			const int globe_center_xy_offset = frame_buffer_offset(160, 80);
			cs_1CB0 = globe_center_xy_offset;
			cs_1CB2 = globe_center_xy_offset;
			cs_1CAE = globe_center_xy_offset - 1;

			assert(globdata[0] == 191); // as int8_t = -65

			di = 1 - int8_t(globdata[0]); // 1 - ( -65 ) = 66
			assert(di == 66);
			assert(globdata[di] == 1);

			gd_val = globdata[di++];
		}

		int si = 3290; // offset into globdata

		do {
			if (drawing_southern_hemisphere) {
				gd_val = -gd_val;
			}

			struct result_t
			{
				uint16_t gd{};
				int16_t grlt_0{};
				uint16_t grlt_1{};
				uint16_t grlt_2{};
			};

			auto func1 = [](const uint8_t* globdata_, const globe_rotation_lookup_table_t& rotation_lookup_table, const int16_t ofs1, const int base_ofs) {
				// 3290,3490,3690,3890,4090,4290,4490,4690,...,14690,14890,15090,15290,15490,15690,15890
				// start-offset(3290)+n*200
				assert((base_ofs >= 3290) && (base_ofs <= 15890) && ((base_ofs % 10) == 0));

				const int8_t lo_ofs1 = lo(ofs1);
				assert((lo_ofs1 >= -98) && (lo_ofs1 <= 98));

				const int offset1 = (lo_ofs1 < 0) ? -lo_ofs1 : lo_ofs1;
				assert((offset1 >= 0) && (offset1 <= 98)); // 1,2,3,4,5,9,11,19,28,39,51,67,74,86,94,97,98

				const uint8_t* sub_globdata = &globdata_[base_ofs + offset1];
				//printf("&globdata_[%i + %i] == &globdata_[%u]\n", base_ofs, offset1, sub_globdata - globdata_);
				//output += std::to_string(base_ofs + offset1) + "\n";

				// sub_globdata contains sizeof(uint16_t)-offsets to the entry but we need a logical index to our entry wich is 4*uint16_t
				const uint8_t index_from_gd1 = sub_globdata[0];
				//0,2,4,6,...,190,192,194,196 -> does not fit into int8_t
				assert((index_from_gd1 >= 0) && (index_from_gd1 <= 196) && ((index_from_gd1 % 2) == 0));

				const auto& entry = globe_rotation_lookup_table[index_from_gd1 / 2];
				assert((entry.unk0 >= 0) && (entry.unk0 <= 25334)); // signed: -25334 ... +25334
				assert((entry.unk1 >= 3) && (entry.unk1 <= 199));
				assert((entry.fp_hi >= 0) && (entry.fp_hi <= 397)); // 0,1,2,3,4,...,397

				// ofs1 < 0 == hi & lo < 0?
				const int16_t grlt_0 = (ofs1 < 0) ? -entry.unk0 : entry.unk0;
				//with uint16_t
				//assert((grlt_0 <= 65138) && ((grlt_0 % 2) == 0));
				//with int16_t
				assert((grlt_0 >= -25334) && (grlt_0 <= 25334) && ((grlt_0 % 2) == 0));

				const uint16_t grlt_2 = entry.fp_hi;

				const uint8_t index_from_gd2 = sub_globdata[MAGIC_200 / 2];
				//1,2,3,4,...,97,98,99 -> fits into int8_t
				assert(index_from_gd2 >= 0 && index_from_gd2 <= 99);

				// (lo_ofs1 < 0) ? entry.unk1(3-199) - index_from_gd2(0..99) : index_from_gd2(0..99)
				const uint16_t gd = (lo_ofs1 < 0) ? entry.unk1 - index_from_gd2 : index_from_gd2;

				const uint16_t grlt_1 = entry.unk1 * 2;
				assert((grlt_1 >= 6) && (grlt_1 <= 398) && ((grlt_1 % 2) == 0));

				return result_t{ gd, grlt_0, grlt_1, grlt_2 };
			};

			auto some_offset = [](int16_t value, int16_t adjust1, int16_t adjust2) {
				if (value < 0) {
					value += adjust1;
				}
				value += adjust2;
				return int(value);
			};

			auto pixel_color = [](uint8_t value) {
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

			// hi,lo int8 values?
			const uint16_t some_value = globe_tilt_lookup_table[MAX_TILT + gd_val];
			const result_t res = func1(globdata, globe_rotation_lookup_table, some_value, si);

			constexpr int MAGIC_OFS1 = 0x62FC; // middle?
			const uint8_t* sub_map = &map[MAGIC_OFS1];

			// left part of the globe
			{
				const int ofs1 = some_offset(
					res.grlt_2 - res.gd,
					res.grlt_1,
					res.grlt_0);

				assert((ofs1 >= -25334) && (ofs1 <= 25339));

				const uint8_t col1 = pixel_color(sub_map[ofs1]);
				framebuffer[cs_1CAE--] = col1;
			}

			// right part of the globe
			{
				const int ofs2 = some_offset(
					res.grlt_2 + res.gd - res.grlt_1,
					res.grlt_1,
					res.grlt_0);

				assert((ofs2 >= -25334) && (ofs2 <= 25339));

				const uint8_t col2 = pixel_color(sub_map[ofs2]);
				framebuffer[cs_1CB0++] = col2;
			}

			si += MAGIC_200;

			gd_val = globdata[di++];
			// al = ax & 0x00ff;
		} while (gd_val >= 0);

		assert((di >= 66) && (di <= 2867));

		cs_1CA6 = di;
		cs_1CB2 += cs_1CB4;
		cs_1CB0 = cs_1CB2;
		cs_1CAE = cs_1CB2 - 1;
	} while (true);
}

void init_globe_rotation_lookup_table() {
	const rotation_lookup_table_entry_t* tablat_entries = reinterpret_cast<const rotation_lookup_table_entry_t*>(&TABLAT_BIN);

	for (int i = 0; i != globe_rotation_lookup_table.size(); i++) {
		globe_rotation_lookup_table[i] = tablat_entries[i];
	}
}

// to show that the table data is not changing over the time
#define ALWAYS_INIT() (true)

struct draw_params_t {
	int16_t  tilt;
	uint16_t rotation;
};

struct rgb_t
{
	uint8_t r{};
	uint8_t g{};
	uint8_t b{};
};

inline
std::array<uint8_t, 3> pal_color(int color_index)
{
	const uint8_t* triple = &PAL_BIN[color_index * 3];
	return { triple[0], triple[1], triple[2] };
}

#define COMPARE_WITH_INITAL_CODE() (false)

#if COMPARE_WITH_INITAL_CODE()
namespace initial_port
{
	void draw_frame(int16_t tilt, int16_t rotation, uint8_t* framebuffer);
}

std::array<uint8_t, FRAMEBUFFER_WIDTH* FRAMEBUFFER_HEIGHT> test_framebuffer{};
#endif

void draw_frame(void *draw_params) {
	if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);

	auto* dp = reinterpret_cast<draw_params_t*>(draw_params);
	int16_t  tilt     = dp->tilt;
	uint16_t rotation = dp->rotation;

#if ALWAYS_INIT()
	init_globe_rotation_lookup_table();
#endif
	precalculate_globe_rotation_lookup_table(rotation);

	precalculate_globe_tilt_lookup_table(tilt);

	draw_globe(framebuffer.data());

#if COMPARE_WITH_INITAL_CODE()
	initial_port::draw_frame(tilt, rotation, test_framebuffer.data());
	if (framebuffer != test_framebuffer)
	{
		assert(false);
		printf("framebuffer != test_framebuffer rotation=%u, tilt=%i\n", rotation, tilt);
		throw 0xdeadbeef;
	}
#endif
	
	uint8_t *screenbuffer = (uint8_t*)screen->pixels;

	for (int i = 0; i != framebuffer.size(); ++i) {
		int color_index = framebuffer[i];

		const auto color = pal_color(color_index);

#ifdef _WIN32
		const rgb_t rgb{ color[2], color[1], color[0]};
#else
		const rgb_t rgb{ color[0], color[1], color[2] };
#endif

#if 1
		unsigned x = i / FRAMEBUFFER_WIDTH;
		unsigned y = i % FRAMEBUFFER_WIDTH;
		for (unsigned w = 0; w < resolution_factor; ++w) {
			for (unsigned h = 0; h < resolution_factor; ++h) {
				const unsigned pixel_offset = ((x * resolution_factor + w)
					                           * (FRAMEBUFFER_WIDTH * resolution_factor)
					                           + (y * resolution_factor + h)) * 4;
				draw_pixel(screenbuffer, pixel_offset, rgb.r, rgb.g, rgb.b, 255);
			}
		}
#else
		const unsigned pixel_offset = 4 * i;
        draw_pixel(screenbuffer, pixel_offset, rgb.r, rgb.g, rgb.b, 255);
#endif
	}

	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

	SDL_Flip(screen);
}

struct pos_t {
	int16_t tilt{};
	int16_t rotation{};
};

struct animated_t {
	int frame{0};

	pos_t next() {
		float f = sinf(frame / 200.0f);
		int16_t tilt = -MAX_TILT * f;
		int16_t rotation = 150 * frame;

		frame++;

		return { tilt, rotation };
	}
};

extern "C"
int main() {
	SDL_Init(SDL_INIT_VIDEO);

#if !ALWAYS_INIT()
	init_globe_rotation_lookup_table();
#endif

	screen = SDL_SetVideoMode(FRAMEBUFFER_WIDTH*resolution_factor, FRAMEBUFFER_HEIGHT*resolution_factor, 32, SDL_SWSURFACE);

#ifdef TEST_SDL_LOCK_OPTS
	EM_ASM("SDL.defaults.copyOnLock = false; SDL.defaults.discardOnLock = true; SDL.defaults.opaqueFrontBuffer = false;");
#endif

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop_arg(draw_frame, NULL, -1, 1);
#else
	bool run = true;

	bool is_animated = false;
	animated_t animated;

	pos_t cursor_based;

	while (run) {
		SDL_PumpEvents();
		Uint8* keystate = SDL_GetKeyState(NULL);

		constexpr int16_t ROTATION_STEP = 100;

		//continuous-response keys
		if (keystate[SDLK_LEFT]) {
			cursor_based.rotation += ROTATION_STEP;
		}
		if (keystate[SDLK_RIGHT]) {
			cursor_based.rotation -= ROTATION_STEP;
		}
		if (keystate[SDLK_UP] && cursor_based.tilt < MAX_TILT - 1){
			++cursor_based.tilt;
		}
		if (keystate[SDLK_DOWN] && cursor_based.tilt > -MAX_TILT - 1) {
			--cursor_based.tilt;
		}

		SDL_Event event;
		while (SDL_PollEvent(&event)) {  // poll until all events are handled!
			if (event.type == SDL_QUIT) {
				run = false;
				break;
			}
			if (event.type == SDL_KEYDOWN) {
				//Set the proper message surface
				switch (event.key.keysym.sym) {
					case SDLK_a:
						is_animated = !is_animated;
						break;
				}
			}
		}

		if (is_animated) {
			cursor_based = animated.next();
		} else {
			animated.frame = cursor_based.rotation / 150;
		}

		draw_params_t dp{ cursor_based.tilt , cursor_based.rotation };
		draw_frame(&dp);

		//SDL_Delay(10);
	}
#endif
	SDL_Quit();

	return 0;
}
