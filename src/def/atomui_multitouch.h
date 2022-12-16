#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <linux/input.h>

#include "atomui_graphics.h"

struct atomui_touch_event { 
	bool 		active; 
	uint32_t 	x; 
	uint32_t 	y; 
};

/* 
	An int value for the average number of fingers humans have => (int)9.99999994
	Although it is technically 8 fingers and two thumbs.
*/ 
#define MAX_FINGERS 10 
struct atomui_multitouch_info {
	struct atomui_touch_event	touch_events					[MAX_FINGERS];
	uint32_t					max_x; 
	uint32_t 					max_y;
	uint8_t						current_touch_slot;
	float 						touch_multiplier; 
};


void atomui_init_multitouch(struct atomui_multitouch_info *, struct atomui_size display_size); 

void atomui_handle_multitouch_event(struct atomui_multitouch_info * info, struct input_event * event_buffer, int buffer_size);