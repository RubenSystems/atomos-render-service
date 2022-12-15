#include "../def/atomui_drm.h"
#include "../def/atomui_drm.h"

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>


#ifndef O_CLOEXEC
#define O_CLOEXEC   02000000
#endif


int atomui_ioctl(int fd, unsigned long request, void *arg) {
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while(ret == -EINTR || ret == -EAGAIN);

	return ret;
}

int atomui_open(const char *device_node) {
	int fd = open((char *)device_node, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		return fd;
	}

	struct atomui_get_capabilities get_cap = {
		.capability = DRM_CAP_DUMB_BUFFER,
		.value = 0
	};

	if (ioctl(fd, DRM_IOCTL_GET_CAP, &get_cap) < 0 || !get_cap.value) {
		return -EOPNOTSUPP;
	}

	return fd;
}

int atomui_get_resources(int fd, struct drm_mode_card_res *res) {
	memset(res, 0, sizeof(struct drm_mode_card_res));


	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, res)) {
		return -1;
	}

	if (res->count_fbs) {
		res->fb_id_ptr = reinterpret_malloc(res->count_fbs * sizeof(uint32_t));
		memset((void *)res->fb_id_ptr, 0, res->count_fbs * sizeof(uint32_t));
	}

	if (res->count_crtcs) {
		res->crtc_id_ptr = reinterpret_malloc(res->count_crtcs * sizeof(uint32_t));
		memset((void *)res->crtc_id_ptr, 0, res->count_crtcs * sizeof(uint32_t));
	}

	if (res->count_connectors) {
		res->connector_id_ptr = reinterpret_malloc(res->count_connectors * sizeof(uint32_t));
		memset((void *)res->connector_id_ptr, 0, res->count_connectors * sizeof(uint32_t));
	}

	if (res->count_encoders) {
		res->encoder_id_ptr = reinterpret_malloc(res->count_encoders * sizeof(uint32_t));
		memset((void *)res->encoder_id_ptr, 0, res->count_encoders * sizeof(uint32_t));
	}

	return ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, res) ? -1 : 0;
}

int atomui_get_connector(int fd, int id, struct drm_mode_get_connector *conn) {
	memset(conn, 0, sizeof(struct drm_mode_get_connector));
	conn->connector_id = id;

	if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn)) {
		return -1;
	}


	if (conn->count_props) {
		conn->props_ptr = reinterpret_malloc(conn->count_props * sizeof(uint32_t));
		conn->prop_values_ptr = reinterpret_malloc(conn->count_props * sizeof(uint64_t));
	}

	if (conn->count_modes) {
		conn->modes_ptr = reinterpret_malloc(conn->count_modes * sizeof(struct drm_mode_modeinfo));
	}

	if (conn->count_encoders) {
		conn->encoders_ptr = reinterpret_malloc(conn->count_encoders * sizeof(uint32_t));
	}

	return ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn) ? -1 : 0;
}


int atomui_get_encoder(int fd, int id, struct drm_mode_get_encoder *enc) {
	memset(enc, 0, sizeof(struct drm_mode_get_encoder));
	enc->encoder_id = id;
	return atomui_ioctl(fd, DRM_IOCTL_MODE_GETENCODER, enc) ? -1 : 0;
}


int atomui_handle_event(int fd, struct atomui_event_context *context) {
	char buffer[1024];
	struct drm_event *e;

	int len = read(fd, buffer, sizeof(buffer));

	if (!len) {
		return 0;
	} else if (len < sizeof(struct drm_event)) {
		return -1;
	}

	for (int i = 0; i < len; i += e->length) {
		e = (struct drm_event *)&buffer[i];

		if (e->type == DRM_EVENT_FLIP_COMPLETE) {
			struct drm_event_vblank *vb = (struct drm_event_vblank *)e;
			context->page_flip_handler(fd, vb->sequence, vb->tv_sec, vb->tv_usec, (void *)vb->user_data);
			break;
		}
	}

	return 0;
}

// int atomui_get_modes(uint32_t h_res, uint32_t v_res, struct atomui_data & data, struct) { 
	// printf("DRM modes:\n");

	// int fd = atomui_open("/dev/dri/card0");
	// struct drm_mode_card_res res;

	// if (atomui_get_resources(fd, &res)) {
	// 	printf("Failed to open card0 resources\n");
	// 	return -1;
	// }

	// struct atomui_data data = {
	// 	.cleanup = false;
	// 	.pflip_pending = false;
	// 	.front_buf = 0;
	// 	.width = hres;
	// 	.height = vres;
	// 	.fd = fd;
	// };
	
	// ioctl(fd, DRM_IOCTL_SET_MASTER, 0);

	// printf("DRM Connectors: %d\n", res.count_connectors);
	// // sleep(1);

	// for (int i = 0; i < res.count_connectors; i++) {
	// 	uint32_t *connectors = (uint32_t *)res.connector_id_ptr;

	// 	struct drm_mode_get_connector conn;
	// 	int ret = atomui_get_connector(fd, connectors[i], &conn);

	// 	if (ret) {
	// 		printf("\tFailed to get connector: %d\n", ret);
	// 		continue;
	// 	} else if (conn.connection != DRM_MODE_CONNECTED) {
	// 		//printf("\tIgnoring unconnected connector. (%d)\n", conn.connection);
	// 		continue;
	// 	}

	// 	struct drm_mode_modeinfo *modes = (struct drm_mode_modeinfo *)conn.modes_ptr;

	// 	for (int m=0; m<conn.count_modes; m++) {
	// 		printf("\tMode: %dx%d\n", modes[m].hdisplay, modes[m].vdisplay);

	// 		//sleep_sec(1);

	// 		if (hres == modes[m].hdisplay && vres == modes[m].vdisplay) {
	// 			printf("HERE!!!!!"); 
	// 			return set_mode(&data, conn, modes[m]);
	// 		}
	// 	}
	// 	free_drm_mode_get_connector(&conn);
	// }

	// ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);

	// free_drm_mode_card_res(&res); 
	// return 0;

// }


//  Helper Impl

/*
	Create a pointer and cast to a 64 bit uint 
*/ 
uint64_t reinterpret_malloc(size_t size) {
	return (uint64_t)(uintptr_t)malloc(size);
}

/*
	Free functions for structs with pointers
*/
void free_drm_mode_card_res(struct drm_mode_card_res * res) {
	free((uint32_t *)res->fb_id_ptr);
	free((uint32_t *)res->crtc_id_ptr);
	free((uint32_t *)res->connector_id_ptr);
	free((uint32_t *)res->encoder_id_ptr);
}

void free_drm_mode_get_connector(struct drm_mode_get_connector * conn) {
	free((uint32_t *)conn->encoders_ptr);
	free((struct drm_mode_modeinfo *)conn->modes_ptr);
	free((uint32_t *)conn->props_ptr);
	free((uint64_t *)conn->prop_values_ptr);
} 
