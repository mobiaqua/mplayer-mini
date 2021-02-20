/*
 * video output for OMAP DRM
 *
 * Copyright (C) 2020 Pawel Kolodziejski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/mman.h>

#include "config.h"
#include "aspect.h"

#include "video_out.h"
#include "video_out_internal.h"
#include "sub/sub.h"
#include "../mp_core.h"
#include "osdep/timer.h"
#include "libavcodec/avcodec.h"

#include <libdrm/omap_drmif.h>
#include <libswscale/swscale.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

static struct frame_info {
    unsigned int w;
    unsigned int h;
    unsigned int dx;
    unsigned int dy;
    unsigned int dw;
    unsigned int dh;
    unsigned int y_stride;
    unsigned int uv_stride;
} yuv420_frame_info, nv12_frame_info;

int yuv420_to_nv12_convert(unsigned char *vdst[3], unsigned char *vsrc[3], unsigned char *, unsigned char *);
void yuv420_to_nv12_open(struct frame_info *dst, struct frame_info *src);

#define ALIGN2(value, align) (((value) + ((1 << (align)) - 1)) & ~((1 << (align)) - 1))

static const vo_info_t info = {
	"omap drm video driver",
	"omap_drm",
	"",
	""
};

typedef struct {
	void           *priv;
	struct omap_bo *bo;
	uint32_t       boHandle;
	int            dmaBuf;
	int            locked;
} DisplayVideoBuffer;

typedef struct {
	struct omap_bo  *bo;
	uint32_t        fbId;
	void            *ptr;
	uint32_t        width, height;
	uint32_t        stride;
	uint32_t        size;
} OSDBuffer;

typedef struct {
	struct omap_bo  *bo;
	uint32_t        boHandle;
	int             dmaBuf;
	uint32_t        fbId;
	void            *ptr;
	uint32_t        width, height;
	uint32_t        stride;
	uint32_t        size;
	uint32_t        srcX, srcY;
	uint32_t        srcWidth, srcHeight;
	uint32_t        dstX, dstY;
	uint32_t        dstWidth, dstHeight;
	DisplayVideoBuffer *db;
} VideoBuffer;

typedef struct {
	int     handle;
} DisplayHandle;

typedef struct {
	DisplayHandle handle;
	int (*getDisplayVideoBuffer)(DisplayVideoBuffer *handle, uint32_t pixelfmt, int width, int height);
	int (*releaseDisplayVideoBuffer)(DisplayVideoBuffer *handle);
} omap_dce_share_t;

extern omap_dce_share_t omap_dce_share;

#define NUM_OSD_FB   2
#define NUM_VIDEO_FB 3

static int                         _dce;
static int                         _initialized;
static int                         _fd;
static struct omap_device          *_omapDevice;

static drmModeResPtr               _drmResources;
static drmModePlaneResPtr          _drmPlaneResources;
static drmModeCrtcPtr              _oldCrtc;
static drmModeModeInfo             _modeInfo;
static uint32_t                    _connectorId;
static uint32_t                    _crtcId;
static int                         _osdPlaneId;
static int                         _videoPlaneId;

struct omap_bo                     *_primaryFbBo;
uint32_t                           _primaryFbId;
OSDBuffer                          _osdBuffers[NUM_OSD_FB];
VideoBuffer                        *_videoBuffers[NUM_VIDEO_FB];
int                                _lastOsdX;
int                                _lastOsdY;
int                                _lastOsdW;
int                                _lastOsdH;
int                                _osdChanged;

int                                _currentOSDBuffer;
int                                _currentVideoBuffer;

static struct SwsContext           *_scaleCtx;

LIBVO_EXTERN(omap_drm)

static int getDisplayVideoBuffer(DisplayVideoBuffer *handle, uint32_t pixelfmt, int width, int height);
static int releaseDisplayVideoBuffer(DisplayVideoBuffer *handle);

static int preinit(const char *arg) {
	int modeId = -1, i, j;
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
	drmModeConnectorPtr connector = NULL;
	drmModeObjectPropertiesPtr props;

	_fd = drmOpen("omapdrm", NULL);
	if (_fd < 0) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed open omapdrm, %s\n", strerror(errno));
		goto fail;
	}

	_omapDevice = omap_device_new(_fd);
	if (!_omapDevice) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed create omap device\n");
		goto fail;
	}

	_drmResources = drmModeGetResources(_fd);
	if (!_drmResources) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed get DRM resources, %s\n", strerror(errno));
		goto fail;
	}

	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmResources) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed get DRM plane resources, %s\n", strerror(errno));
		goto fail;
	}

	_connectorId = -1;
	for (int i = 0; i < _drmResources->count_connectors; i++) {
		connector = drmModeGetConnector(_fd, _drmResources->connectors[i]);
		if (connector == NULL)
			continue;
		if (connector->connection != DRM_MODE_CONNECTED || connector->count_modes == 0) {
			drmModeFreeConnector(connector);
			continue;
		}
		if (connector->connector_type == DRM_MODE_CONNECTOR_HDMIA ||
		    connector->connector_type == DRM_MODE_CONNECTOR_HDMIB) {
			_connectorId = connector->connector_id;
			break;
		}
		drmModeFreeConnector(connector);
	}

	if (_connectorId == -1) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to find active HDMI connector!\n");
		goto fail;
	}

	for (j = 0; j < connector->count_modes; j++) {
		drmModeModeInfoPtr mode = &connector->modes[j];
		if ((mode->vrefresh >= 60) && (mode->type & DRM_MODE_TYPE_PREFERRED)) {
			modeId = j;
			break;
		}
	}

	if (modeId == -1) {
		uint64_t highestArea = 0;
		for (j = 0; j < connector->count_modes; j++) {
			drmModeModeInfoPtr mode = &connector->modes[j];
			const uint64_t area = mode->hdisplay * mode->vdisplay;
			if ((mode->vrefresh >= 60) && (area > highestArea)) {
				highestArea = area;
				modeId = j;
			}
		}
	}

	_crtcId = -1;
	for (i = 0; i < connector->count_encoders; i++) {
		drmModeEncoderPtr encoder = drmModeGetEncoder(_fd, connector->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id) {
			_crtcId = encoder->crtc_id;
			drmModeFreeEncoder(encoder);
			break;
		}
		drmModeFreeEncoder(encoder);
	}

	if (modeId == -1 || _crtcId == -1) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to find suitable display output!\n");
		drmModeFreeConnector(connector);
		return -1;
	}

	_modeInfo = connector->modes[modeId];

	drmModeFreeConnector(connector);

	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmPlaneResources) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to plane resources!\n");
		return -1;
	}
	_osdPlaneId = -1;
	for (i = 0; i < _drmPlaneResources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(_fd, _drmPlaneResources->planes[i]);
		if (!plane)
			continue;
		if (plane->crtc_id == 0) {
			_osdPlaneId = plane->plane_id;
			drmModeFreePlane(plane);
			break;
		}
		drmModeFreePlane(plane);
	}
	if (_osdPlaneId == -1) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to find plane!\n");
		return -1;
	}
	_videoPlaneId = -1;
	for (i = 0; i < _drmPlaneResources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(_fd, _drmPlaneResources->planes[i]);
		if (!plane)
			continue;
		if (plane->crtc_id == 0 && plane->plane_id != _osdPlaneId) {
			_videoPlaneId = plane->plane_id;
			drmModeFreePlane(plane);
			break;
		}
		drmModeFreePlane(plane);
	}
	if (_videoPlaneId == -1) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to find plane!\n");
		return -1;
	}

	props = drmModeObjectGetProperties(_fd, _osdPlaneId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to find properties for plane!\n");
		return -1;
	}
	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop && strcmp(prop->name, "zorder") == 0 && drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
			uint64_t value = props->prop_values[i];
			if (drmModeObjectSetProperty(_fd, _osdPlaneId, DRM_MODE_OBJECT_PLANE, props->props[i], 1)) {
				mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to set zorder property for plane!\n");
				return -1;
			}
		}
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);

	props = drmModeObjectGetProperties(_fd, _videoPlaneId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to find properties for plane!\n");
		return -1;
	}
	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop && strcmp(prop->name, "zorder") == 0 && drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
			uint64_t value = props->prop_values[i];
			if (drmModeObjectSetProperty(_fd, _videoPlaneId, DRM_MODE_OBJECT_PLANE, props->props[i], 0)) {
				mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed to set zorder property for plane!\n");
				return -1;
			}
		}
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);

	mp_msg(MSGT_VO, MSGL_INFO, "[omap_drm] Using display HDMI output: %dx%d@%d\n",
			_modeInfo.hdisplay, _modeInfo.vdisplay, _modeInfo.vrefresh);

	_primaryFbBo = omap_bo_new(_omapDevice, _modeInfo.hdisplay * _modeInfo.vdisplay * 4, OMAP_BO_WC | OMAP_BO_SCANOUT);
	if (!_primaryFbBo) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed allocate buffer!\n");
		goto fail;
	}
	handles[0] = omap_bo_handle(_primaryFbBo);
	pitches[0] = _modeInfo.hdisplay * 4;
	if (drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay, DRM_FORMAT_ARGB8888,
			handles, pitches, offsets, &_primaryFbId, 0) < 0) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed add primary buffer: %s\n", strerror(errno));
		goto fail;
	}
	omap_bo_cpu_prep(_primaryFbBo, OMAP_GEM_WRITE);
	memset(omap_bo_map(_primaryFbBo), 0, omap_bo_size(_primaryFbBo));
	omap_bo_cpu_fini(_primaryFbBo, OMAP_GEM_WRITE);

	_oldCrtc = drmModeGetCrtc(_fd, _crtcId);
	if (drmModeSetCrtc(_fd, _crtcId, _primaryFbId, 0, 0, &_connectorId, 1, &_modeInfo) < 0) {
		goto fail;
	}

	for (i = 0; i < NUM_OSD_FB; i++) {
		_osdBuffers[i].bo = omap_bo_new(_omapDevice, _modeInfo.hdisplay * _modeInfo.vdisplay * 4,
		                                OMAP_BO_WC | OMAP_BO_SCANOUT);
		if (!_osdBuffers[i].bo) {
			mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed allocate buffer!\n");
			goto fail;
		}
		handles[0] = omap_bo_handle(_osdBuffers[i].bo);
		pitches[0] = _modeInfo.hdisplay * 4;
		if (drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay,
			            DRM_FORMAT_ARGB8888,
			            handles, pitches, offsets, &_osdBuffers[i].fbId, 0) < 0) {
			mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed add video buffer: %s\n", strerror(errno));
			goto fail;
		}
		_osdBuffers[i].width = _modeInfo.hdisplay;
		_osdBuffers[i].height = _modeInfo.vdisplay;
		_osdBuffers[i].stride = pitches[0];
		_osdBuffers[i].size = omap_bo_size(_osdBuffers[i].bo);
		_osdBuffers[i].ptr = omap_bo_map(_osdBuffers[i].bo);
		if (!_osdBuffers[i].ptr) {
			mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] preinit() Failed get primary frame buffer!\n");
			goto fail;
		}
		omap_bo_cpu_prep(_osdBuffers[i].bo, OMAP_GEM_WRITE);
		memset(_osdBuffers[i].ptr, 0, _osdBuffers[i].size);
		omap_bo_cpu_fini(_osdBuffers[i].bo, OMAP_GEM_WRITE);
	}

	omap_dce_share.handle.handle = _fd;
	omap_dce_share.getDisplayVideoBuffer = &getDisplayVideoBuffer;
	omap_dce_share.releaseDisplayVideoBuffer = &releaseDisplayVideoBuffer;

	_scaleCtx = NULL;
	_dce = 0;
	_currentOSDBuffer = 0;
	_currentVideoBuffer = 0;
	_osdChanged = 0;

	_initialized = 1;

	return 0;

fail:

	for (int i = 0; i < NUM_OSD_FB; i++) {
		if (_osdBuffers[i].fbId) {
			drmModeRmFB(_fd, _osdBuffers[i].fbId);
		}
		if (_osdBuffers[i].bo) {
			omap_bo_del(_osdBuffers[i].bo);
		}
	}
	memset(_osdBuffers, 0, sizeof(OSDBuffer) * NUM_OSD_FB);

	if (_primaryFbId) {
		drmModeRmFB(_fd, _primaryFbId);
		_primaryFbId = 0;
	}
	if (_primaryFbBo) {
		omap_bo_del(_primaryFbBo);
		_primaryFbBo = NULL;
	}

	if (_drmPlaneResources) {
		drmModeFreePlaneResources(_drmPlaneResources);
		_drmPlaneResources = NULL;
	}

	if (_drmResources) {
		drmModeFreeResources(_drmResources);
		_drmResources = NULL;
	}

	if (_omapDevice) {
		omap_device_del(_omapDevice);
		_omapDevice = NULL;
	}

	if (_fd != -1) {
		drmClose(_fd);
		_fd = -1;
	}

	return -1;
}

static void uninit(void) {
	if (!_initialized)
		return;

	for (int i = 0; i < NUM_OSD_FB; i++) {
		if (_osdBuffers[i].fbId) {
			drmModeRmFB(_fd, _osdBuffers[i].fbId);
		}
		if (_osdBuffers[i].bo) {
			omap_bo_del(_osdBuffers[i].bo);
		}
	}
	memset(_osdBuffers, 0, sizeof(OSDBuffer) * NUM_OSD_FB);

	if (!_dce) {
		for (int i = 0; i < NUM_VIDEO_FB; i++) {
			if (_videoBuffers[i] && _videoBuffers[i]->fbId) {
				drmModeRmFB(_fd, _videoBuffers[i]->fbId);
			}
			if (_videoBuffers[i] && _videoBuffers[i]->bo) {
				omap_bo_del(_videoBuffers[i]->bo);
			}
		}
		memset(_videoBuffers, 0, sizeof(VideoBuffer) * NUM_VIDEO_FB);
	}

	if (_oldCrtc) {
		drmModeSetCrtc(_fd, _oldCrtc->crtc_id, _oldCrtc->buffer_id,
			       _oldCrtc->x, _oldCrtc->y, &_connectorId, 1, &_oldCrtc->mode);
		drmModeFreeCrtc(_oldCrtc);
		_oldCrtc = NULL;
	}

	if (_primaryFbId) {
		drmModeRmFB(_fd, _primaryFbId);
		_primaryFbId = 0;
	}
	if (_primaryFbBo) {
		omap_bo_del(_primaryFbBo);
		_primaryFbBo = NULL;
	}

	if (_drmPlaneResources)
		drmModeFreePlaneResources(_drmPlaneResources);

	if (_drmResources)
		drmModeFreeResources(_drmResources);

	if (_omapDevice)
		omap_device_del(_omapDevice);

	if (_fd != -1)
		drmClose(_fd);

	_initialized = 0;
}

static VideoBuffer *getVideoBuffer(uint32_t pixelfmt, int width, int height) {
	DisplayVideoBuffer buffer;
	VideoBuffer *videoBuffer;

	if (getDisplayVideoBuffer(&buffer, pixelfmt, width, height) != 0) {
		return NULL;
	}

	videoBuffer = (VideoBuffer *)buffer.priv;
	videoBuffer->db = NULL;

	return videoBuffer;
}

static int getDisplayVideoBuffer(DisplayVideoBuffer *handle, uint32_t pixelfmt, int width, int height) {
	VideoBuffer *videoBuffer;
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
	uint32_t fbSize;
	int ret;

	if (!_initialized || !handle)
		return -1;

	videoBuffer = calloc(1, sizeof(VideoBuffer));

	fbSize = width * height * 3 / 2;
	handle->locked = 0;
	videoBuffer->bo = handle->bo = omap_bo_new(_omapDevice, fbSize, OMAP_BO_WC | OMAP_BO_SCANOUT);
	if (!videoBuffer->bo) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] getVideoBuffer() Failed allocate video buffer\n");
		return -1;
	}
	handles[0] = videoBuffer->boHandle = handle->boHandle = omap_bo_handle(handle->bo);
	pitches[0] = width;
	handles[1] = handles[0];
	pitches[1] = pitches[0];
	offsets[1] = width * height;
	if (drmModeAddFB2(_fd, width, height,
		            DRM_FORMAT_NV12, handles, pitches, offsets, &videoBuffer->fbId, 0) < 0) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] getVideoBuffer() Failed add video buffer: %s\n", strerror(errno));
		return -1;
	}
	videoBuffer->srcX = 0;
	videoBuffer->srcY = 0;
	videoBuffer->srcWidth = width;
	videoBuffer->srcHeight = height;
	videoBuffer->stride = pitches[0];
	videoBuffer->width = width;
	videoBuffer->height = height;
	videoBuffer->dstX = 0;
	videoBuffer->dstY = 0;
	videoBuffer->dstWidth = width;
	videoBuffer->dstHeight = height;
	videoBuffer->dmaBuf = handle->dmaBuf = omap_bo_dmabuf(videoBuffer->bo);
	videoBuffer->size = omap_bo_size(videoBuffer->bo);
	videoBuffer->ptr = omap_bo_map(videoBuffer->bo);
	if (!videoBuffer->ptr) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] getVideoBuffer() Failed get video frame buffer\n");
		return -1;
	}

	videoBuffer->db = handle;
	handle->priv = videoBuffer;

	return 0;
}

static int releaseVideoBuffer(VideoBuffer *buffer) {
	if (!_initialized || !buffer)
		return -1;

	drmModeRmFB(_fd, buffer->fbId);

	close(buffer->dmaBuf);

	omap_bo_del(buffer->bo);

	free(buffer);

	return 0;
}

static int releaseDisplayVideoBuffer(DisplayVideoBuffer *handle) {
	VideoBuffer *videoBuffer;

	if (!_initialized || !handle)
		return -1;

	videoBuffer = (VideoBuffer *)handle->priv;
	if (!videoBuffer)
		return -1;

	if (releaseVideoBuffer(videoBuffer) != 0)
		return -1;

	handle->bo = NULL;
	handle->priv = NULL;

	return 0;
}

static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format) {
	if (!_initialized)
		return -1;

	switch (format) {
	case IMGFMT_NV12:
		_dce = 1;
		break;
	case IMGFMT_YV12:
		_dce = 0;
		break;
	default:
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] config() Error wrong pixel format\n");
		return -1;
	}

	_currentOSDBuffer = 0;
	_currentVideoBuffer = 0;

	return 0;
}

static int query_format(uint32_t format) {
	if (format == IMGFMT_YV12 || format == IMGFMT_NV12)
		return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_OSD | VFCAP_EOSD | VFCAP_EOSD_UNSCALED |
		       VFCAP_FLIP | VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VOCAP_NOSLICES;

	return 0;
}

static int draw_frame(uint8_t *src[]) {
	// empty
	return VO_FALSE;
}

static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y) {
	// empty
	return VO_FALSE;
}

static uint32_t put_image(mp_image_t *mpi) {
	float x, y, w, h;
	int frame_width, frame_height;

	frame_width = mpi->width;
	frame_height = mpi->height;

	if (_dce) {
		DisplayVideoBuffer *db = (DisplayVideoBuffer *)(mpi->priv);
		db->locked = 1;
		_videoBuffers[_currentVideoBuffer] = (VideoBuffer *)db->priv;
	} else {
		uint8_t *srcPtr[4] = {};
		uint8_t *dstPtr[4] = {};
		int srcStride[4] = {};
		int dstStride[4] = {};
		uint8_t *dst;

		if (!_videoBuffers[_currentVideoBuffer])
			_videoBuffers[_currentVideoBuffer] = getVideoBuffer(IMGFMT_NV12, frame_width, frame_height);
		dst = (uint8_t *)_videoBuffers[_currentVideoBuffer]->ptr;
		if (0 && mpi->imgfmt == IMGFMT_YV12 && (ALIGN2(frame_width, 5) == frame_width)) {
			srcPtr[0] = mpi->planes[0];
			srcPtr[1] = mpi->planes[1];
			srcPtr[2] = mpi->planes[2];
			dstPtr[0] = dst;
			dstPtr[1] = dst + frame_width * frame_height;
			dstPtr[2] = 0;

			yuv420_frame_info.w = frame_width;
			yuv420_frame_info.h = frame_height;
			yuv420_frame_info.dx = 0;
			yuv420_frame_info.dy = 0;
			yuv420_frame_info.dw = frame_width;
			yuv420_frame_info.dh = frame_height;
			yuv420_frame_info.y_stride = mpi->stride[0];
			yuv420_frame_info.uv_stride = mpi->stride[1];

			nv12_frame_info.w = frame_width;
			nv12_frame_info.h = frame_height;
			nv12_frame_info.dx = 0;
			nv12_frame_info.dy = 0;
			nv12_frame_info.dw = frame_width;
			nv12_frame_info.dh = frame_height;
			nv12_frame_info.y_stride = frame_width;
			nv12_frame_info.uv_stride = frame_width;

			yuv420_to_nv12_open(&yuv420_frame_info, &nv12_frame_info);

			omap_bo_cpu_prep(_videoBuffers[_currentVideoBuffer]->bo, OMAP_GEM_WRITE);
			yuv420_to_nv12_convert(dstPtr, srcPtr, NULL, NULL);
			omap_bo_cpu_fini(_videoBuffers[_currentVideoBuffer]->bo, OMAP_GEM_WRITE);
		} else if (mpi->imgfmt == IMGFMT_YV12) {
			srcPtr[0] = mpi->planes[0];
			srcPtr[1] = mpi->planes[1];
			srcPtr[2] = mpi->planes[2];
			srcPtr[3] = mpi->planes[3];
			srcStride[0] = mpi->stride[0];
			srcStride[1] = mpi->stride[1];
			srcStride[2] = mpi->stride[2];
			srcStride[3] = mpi->stride[3];
			dstPtr[0] = dst;
			dstPtr[1] = dst + frame_width * frame_height;
			dstPtr[2] = NULL;
			dstPtr[3] = NULL;
			dstStride[0] = frame_width;
			dstStride[1] = frame_width;
			dstStride[2] = 0;
			dstStride[3] = 0;

			if (!_scaleCtx) {
				_scaleCtx = sws_getContext(frame_width, frame_height, AV_PIX_FMT_YUV420P,
					                   frame_width, frame_height,
					                   AV_PIX_FMT_NV12, SWS_POINT, NULL, NULL, NULL);
				if (!_scaleCtx) {
					mp_msg(MSGT_VO, MSGL_FATAL,
						"[omap_drm] Error: put_image() Can not create scale context!\n");
					goto fail;
				}
			}
			omap_bo_cpu_prep(_videoBuffers[_currentVideoBuffer]->bo, OMAP_GEM_WRITE);
			sws_scale(_scaleCtx, (const uint8_t *const *)srcPtr, srcStride, 0, frame_height, dstPtr, dstStride);
			omap_bo_cpu_fini(_videoBuffers[_currentVideoBuffer]->bo, OMAP_GEM_WRITE);
		} else {
			mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] Error: put_image() Not supported format!\n");
			goto fail;
		}
	}

	if ((mpi->flags & 0x800000) || (mpi->w == 720 && (mpi->h == 576 || mpi->h == 480))) { // hack: anisotropic
		x = 0;
		y = 0;
		w = _modeInfo.hdisplay;
		h = _modeInfo.vdisplay;
	} else {
		float rw = (float)(mpi->w) / _modeInfo.hdisplay;
		float rh = (float)(mpi->h) / _modeInfo.vdisplay;
		if (rw >= rh) {
			w = _modeInfo.hdisplay;
			h = _modeInfo.vdisplay * (rh / rw);
			x = 0;
			y = (_modeInfo.vdisplay - h) / 2;
		} else {
			w = _modeInfo.hdisplay * (rw / rh);
			h = _modeInfo.vdisplay;
			x = (_modeInfo.hdisplay - w) / 2;
			y = 0;
		}
	}

	_videoBuffers[_currentVideoBuffer]->srcX = mpi->x;
	_videoBuffers[_currentVideoBuffer]->srcY = mpi->y;
	_videoBuffers[_currentVideoBuffer]->srcWidth = mpi->w;
	_videoBuffers[_currentVideoBuffer]->srcHeight = mpi->h;

	_videoBuffers[_currentVideoBuffer]->dstX = x;
	_videoBuffers[_currentVideoBuffer]->dstY = y;
	_videoBuffers[_currentVideoBuffer]->dstWidth = w;
	_videoBuffers[_currentVideoBuffer]->dstHeight = h;

	return VO_TRUE;

fail:
	return VO_FALSE;
}

static void draw_alpha(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int srcstride) {
	int x, y;
	int dststride;
	uint8_t *dstbase;

	_lastOsdX = x0;
	_lastOsdY = y0;
	_lastOsdW = w;
	_lastOsdH = h;

	dststride = _osdBuffers[_currentOSDBuffer].stride;
	dstbase = ((uint8_t *)_osdBuffers[_currentOSDBuffer].ptr) + (y0 * _osdBuffers[_currentOSDBuffer].stride) + (x0 * 4);

	omap_bo_cpu_prep(_osdBuffers[_currentOSDBuffer].bo, OMAP_GEM_WRITE);

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (srca[x]) {
				dstbase[4 * x + 0] = ((dstbase[4 * x + 0] * srca[x]) >> 8) + src[x];
				dstbase[4 * x + 1] = ((dstbase[4 * x + 1] * srca[x]) >> 8) + src[x];
				dstbase[4 * x + 2] = ((dstbase[4 * x + 2] * srca[x]) >> 8) + src[x];
				dstbase[4 * x + 3] = 255;
			}
		}
		src += srcstride;
		srca += srcstride;
		dstbase += dststride;
	}

	omap_bo_cpu_fini(_osdBuffers[_currentOSDBuffer].bo, OMAP_GEM_WRITE);
}

static void draw_osd(void) {
	_osdChanged = vo_osd_changed(0);
	if (_osdChanged) {
		omap_bo_cpu_prep(_osdBuffers[_currentOSDBuffer].bo, OMAP_GEM_WRITE);
		memset(_osdBuffers[_currentOSDBuffer].ptr, 0, _osdBuffers[_currentOSDBuffer].size);
		omap_bo_cpu_fini(_osdBuffers[_currentOSDBuffer].bo, OMAP_GEM_WRITE);

		vo_draw_text(_modeInfo.hdisplay, _modeInfo.vdisplay - 20, draw_alpha);
	}
}

static void flip_page() {
	if (!_initialized)
		goto fail;

	if (drmModeSetPlane(_fd, _videoPlaneId, _crtcId,
			_videoBuffers[_currentVideoBuffer]->fbId, 0,
			_videoBuffers[_currentVideoBuffer]->dstX,
			_videoBuffers[_currentVideoBuffer]->dstY,
			_videoBuffers[_currentVideoBuffer]->dstWidth,
			_videoBuffers[_currentVideoBuffer]->dstHeight,
			_videoBuffers[_currentVideoBuffer]->srcX << 16,
			_videoBuffers[_currentVideoBuffer]->srcY << 16,
			_videoBuffers[_currentVideoBuffer]->srcWidth << 16,
			_videoBuffers[_currentVideoBuffer]->srcHeight << 16
			)) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] Error: flip() Failed set plane: %s\n", strerror(errno));
		goto fail;
	}
	if (++_currentVideoBuffer >= NUM_VIDEO_FB)
		_currentVideoBuffer = 0;
	if (_videoBuffers[_currentVideoBuffer] &&
		_videoBuffers[_currentVideoBuffer]->db) {
		_videoBuffers[_currentVideoBuffer]->db->locked = 0;
	}

	if (_osdChanged) {
		if (drmModeSetPlane(_fd, _osdPlaneId, _crtcId,
				_osdBuffers[_currentOSDBuffer].fbId, 0,
				0, 0, _modeInfo.hdisplay, _modeInfo.vdisplay,
				0, 0, _modeInfo.hdisplay << 16, _modeInfo.vdisplay << 16
				)) {
			mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm] Error: flip() Failed set plane: %s\n", strerror(errno));
			goto fail;
		}
		if (++_currentOSDBuffer >= NUM_OSD_FB)
			_currentOSDBuffer = 0;
		_osdChanged = 0;
	}

	return;

fail:

	return;
}

static void check_events(void) {
	// empty
}

static int control(uint32_t request, void *data) {
	switch (request) {
	case VOCTRL_QUERY_FORMAT:
		return query_format(*((uint32_t *)data));
	case VOCTRL_FULLSCREEN:
		return VO_TRUE;
	case VOCTRL_UPDATE_SCREENINFO:
		vo_screenwidth = _modeInfo.hdisplay;
		vo_screenheight = _modeInfo.vdisplay;
		aspect_save_screenres(vo_screenwidth, vo_screenheight);
		return VO_TRUE;
	case VOCTRL_DRAW_IMAGE:
		return put_image(data);
	}
	return VO_NOTIMPL;
}
