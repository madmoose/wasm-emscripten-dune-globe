//===========================================================================
// DO NOT TOUCH THIS CODE - THIS IS THE REFERENCE IMPLEMENTATION FOR TESTING
// from git commit:
//   Date : 3 days ago(03.04.2022 10:07 : 42)
//   Commit hash : 1759417e841296c600a5c9057fefd13a93a36a3e
//===========================================================================

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cmath>

namespace initial_port
{

#include "GLOBDATA.BIN.inc"
#include "MAP.BIN.inc"
#include "PAL.BIN.inc"
#include "TABLAT.BIN.inc"

uint16_t     globe_rotation_lookup_table[396];
uint16_t     globe_tilt_lookup_table[196];

inline
int8_t uint8_as_int8(uint8_t u) {
	return u < INT8_MAX ? u : u - UINT8_MAX - 1;
}

inline
int16_t uint16_as_int16(uint16_t u) {
	return u < INT16_MAX ? u : u - UINT16_MAX - 1;
}

/*
 *  globe_rotation is value from 0x0000 - 0xffff.
 *  Think of it as the fractional part of a 16.16 fixed point number.
 */
void precalculate_globe_rotation_lookup_table(uint16_t globe_rotation) {
	uint16_t ax, cx, dx, bx;

	uint32_t dxax = 398 * globe_rotation;

	// Floor the 16.16 fp value
	dxax &= ~0xffff;

	globe_rotation_lookup_table[2] = (dxax >> 16) & 0xffff;
	globe_rotation_lookup_table[3] = (dxax >>  0) & 0xffff;

	// Add 0.5 in 16.16 fixed point
	dxax += 0x8000;

	// Convert back to value from 0-1 in 16.16 fixed point
	bx = dxax / 398;

	for (int i = 1; i != 99; ++i) {
		uint32_t dxax = 2 * uint32_t(bx) * uint32_t(globe_rotation_lookup_table[4 * i + 1]);
		globe_rotation_lookup_table[4 * i + 2] = (dxax >> 16) & 0xffff;
		globe_rotation_lookup_table[4 * i + 3] = (dxax >>  0) & 0xffff;
	}
}

void precalculate_globe_tilt_lookup_table(int16_t globe_tilt) {
	int i = 0;

	if (globe_tilt < -98) {
		globe_tilt = -98;
	}
	if (globe_tilt > 98) {
		globe_tilt = 98;
	}

	// For stage 1, count down from globe_tilt-99 to -98
	if (globe_tilt > 0) {
		int v = globe_tilt - 98;
		do {
			globe_tilt_lookup_table[i++] = uint8_t(--v);
		} while (i != 196 && v > -98);
	}

	if (i == 196) {
		return;
	}

	// For stage 2, count down from 98 to 0
	int v = globe_tilt + 98 - i;
	do {
		globe_tilt_lookup_table[i++] = uint8_t(v--);
	} while (i != 196 && v >= 0);

	if (i == 196) {
		return;
	}

	// For stage 3, count up from 1 to 98, binary-or'ed with 0xff00
	v = 1;
	do {
		globe_tilt_lookup_table[i++] = v++ | 0xff00;
	} while (i != 196 && v <= 98);

	if (i == 196) {
		return;
	}

	// For stage 4, count up from -98 to 0, binary-or'ed with 0xff00
	v = -98;
	do {
		globe_tilt_lookup_table[i++] = v++ | 0xff00;
	} while (i != 196 && v <= 0);
}

void draw_globe(uint8_t *framebuffer) {
	const uint8_t  *globdata = GLOBDATA_BIN;
	const uint8_t  *map      = MAP_BIN;

	uint16_t cs_1CA8 = 1;    // offset into globdata
	uint16_t cs_1CA6 = 1;    // offset into globdata
	uint16_t cs_1CAA = 3290; // offset into globdata
	uint16_t cs_1CAC = 98;   // offset into globe_tilt_lookup_table

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
			ax = globe_tilt_lookup_table[cs_1CAC + uint8_as_int8(ax & 0xff)];

			if (uint16_as_int16(ax) >= 0) {
				ax = uint8_as_int8(ax & 0xff);
				if (uint16_as_int16(ax) < 0) {
					ax = -ax;
					uint16_t bp = ax;
					bx = globdata[bp + si];
					ax = globdata[bp + si + 0x64];

					bp = bx;
					bx = globe_rotation_lookup_table[2 * bp + 0];
					cx = globe_rotation_lookup_table[2 * bp + 1];
					dx = globe_rotation_lookup_table[2 * bp + 2];

					ax = cx - ax;
				} else {
					uint16_t bp = ax;
					bx = globdata[bp + si];
					ax = globdata[bp + si + 0x64];

					bp = bx;
					bx = globe_rotation_lookup_table[2 * bp + 0];
					cx = globe_rotation_lookup_table[2 * bp + 1];
					dx = globe_rotation_lookup_table[2 * bp + 2];
				}
			} else {
				ax = uint8_as_int8(ax & 0xff);
				if (uint16_as_int16(ax) < 0) {
					ax = -ax;
					uint16_t bp = ax;
					bx = globdata[bp + si];
					ax = globdata[bp + si + 0x64];

					bp = bx;
					bx = globe_rotation_lookup_table[2 * bp + 0];
					cx = globe_rotation_lookup_table[2 * bp + 1];
					dx = globe_rotation_lookup_table[2 * bp + 2];

					ax = cx - ax;
					bx = -bx;
				} else {
					uint16_t bp = ax;
					bx = globdata[bp + si];
					ax = globdata[bp + si + 0x64];

					bp = bx;
					bx = globe_rotation_lookup_table[2 * bp + 0];
					cx = globe_rotation_lookup_table[2 * bp + 1];
					dx = globe_rotation_lookup_table[2 * bp + 2];

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

void draw_frame(int16_t tilt, int16_t rotation, uint8_t* framebuffer) {
	for (int i = 0; i != 396; i++) {
		uint16_t u = (TABLAT_BIN[2*i + 0] << 0) + (TABLAT_BIN[2*i+ 1 ] << 8);
		globe_rotation_lookup_table[i] = u;
	}

	precalculate_globe_rotation_lookup_table(rotation);
	precalculate_globe_tilt_lookup_table(tilt);

	draw_globe(framebuffer);
}

}