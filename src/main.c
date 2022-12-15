#include "def/atomui_multitouch.h"
#include "def/atomui_drm.h"

#include <linux/input.h>
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
	printf("[ATOMUI] - MODES:\n");

	int fd = atomui_open("/dev/dri/card0");
	struct drm_mode_card_res resource;

	if (atomui_get_resources(fd, &resource)) {
		printf("[ATOMUI] - Failed to open card0 resources\n");
		return -1;
	}

	// Set the hres and vres to the cmdline args. If unset, 0
	struct atomui_size required_size = {
		.width = (argc == 3) ? atoi(argv[1]) : 0,
		.height = (argc == 3) ? atoi(argv[2]) : 0
	};


	struct atomui_data data = {
		.cleanup = false,
		.pflip_pending = false,
		.front_buf = 0,
		.width = required_size.width,
		.height = required_size.height,
		.fd = fd
	};

	struct drm_mode_get_connector connector; 
	struct drm_mode_modeinfo mode;

	ioctl(fd, DRM_IOCTL_SET_MASTER, 0);

	printf("DRM Connectors: %d\n", resource.count_connectors);

	int m_get = atomui_get_modes(fd, required_size, &resource, &connector, &mode);
	free_drm_mode_card_res(&resource); 
	if (m_get) {	
		// WWWAAARRRNNNIINNNGGGGGGGG :: connector is memleaked free it
		return set_mode(&data, connector, mode);
	} else {
		ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
		return 0;
	}


	// for (int i = 0; i < resource.count_connectors; i++) {
	// 	uint32_t *connectors = (uint32_t *)resource.connector_id_ptr;

	// 	struct drm_mode_get_connector conn;
	// 	int ret = atomui_get_connector(fd, connectors[i], &conn);

	// 	if (ret) {
	// 		printf("\tFailed to get connector: %d\n", ret);
	// 		continue;
	// 	}

	// 	//printf("Found Connector: %d - %d.  Modes %d\n", i, connectors[i], conn.count_modes);

	// 	if (conn.connection != DRM_MODE_CONNECTED) {
	// 		//printf("\tIgnoring unconnected connector. (%d)\n", conn.connection);
	// 		continue;
	// 	}

	// 	struct drm_mode_modeinfo *modes = (struct drm_mode_modeinfo *)conn.modes_ptr;

	// 	for (int m=0; m<conn.count_modes; m++) {
	// 		printf("\tMode: %dx%d\n", modes[m].hdisplay, modes[m].vdisplay);

	// 		//sleep_sec(1);

	// 		if (required_size.width == modes[m].hdisplay && required_size.height == modes[m].vdisplay) {
	// 			printf("HERE!!!!!"); 
	// 			return set_mode(&data, conn, modes[m]);
	// 		}
	// 	}
	// 	free_drm_mode_get_connector(&conn);
	// }

}

bool create_framebuffer(int fd, struct atomui_buffer *buf) {
	struct drm_mode_create_dumb creq;
	struct drm_mode_create_dumb dreq;
	struct drm_mode_map_dumb mreq;

	memset(&creq, 0, sizeof(creq));
	creq.width = buf->size.width;
	creq.height = buf->size.height;
	creq.bpp = 32;


	int ret = atomui_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);

	if (ret < 0) {
		printf("Failed to create buffer: %d\n", ret);
		return false;
	}

	buf->stride = creq.pitch;
	buf->handle = creq.handle;

	// TODO: - TRY OUTPUTING CREQ.SIZE and SEE IF IT IS THE SAME AS W * H
	// buf->size = creq.size;

	printf("%i\n\n", (unsigned int)creq.size);



	struct drm_mode_fb_cmd fbcmd;
	memset(&fbcmd, 0, sizeof(fbcmd));
	fbcmd.width = buf->size.width;
	fbcmd.height = buf->size.height;
	fbcmd.depth = 24;
	fbcmd.bpp = 32;
	fbcmd.pitch = buf->stride;
	fbcmd.handle = buf->handle;

	ret = atomui_ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fbcmd);

	if (ret < 0) {
		printf("Failed to add FB: %d\n", ret);
		return false;
	}

	buf->fb = fbcmd.fb_id;
	memset(&mreq, 0, sizeof(mreq));
	mreq.handle = buf->handle;

	ret = atomui_ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);

	if (ret) {
		printf("Failed to map FB: %d\n", ret);
		return false;
	}

	buf->map = mmap(0, area_from_size(buf->size), PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);

	if (((int64_t)buf->map) == -1) {
		printf("Failed to map FB!\n");
		return false;
	}

	memset(buf->map, 0, area_from_size(buf->size));

	return true;
}

