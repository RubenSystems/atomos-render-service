#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../drm/drm_mode.h"
#include "../drm/drm.h"


enum atomui_modes {
	DRM_MODE_CONNECTED 		= 1,
	DRM_MODE_DISCONNECTED 	= 2,
	DRM_MODE_UNKNOWN 		= 3
};

struct atomui_event_context {
	int 	version; 
	void (*page_flip_handler)(int fd, uint32_t sequence, uint32_t tv_sec, uint32_t tv_usec, void *user_data);
};


struct atomui_get_capabilities {
	uint64_t capability;
	uint64_t value;
};


struct atomui_buffer {
	uint32_t 	width;
	uint32_t 	height;
	uint32_t 	stride;
	uint32_t 	size;
	uint32_t 	handle;
	uint8_t *	map;
	uint32_t 	fb;
};

struct atomui_data {
	int 					fd;
	struct atomui_buffer 	framebuffer[2];
	uint32_t 				crt_id;
	bool 					pflip_pending;
	bool 					cleanup;
	int 					front_buf;
	uint32_t 				width;
	uint32_t 				height;
};


int set_mode(struct atomui_data *, struct drm_mode_get_connector, struct drm_mode_modeinfo);

int atomui_ioctl(int fd, unsigned long request, void * arg);
int atomui_open(const char * device_node);
int atomui_get_resources(int fd, struct drm_mode_card_res * res);
int atomui_get_connector(int fd, int id, struct drm_mode_get_connector * conn);
int atomui_get_encoder(int fd, int id, struct drm_mode_get_encoder *enc);
int atomui_get_modes(uint32_t width, uint32_t height);

//  Helper functions 
uint64_t reinterpret_malloc(size_t size);
void free_drm_mode_card_res(struct drm_mode_card_res * res); 
void free_drm_mode_get_connector(struct drm_mode_get_connector * res);




