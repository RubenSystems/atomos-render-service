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


int atomui_handle_event(int fd, struct atomui_render_context *context, char * buffer) {
	// char buffer[1024];
	struct drm_event *e;

	int len = read(fd, buffer, ATOMUI_SMALL_BUF_SIZE);

	if (!len) {
		return 0;
	} else if (len < sizeof(struct drm_event)) {
		return -1;
	}

	for (int i = 0; i < len; i += e->length) {
		e = (struct drm_event *)&buffer[i];

		if (e->type == DRM_EVENT_FLIP_COMPLETE) {
			struct drm_event_vblank *vb = (struct drm_event_vblank *)e;
			context->render_handler(fd, vb->sequence, vb->tv_sec, vb->tv_usec, (void *)vb->user_data);
			break;
		}
	}

	return 0;
}


int atomui_get_modes(int fd, struct atomui_size size, struct drm_mode_card_res * resource, struct drm_mode_get_connector * _connector, struct drm_mode_modeinfo * _mode) {
	for (int connector_index = 0; connector_index < resource->count_connectors; connector_index++) {
		uint32_t * connectors = (uint32_t *)resource->connector_id_ptr;

		int ret = atomui_get_connector(fd, connectors[connector_index], _connector);

		if (ret || _connector->connection != DRM_MODE_CONNECTED) {
			continue;
		}

		struct drm_mode_modeinfo * modes = (struct drm_mode_modeinfo *)_connector->modes_ptr;

		for (int mode_index = 0; mode_index < _connector->count_modes; mode_index ++) {
			printf("\t[ATOMUI] - mode: %dx%d\n", modes[mode_index].hdisplay, modes[mode_index].vdisplay);

			if (size.width == modes[mode_index].hdisplay && size.height == modes[mode_index].vdisplay) {
				*_mode = ((struct drm_mode_modeinfo *)_connector->modes_ptr)[mode_index];
				return 1;
			}
		}
		free_drm_mode_get_connector(_connector);
	}
	return -1; 
}

// Create framebuffer 

bool atomui_create_framebuffer(int fd, struct atomui_buffer *buf) {
	struct drm_mode_create_dumb creq;
	struct drm_mode_create_dumb dreq;
	struct drm_mode_map_dumb mreq;

	memset(&creq, 0, sizeof(creq));
	creq.width = buf->size.width;
	creq.height = buf->size.height;
	creq.bpp = 32; // Bits per pixel (A A R R G G B B) (four bits per letter)
 

	int ret = atomui_ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);

	if (ret < 0) {
		printf("Failed to create buffer: %d\n", ret);
		return false;
	}

	buf->stride = creq.pitch;
	buf->handle = creq.handle;

	struct drm_mode_fb_cmd fbcmd;
	memset(&fbcmd, 0, sizeof(fbcmd));
	fbcmd.width = buf->size.width;
	fbcmd.height = buf->size.height;
	fbcmd.depth = 24;
	fbcmd.bpp = 32; // Bits per pixel see above for expl
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

	if (buf->map == -1) {
		printf("Failed to map FB!\n");
		return false;
	}

	// memset(buf->map, 0, area_from_size(buf->size));

	return true;
}


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

void free_framebuffer(struct atomui_buffer * buf) {
	munmap(buf, area_from_size(buf->size));
}
