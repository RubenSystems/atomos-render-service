#include "../def/atomui_graphics.h"

uint32_t area_from_size(struct atomui_size size) {

	// Multiply by four becuase of alpha, red, green and blue channels
	return size.width * size.height * 4;
}