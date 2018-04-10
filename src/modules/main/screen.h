#pragma once
#include <stddef.h>
#include <stdint.h>

#include "util.h"

class screen
{
public:
	using pixel_t = uint32_t;

	screen(
		void *framebuffer,
		unsigned hres, unsigned vres,
		pixel_t rmask, pixel_t gmask, pixel_t bmask);

	screen(const screen &other);

	unsigned width() const { return m_resolution.x; }
	unsigned height() const { return m_resolution.y; }

	void fill(pixel_t color);
	void draw_pixel(unsigned x, unsigned y, pixel_t color);

	screen() = delete;

	struct color_masks {
		pixel_t r, g, b;
		color_masks(pixel_t r, pixel_t g, pixel_t b);
		color_masks(const color_masks &other);
	};

private:
	pixel_t *m_framebuffer;
	color_masks m_masks;
	coord<unsigned> m_resolution;
};
