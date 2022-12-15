#include "../def/atomos_drm.h"
#include "../def/atomos_drm.h"
#include "../def/multitouch.h"

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


int atomos_ioctl(int fd, unsigned long request, void *arg) {
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while(ret == -EINTR || ret == -EAGAIN);

	return ret;
}


//opening a device such as /dev/dri/card0
int atomos_open(const char *device_node) {
	int fd = open((char *)device_node, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		return fd;
	}

	struct atomos_get_cap get_cap = {
		.capability = DRM_CAP_DUMB_BUFFER,
		.value = 0
	};

	if (ioctl(fd, DRM_IOCTL_GET_CAP, &get_cap) < 0) {
		return -EOPNOTSUPP;
	}

	return fd;
}

int atomos_get_resources(int fd, struct drm_mode_card_res *res) {
	memset(res, 0, sizeof(struct drm_mode_card_res));


	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, res)) {
		return -1;
	}

	// Only works with 64bit pointers
	// Check helper functions below for freeing logic 
	if (res->count_fbs) {
		res->fb_id_ptr = (uint64_t)malloc(res->count_fbs * sizeof(uint32_t));
		memset((void *)res->fb_id_ptr, 0, res->count_fbs * sizeof(uint32_t));
	}

	if (res->count_crtcs) {
		res->crtc_id_ptr = (uint64_t)malloc(res->count_crtcs * sizeof(uint32_t));
		memset((void *)res->crtc_id_ptr, 0, res->count_crtcs * sizeof(uint32_t));
	}

	if (res->count_connectors) {
		res->connector_id_ptr = (uint64_t)malloc(res->count_connectors * sizeof(uint32_t));
		memset((void *)res->connector_id_ptr, 0, res->count_connectors * sizeof(uint32_t));
	}

	if (res->count_encoders) {
		res->encoder_id_ptr = (uint64_t)malloc(res->count_encoders * sizeof(uint32_t));
		memset((void *)res->encoder_id_ptr, 0, res->count_encoders * sizeof(uint32_t));
	}

	int ior = ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, res);

	return ior ? -1 : 0;
}

int atomos_get_connector(int fd, int id, struct drm_mode_get_connector *conn) {
	memset(conn, 0, sizeof(struct drm_mode_get_connector));
	conn->connector_id = id;

	if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn)) {
		return -1;
	}


	if (conn->count_props) {
		conn->props_ptr = (uint64_t)malloc(conn->count_props * sizeof(uint32_t));
		conn->prop_values_ptr = (uint64_t)malloc(conn->count_props * sizeof(uint64_t));
	}

	if (conn->count_modes) {
		conn->modes_ptr = (uint64_t)malloc(conn->count_modes * sizeof(struct drm_mode_modeinfo));
	}

	if (conn->count_encoders) {
		conn->encoders_ptr = (uint64_t)malloc(conn->count_encoders * sizeof(uint32_t));
	}

	return ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, conn) ? -1 : 0;
}


int atomos_get_encoder(int fd, int id, struct drm_mode_get_encoder *enc) {
	memset(enc, 0, sizeof(struct drm_mode_get_encoder));
	enc->encoder_id = id;
	return atomos_ioctl(fd, DRM_IOCTL_MODE_GETENCODER, enc) ? -1 : 0;
}


int atomos_handle_event(int fd, struct atomos_event_context *context) {
	char buffer[1024];
	struct drm_event *e;

	int len = read(fd, buffer, sizeof(buffer));

	if (!len) {
		return 0;
	} else if (len < sizeof(struct drm_event)) {
		return -1;
	}

	int i = 0; 
	while(i < len) {
		e = (struct drm_event *)&buffer[i];
		i += e->length;

		switch(e->type) {
			case DRM_EVENT_FLIP_COMPLETE: {
				struct drm_event_vblank *vb = (struct drm_event_vblank *)e;
				context->page_flip_handler(fd, vb->sequence, vb->tv_sec, vb->tv_usec, (void *)vb->user_data);
			} break;
		}
	}

	return 0;
}


//  Helper Impl
void free_drm_mode_card_res(struct drm_mode_card_res * res) {
	free((uint32_t *)res->fb_id_ptr);
	free((uint32_t *)res->crtc_id_ptr);
	free((uint32_t *)res->connector_id_ptr);
	free((uint32_t *)res->encoder_id_ptr);
}

void free_drm_mode_get_connector(struct drm_mode_get_connector * res) {
	free((uint32_t *)res->encoders_ptr);
	free((uint32_t *)res->modes_ptr);
	free((uint32_t *)res->props_ptr);
	free((uint32_t *)res->prop_values_ptr);
} 
