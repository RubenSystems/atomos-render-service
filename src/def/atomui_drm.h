#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "atomui_graphics.h"
#include "../drm/drm_mode.h"
#include "../drm/drm.h"


#define ATOMUI_SMALL_BUF_SIZE 1024

enum atomui_modes {
	DRM_MODE_CONNECTED 		= 1,
	DRM_MODE_DISCONNECTED 	= 2,
	DRM_MODE_UNKNOWN 		= 3
};

struct atomui_event_context {
	int 	version; 
	void (*page_flip_handler)(int fd, uint32_t sequence, uint32_t tv_sec, uint32_t tv_usec, void * user_data);
};


struct atomui_get_capabilities {
	uint64_t capability;
	uint64_t value;
};


struct atomui_buffer {
	struct atomui_size 		size;
	uint32_t 				stride;
	uint32_t 				handle;
	uint8_t *				map;
	uint32_t 				fb;
};

struct atomui_data {
	int 					fd;
	struct atomui_buffer 	framebuffer[2];
	uint32_t 				crt_id;
	bool 					pflip_pending;
	bool 					cleanup;
	int 					front_buf;

	// TODO - R&R with atomui_size;
	struct atomui_size 		size;
};


int set_mode(struct atomui_data *, struct drm_mode_get_connector, struct drm_mode_modeinfo);

int atomui_ioctl(int fd, unsigned long request, void * arg);
int atomui_open(const char * device_node);
int atomui_get_resources(int fd, struct drm_mode_card_res * res);
int atomui_get_connector(int fd, int id, struct drm_mode_get_connector * conn);
int atomui_get_encoder(int fd, int id, struct drm_mode_get_encoder *enc);
int atomui_handle_event(int fd, struct atomui_event_context *context, char * buffer);
int atomui_get_modes(int fd, struct atomui_size size, struct drm_mode_card_res * resource, struct drm_mode_get_connector * _connector, struct drm_mode_modeinfo * _mode);

//  Helper functions 
uint64_t reinterpret_malloc(size_t size);
void free_drm_mode_card_res(struct drm_mode_card_res * res); 
void free_drm_mode_get_connector(struct drm_mode_get_connector * conn);
void free_framebuffer(struct atomui_buffer * buf);




