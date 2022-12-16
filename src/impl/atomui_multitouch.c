#include <string.h>


#include "../def/atomui_multitouch.h"



void atomui_init_multitouch(struct atomui_multitouch_info * info, struct atomui_size size) {
	memset(&info->touch_events, 0, sizeof(info->touch_events) / sizeof(struct atomui_touch_event));
	info->max_x = size.width;
	info->max_y = size.height;
	info->current_touch_slot = 0;
	info->touch_multiplier = size.height / (float)size.width;
}

void atomui_handle_multitouch_event(struct atomui_multitouch_info * info, struct input_event * event_buffer, int buffer_size) {

	if (buffer_size < sizeof(struct input_event)) {
		return;
	}
	
	uint8_t number_of_events = buffer_size / sizeof(struct input_event);

	for (uint8_t event_index = 0; event_index < number_of_events; event_index ++) {
		struct input_event * event = &(event_buffer[event_index]);
		switch (event->type) {
		case EV_SYN:
			break;
		case EV_ABS: 
			{
				switch (event->code) {
				case ABS_MT_SLOT:
					info->current_touch_slot = event->value;
					break;
				case ABS_MT_POSITION_X:
					info->touch_events[info->current_touch_slot].x = event->value / info->touch_multiplier;
					break;
				case ABS_MT_POSITION_Y:
					info->touch_events[info->current_touch_slot].y = event->value * info->touch_multiplier;
					break;
				case ABS_MT_TOUCH_MAJOR:

					break;
				case ABS_MT_TRACKING_ID:

					if (event->value == -1) {
						info->touch_events[info->current_touch_slot].active = false;
					} else {
						info->touch_events[info->current_touch_slot].active = true;
					}

					break;
				}
			}
			break;
		}
	}
}