int cursor_size = 20;
uint32_t bg_color = 0xFFFFFFFF ;//AARRGGBB
uint32_t cursor_color = 0xFF333333;


static void draw_data(int fd, struct atomui_data *data) {
	struct atomui_buffer *buf = &data->framebuffer[data->front_buf ^ 1];

	uint32_t *p = (uint32_t *)buf->map;
	
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
				int pos = (start_x + x) + ((start_y + y) * data->width);

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
	flip.user_data = (uint64_t)data;
	flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
	flip.reserved = 0;

	int ret = atomui_ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip);

	if (!ret) {
		data->pflip_pending = true;
		data->front_buf ^= 1;
	} else {
		printf("Failed to flip: %d\n", ret);
	}
}

static void page_flip_event(int fd, uint32_t frame, uint32_t sec, uint32_t usec, void *data) {
	struct atomui_data *dev = data;
	dev->pflip_pending = false;

	if (!dev->cleanup) {
		draw_data(fd, dev);
	}
}




int set_mode(struct atomui_data *data, struct drm_mode_get_connector conn, struct drm_mode_modeinfo mode) {
	if (!conn.encoder_id) {
		printf("No encoder found!\n");
		return -1;
	}

	struct drm_mode_get_encoder enc;
	int ret = 0;

	if (ret = atomui_get_encoder(data->fd, conn.encoder_id, &enc)) {
		printf("Encoder load failed: %d, %d - %d - %X\n", ret, data->fd, conn.encoder_id, &enc);
		return -1;
	}

	if (!enc.crtc_id) {
		printf("No CRT Controller!\n");
		return -1;
	}

	data->framebuffer[0].size.width = mode.hdisplay;
	data->framebuffer[0].size.height = mode.vdisplay;
	data->framebuffer[1].size.width = mode.hdisplay;
	data->framebuffer[1].size.height = mode.vdisplay;



	memset(&multitouch_info.touch_events, 0, sizeof(multitouch_info.touch_events) / sizeof(struct atomui_touch_event));
	multitouch_info.max_x = mode.hdisplay;
	multitouch_info.max_y = mode.vdisplay;
	multitouch_info.current_touch_slot = 0;
	float finger_position_multiplier = mode.vdisplay / (float)mode.hdisplay;
	printf("%i %i %f\n", mode.vdisplay, mode.hdisplay, finger_position_multiplier);

	if (!create_framebuffer(data->fd, &data->framebuffer[0])) {
		printf("Failed to create framebuffer 1!\n");
		return -1;
	}

	if (!create_framebuffer(data->fd, &data->framebuffer[1])) {
		printf("Failed to create framebuffer 2!\n");
		return -1;
	}

	printf("Buffer created with size: %d\n", data->framebuffer[0].size);

	struct drm_mode_crtc crtc;
	memset(&crtc, 0, sizeof(crtc));
	crtc.crtc_id = enc.crtc_id;

	data->crt_id = enc.crtc_id;

	//get the current CRTC, should be FB controller.
	ret = atomui_ioctl(data->fd, DRM_IOCTL_MODE_GETCRTC, &crtc);
	saved_crtc = crtc;

	printf("Get CRTC: %d = %d (%d, %d, %x, %s)\n", crtc.crtc_id, ret, crtc.fb_id, crtc.count_connectors, crtc.set_connectors_ptr, crtc.mode.name);

	memset(&crtc, 0, sizeof(crtc));
	crtc.crtc_id = enc.crtc_id;

	memmove(&crtc.mode, &mode, sizeof(mode));
	crtc.x = 0;
	crtc.y = 0;
	crtc.fb_id = data->framebuffer[0].fb;
	crtc.count_connectors = 1;
	crtc.set_connectors_ptr = (uint64_t)&conn.connector_id;
	crtc.mode_valid = 1;

	int multitouch_fd = open("/dev/input/event0", O_RDONLY);

	printf("CRTC RES: %d/%d...\n", crtc.mode.hdisplay, crtc.mode.vdisplay);

	// sleep(4);

	//about to set mode...
	ret = atomui_ioctl(data->fd, DRM_IOCTL_MODE_SETCRTC, &crtc);

	if (ret) {
		printf("FAILED TO SET CRTC! %d\n", ret);
		return ret;
	}


	draw_data(data->fd, data);

	fd_set fds;
	FD_ZERO(&fds);

	struct atomui_event_context ev;
	memset(&ev, 0, sizeof(ev));
	ev.version = 2;
	ev.page_flip_handler = page_flip_event;

	while(true) {
		FD_SET(0, &fds);
		FD_SET(data->fd, &fds);
		FD_SET(multitouch_fd, &fds);

		ret = select(multitouch_fd + 1, &fds, NULL, NULL, NULL);

		if (ret < 0) {
			printf("SELECT FAILED! %d\n", ret);
			break;
		}

		if (FD_ISSET(0, &fds)) {
			//user pressed key...

			break;
		}

		if (FD_ISSET(data->fd, &fds)) {
			//drawing happened on the buffer...
			atomui_handle_event(data->fd, &ev);
		}

		if (FD_ISSET(multitouch_fd, &fds)) {

			struct input_event events [64];
			uint8_t event_slot = 0;
			int byte_count = read(multitouch_fd, events, sizeof(struct input_event) * 64);
			

			if (byte_count < sizeof(struct input_event)) {
				continue;
			}
			uint8_t number_of_events = byte_count / sizeof(struct input_event);

			for (uint8_t event_index = 0; event_index < number_of_events; event_index ++) {
				struct input_event * event = &(events[event_index]);
				switch (event->type) {
				case EV_SYN:
					break;
				case EV_ABS: 
					{
						switch (event->code) {
						case ABS_MT_SLOT:
							multitouch_info.current_touch_slot = event->value;
							break;
						case ABS_MT_POSITION_X:
							multitouch_info.touch_events[multitouch_info.current_touch_slot].x = event->value / finger_position_multiplier;
							break;
						case ABS_MT_POSITION_Y:
							multitouch_info.touch_events[multitouch_info.current_touch_slot].y = event->value * finger_position_multiplier;
							break;
						case ABS_MT_TOUCH_MAJOR:

							break;
						case ABS_MT_TRACKING_ID:

							if (event->value == -1) {
								multitouch_info.touch_events[multitouch_info.current_touch_slot].active = false;
							} else {
								multitouch_info.touch_events[multitouch_info.current_touch_slot].active = true;
							}

							break;

						}
					}
					break;
				}
			}
		}
	}

	//after loop, restore original CRTC...

	saved_crtc.count_connectors = 1;
	saved_crtc.mode_valid = 1;
	saved_crtc.set_connectors_ptr = (uint64_t)&conn.connector_id;

	ret = atomui_ioctl(data->fd, DRM_IOCTL_MODE_SETCRTC, &saved_crtc);

	ioctl(data->fd, DRM_IOCTL_DROP_MASTER, 0);

	close(data->fd);

	return 0;
}
