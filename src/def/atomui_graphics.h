#pragma once

#include <stdint.h>

/*
	Quantity information
*/ 

// Point in space
struct atomui_point {
	uint32_t 			x; 
	uint32_t 			y; 
};


// Size 
struct atomui_size {
	uint32_t 			width; 
	uint32_t 			height;
};


/*
	Helper functionality
*/
uint32_t area_from_size(struct atomui_size size);