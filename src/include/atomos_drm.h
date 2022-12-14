#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../drm/drm_mode.h"
#include "../drm/drm.h"

struct atomos_get_cap {
	uint64_t capability;
	uint64_t value;
};

enum atomos_modes {
	DRM_MODE_CONNECTED 		= 1,
	DRM_MODE_DISCONNECTED 	= 2,
	DRM_MODE_UNKNOWN 		= 3
};

struct atomos_event_context {
	int 	version; 
	void (*vblank_handler)(int fd, uint32_t sequence, uint32_t tv_sec, uint32_t tv_usec, void *user_data);
	void (*page_flip_handler)(int fd, uint32_t sequence, uint32_t tv_sec, uint32_t tv_usec, void *user_data);
};


struct atomos_buffer {
	uint32_t 	width;
	uint32_t 	height;
	uint32_t 	stride;
	uint32_t 	size;
	uint32_t 	handle;
	uint8_t *	map;
	uint32_t 	fb;
};

struct atomos_data {
	int 					fd;
	struct atomos_buffer 	framebuffer[2];
	uint32_t 				crt_id;
	bool 					pflip_pending;
	bool 					cleanup;
	int 					front_buf;
	uint32_t 				width;
	uint32_t 				height;
};


int set_mode(struct atomos_data *, struct drm_mode_get_connector, struct drm_mode_modeinfo);

int atomos_ioctl(int fd, unsigned long request, void * arg);
int atomos_open(const char * device_node);
int atomos_get_resources(int fd, struct drm_mode_card_res * res);
int atomos_get_connector(int fd, int id, struct drm_mode_get_connector * conn);
int atomos_get_encoder(int fd, int id, struct drm_mode_get_encoder *enc);

//  Helper functions 
void free_drm_mode_card_res(struct drm_mode_card_res * res); 
void free_drm_mode_get_connector(struct drm_mode_get_connector * res);




