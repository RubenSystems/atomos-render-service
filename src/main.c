#include "def/atomui_multitouch.h"
#include "def/atomui_drm.h"


#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


struct atomui_multitouch_info multitouch_info; 
struct drm_mode_crtc saved_crtc;


int main(int argc, char **argv) {
	printf("[ATOMUI] - STARTING\n");

	struct drm_mode_card_res resource;
	struct drm_mode_get_connector connector; 
	struct drm_mode_modeinfo mode;

	int fd = atomui_open("/dev/dri/card0");

	if (atomui_get_resources(fd, &resource)) {
		printf("[ATOMUI] - Failed to open card0 resources\n");
		return -1;
	}

	struct atomui_size required_size = {
		.width = (argc == 3) ? atoi(argv[1]) : 0,
		.height = (argc == 3) ? atoi(argv[2]) : 0
	};

	struct atomui_data data = {
		.cleanup = false,
		.pflip_pending = false,
		.front_buf = 0,
		.size = {
			.width = required_size.width,
			.height = required_size.height,
		},
		.fd = fd
	};

	ioctl(fd, DRM_IOCTL_SET_MASTER, 0);

	int m_get = atomui_get_modes(fd, required_size, &resource, &connector, &mode);
	free_drm_mode_card_res(&resource); 
	if (m_get) {	
		return set_mode(&data, connector, mode);
	} else {
		ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
		return 0;
	}
}


int cursor_size = 5;
uint32_t bg_color = 0xFFFFFFFF ;//AARRGGBB
uint32_t cursor_color = 0xFF333333;


static void draw_data(int fd, struct atomui_data *data) {
	struct atomui_buffer *buf = &data->framebuffer[data->front_buf ^ 1];

	uint32_t * p = (uint32_t *)buf->map;
	
	//clear background.
	for (int i = 0; i < area_from_size(buf->size) / 4; i ++) {
		p[i] = bg_color;
	}

	int start_x, start_y;
	for (int finger = 0; finger < sizeof(multitouch_info.touch_events) / sizeof(struct atomui_touch_event); finger ++) {
		if (!multitouch_info.touch_events[finger].active) {
			continue;
		}
		start_x = multitouch_info.touch_events[finger].x;
		start_y = multitouch_info.touch_events[finger].y;
		for (int x = 0; x < cursor_size; x++) {
			for (int y = 0; y < cursor_size; y++) {
				int pos = (start_x + x) + ((start_y + y) * data->size.width);

				if (pos * 4 >= area_from_size(buf->size)) {
					break;
				}
				p[pos] = cursor_color;
			}
		}
	}

	struct drm_mode_crtc_page_flip flip;
	flip.fb_id = buf->fb;
	flip.crtc_id = data->crt_id;
	flip.user_data = (uint64_t)(uintptr_t)data;
	flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
	flip.reserved = 0;

	// Send frame to be rendered
	int ret = atomui_ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip);

	if (!ret) {
		data->pflip_pending = true;
		data->front_buf ^= 1;
	} else {
		printf("Failed to flip: %d\n", ret);
	}
}

static void request_render(int fd, uint32_t frame, uint32_t sec, uint32_t usec, void *data) {
	struct atomui_data *dev = data;
	dev->pflip_pending = false;

	if (!dev->cleanup) {
		draw_data(fd, dev);
	}
}




int set_mode(struct atomui_data * data, struct drm_mode_get_connector conn, struct drm_mode_modeinfo mode) {

	// Present
	struct atomui_size mode_size = {
		.width = mode.hdisplay,
		.height = mode.vdisplay 
	};
	struct atomui_render_context ev = { 
		.render_handler = request_render 
	};

	// yet to come 
	struct drm_mode_get_encoder enc;
	struct drm_mode_crtc crtc;

	// Check for issues
	if (
		!conn.encoder_id												||
		atomui_get_encoder(data->fd, conn.encoder_id, &enc)			 	|| 
		!enc.crtc_id 													||
		!atuomui_create_dual_framebuffer(data, mode_size)
	) {
		// Specificity is what is important
		printf("[ATOMUI] - FAILURE IN: connection_encoder id, getting encoder, no crtc id for encoder or in creating a dual framebuffer");
		return -1;
	}


	// Set stuff up
	int multitouch_fd = open("/dev/input/event0", O_RDONLY);
	atomui_init_multitouch(&multitouch_info, mode_size);

	
	data->crt_id = enc.crtc_id;

	memset(&crtc, 0, sizeof(crtc));
	memmove(&crtc.mode, &mode, sizeof(mode));
	crtc.crtc_id = enc.crtc_id;
	crtc.x = 0;
	crtc.y = 0;
	crtc.fb_id = data->framebuffer[0].fb;
	crtc.count_connectors = 1;
	crtc.set_connectors_ptr = (uint64_t)(uintptr_t)&conn.connector_id;
	crtc.mode_valid = 1;

	if (atomui_ioctl(data->fd, DRM_IOCTL_MODE_SETCRTC, &crtc)) {
		printf("FAILED TO SET CRTC");
		return -1;
	}


	draw_data(data->fd, data);

	fd_set fds;
	FD_ZERO(&fds);

	char event_buffer[ATOMUI_SMALL_BUF_SIZE];
	struct input_event multitouch_event_buffer [64];

	while(true) {
		FD_SET(0, &fds);
		FD_SET(data->fd, &fds);
		FD_SET(multitouch_fd, &fds);

		if (select(multitouch_fd + 1, &fds, NULL, NULL, NULL) < 0 || FD_ISSET(0, &fds)) {
			break;
		}

		if (FD_ISSET(data->fd, &fds)) {
			// request a render
			atomui_handle_event(data->fd, &ev, event_buffer);
		}

		if (FD_ISSET(multitouch_fd, &fds)) {

			int byte_count = read(multitouch_fd, multitouch_event_buffer, sizeof(struct input_event) * 64);
			atomui_handle_multitouch_event(&multitouch_info, multitouch_event_buffer, byte_count);

		}
	}

	// Cleanup
	free_framebuffer(&data->framebuffer[0]);
	free_framebuffer(&data->framebuffer[1]);
	free_drm_mode_get_connector(&conn);
	close(data->fd);

	return 0; // this line of code returns zero
}
