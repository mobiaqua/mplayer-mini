/*
 * video output for OMAP DRM EGL
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
#include <unistd.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/mman.h>

#include "config.h"
#if OMAP_DRM
#include "aspect.h"

#include "video_out.h"
#include "video_out_internal.h"
#include "sub/sub.h"
#include "../mp_core.h"
#include "osdep/timer.h"
#include "libavcodec/avcodec.h"

#include <libswscale/swscale.h>
#include <drm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <gbm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define ALIGN2(value, align) (((value) + ((1 << (align)) - 1)) & ~((1 << (align)) - 1))

#ifndef SIZE_OF_ARRAY
#define SIZE_OF_ARRAY(a)   ((int)(sizeof(a) / sizeof(a[0])))
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 02000000
#endif

static const vo_info_t info = {
	"omap drm egl video driver",
	"omap_drm_egl",
	"",
	""
};

typedef struct {
	int             fd;
	struct gbm_bo   *gbmBo;
	uint32_t        fbId;
} DrmFb;

typedef struct {
	void           *priv;
	uint32_t       handle;
	int            dmaBuf;
	int            locked;
} DisplayVideoBuffer;

typedef struct {
    uint32_t           handle;
	int                dmabuf;
	void               *mapPtr;
	uint32_t           mapSize;
	EGLImageKHR        image;
	GLuint             glTexture;
	DisplayVideoBuffer *db;
} RenderTexture;

typedef struct {
	int     handle;
} DisplayHandle;

typedef struct {
	DisplayHandle handle;
	int (*getDisplayVideoBuffer)(DisplayVideoBuffer *handle, uint32_t pixelfmt, int width, int height);
	int (*releaseDisplayVideoBuffer)(DisplayVideoBuffer *handle);
} omap_dce_share_t;

extern omap_dce_share_t omap_dce_share;

static int                         _dce;
static int                         _initialized;
static int                         _fd;
static struct gbm_device           *_gbmDevice;
static struct gbm_surface          *_gbmSurface;

static drmModeResPtr               _drmResources;
static drmModePlaneResPtr          _drmPlaneResources;
static drmModeCrtcPtr              _oldCrtc;
static drmModeModeInfo             _modeInfo;
static uint32_t                    _connectorId;
static uint32_t                    _crtcId;
static int                         _planeId;

static EGLDisplay                  _eglDisplay;
static EGLSurface                  _eglSurface;
static EGLConfig                   _eglConfig;
static EGLContext                  _eglContext;
static PFNEGLCREATEIMAGEKHRPROC    eglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC   eglDestroyImageKHR;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
static GLuint                      _vertexShader;
static GLuint                      _fragmentShader;
static GLuint                      _glProgram;
static RenderTexture               *_renderTexture;
static struct SwsContext           *_scaleCtx;
static uint32_t                    _primaryHandle;
static uint32_t                    _primaryFbId;
static void                        *_primaryPtr;
static uint32_t                    _primarySize;

LIBVO_EXTERN(omap_drm_egl)

static RenderTexture *getRenderTexture(uint32_t pixelfmt, int width, int height);
static int releaseRenderTexture(RenderTexture *texture);
static DrmFb *getDrmFb(struct gbm_bo *gbmBo);
static int getDisplayVideoBuffer(DisplayVideoBuffer *handle, uint32_t pixelfmt, int width, int height);
static int releaseDisplayVideoBuffer(DisplayVideoBuffer *handle);

#define EGL_STR_ERROR(value) case value: return #value;
static const char* eglGetErrorStr(EGLint error) {
	switch (error) {
		EGL_STR_ERROR(EGL_SUCCESS)
		EGL_STR_ERROR(EGL_NOT_INITIALIZED)
		EGL_STR_ERROR(EGL_BAD_ACCESS)
		EGL_STR_ERROR(EGL_BAD_ALLOC)
		EGL_STR_ERROR(EGL_BAD_ATTRIBUTE)
		EGL_STR_ERROR(EGL_BAD_CONFIG)
		EGL_STR_ERROR(EGL_BAD_CONTEXT)
		EGL_STR_ERROR(EGL_BAD_CURRENT_SURFACE)
		EGL_STR_ERROR(EGL_BAD_DISPLAY)
		EGL_STR_ERROR(EGL_BAD_MATCH)
		EGL_STR_ERROR(EGL_BAD_NATIVE_PIXMAP)
		EGL_STR_ERROR(EGL_BAD_NATIVE_WINDOW)
		EGL_STR_ERROR(EGL_BAD_PARAMETER)
		EGL_STR_ERROR(EGL_BAD_SURFACE)
		EGL_STR_ERROR(EGL_CONTEXT_LOST)
		default: return "Unknown";
	}
}
#undef EGL_STR_ERROR

static int preinit(const char *arg) {
	int modeId = -1, i, j;
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
	struct gbm_bo *gbmBo;
	DrmFb *drmFb;
	drmModeConnectorPtr connector = NULL;
	EGLint major, minor;
	EGLint numConfig;
	GLint shaderStatus;
	drmModeObjectPropertiesPtr props;
	drmDevice *devices[DRM_MAX_MINOR] = { 0 };
	struct drm_mode_create_dumb creq = {0};
	struct drm_mode_map_dumb mreq = {0};

	const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	const EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};

	static const GLchar *vertexShaderSource =
		"attribute vec4 position;                      \n"
		"attribute vec2 texCoord;                      \n"
		"varying   vec2 textureCoords;                 \n"
		"void main()                                   \n"
		"{                                             \n"
		"    textureCoords = texCoord;                 \n"
		"    gl_Position = position                ;   \n"
		"}                                             \n";

	static const GLchar *fragmentShaderSource =
		"#extension GL_OES_EGL_image_external : require               \n"
		"precision mediump float;                                     \n"
		"varying vec2               textureCoords;                    \n"
		"uniform samplerExternalOES textureSampler;                   \n"
		"void main()                                                  \n"
		"{                                                            \n"
		"    gl_FragColor = texture2D(textureSampler, textureCoords); \n"
		"}                                                            \n";

	int card_count = drmGetDevices2(0, devices, SIZE_OF_ARRAY(devices));
	for (int i = 0; i < card_count; i++) {
		drmDevice *dev = devices[i];
		if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY))) {
			continue;
		}
		_fd = open(dev->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC);
		_drmResources = drmModeGetResources(_fd);
		if (!_drmResources) {
			close(_fd);
			_fd = -1;
			continue;
		}
		break;
	}
	if (_fd < 0) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed open, %s\n", strerror(errno));
		goto fail;
	}

	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmResources) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed get DRM plane resources, %s\n", strerror(errno));
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
		if (connector->connector_type == DRM_MODE_CONNECTOR_HDMIA || connector->connector_type == DRM_MODE_CONNECTOR_HDMIB) {
			_connectorId = connector->connector_id;
			break;
		}
		drmModeFreeConnector(connector);
	}

	if (_connectorId == -1) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to find active HDMI connector!\n");
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
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to find suitable display output!\n");
		drmModeFreeConnector(connector);
		return -1;
	}

	_modeInfo = connector->modes[modeId];

	drmModeFreeConnector(connector);

	_planeId = -1;
	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmPlaneResources) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to plane resources!\n");
		return -1;
	}
	for (i = 0; i < _drmPlaneResources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(_fd, _drmPlaneResources->planes[i]);
		if (!plane)
			continue;
		if (plane->crtc_id == 0) {
			_planeId = plane->plane_id;
			drmModeFreePlane(plane);
			break;
		}
		drmModeFreePlane(plane);
	}
	if (_planeId == -1) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to find plane!\n");
		return -1;
	}

	props = drmModeObjectGetProperties(_fd, _planeId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to find properties for plane!\n");
		return -1;
	}
	for (i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop && strcmp(prop->name, "zorder") == 0 && drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
			uint64_t value = props->prop_values[i];
			if (drmModeObjectSetProperty(_fd, _planeId, DRM_MODE_OBJECT_PLANE, props->props[i], 0)) {
				mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to set zorder property for plane!\n");
				return -1;
			}
		}
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);

	mp_msg(MSGT_VO, MSGL_INFO, "[omap_drm_egl] Using display HDMI output: %dx%d@%d\n",
	       _modeInfo.hdisplay, _modeInfo.vdisplay, _modeInfo.vrefresh);

	_gbmDevice = gbm_create_device(_fd);
	if (!_gbmDevice) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to create gbm device!\n");
		goto fail;
	}

	_gbmSurface = gbm_surface_create(
	    _gbmDevice,
	    _modeInfo.hdisplay,
	    _modeInfo.vdisplay,
	    GBM_FORMAT_XRGB8888,
	    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING
	    );
	if (!_gbmSurface) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to create gbm surface!\n");
		goto fail;
	}

	_eglDisplay = eglGetDisplay((EGLNativeDisplayType)_gbmDevice);
	if (_eglDisplay == EGL_NO_DISPLAY) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to create display!\n");
		goto fail;
	}

	if (!eglInitialize(_eglDisplay, &major, &minor)) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to initialize egl, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	mp_msg(MSGT_VO, MSGL_INFO, "[omap_drm_egl] EGL vendor version: \"%s\"\n", eglQueryString(_eglDisplay, EGL_VERSION));

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to bind EGL_OPENGL_ES_API, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	if (!eglChooseConfig(_eglDisplay, configAttribs, &_eglConfig, 1, &numConfig)) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to choose config, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}
	if (numConfig != 1) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() More than 1 config: %d\n", numConfig);
		goto fail;
	}

	_eglContext = eglCreateContext(_eglDisplay, _eglConfig, EGL_NO_CONTEXT, contextAttribs);
	if (_eglContext == EGL_NO_CONTEXT) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to create context, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	_eglSurface = eglCreateWindowSurface(_eglDisplay, _eglConfig, _gbmSurface, NULL);
	if (_eglSurface == EGL_NO_SURFACE) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed to create egl surface, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	if (!eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext)) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed attach rendering context to egl surface, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	if (!(eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR"))) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() No eglCreateImageKHR!\n");
		goto fail;
	}

	if (!(eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR"))) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() No eglDestroyImageKHR!\n");
		goto fail;
	}

	if (!(glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES"))) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() No glEGLImageTargetTexture2DOES!\n");
		goto fail;
	}

	if (!strstr(eglQueryString(_eglDisplay, EGL_EXTENSIONS), "EGL_EXT_image_dma_buf_import")) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() No EGL_EXT_image_dma_buf_import extension!\n");
		goto fail;
	}

	_vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(_vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(_vertexShader);
	glGetShaderiv(_vertexShader, GL_COMPILE_STATUS, &shaderStatus);
	if (!shaderStatus) {
		char logStr[shaderStatus];
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Vertex shader compilation failed!\n");
		glGetShaderiv(_vertexShader, GL_INFO_LOG_LENGTH, &shaderStatus);
		if (shaderStatus > 1) {
			glGetShaderInfoLog(_vertexShader, shaderStatus, NULL, logStr);
			mp_msg(MSGT_VO, MSGL_FATAL, logStr);
		}
		goto fail;
	}

	_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(_fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(_fragmentShader);
	glGetShaderiv(_fragmentShader, GL_COMPILE_STATUS, &shaderStatus);
	if (!shaderStatus) {
		char logStr[shaderStatus];
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Fragment shader compilation failed!\n");
		glGetShaderiv(_fragmentShader, GL_INFO_LOG_LENGTH, &shaderStatus);
		if (shaderStatus > 1) {
			glGetShaderInfoLog(_fragmentShader, shaderStatus, NULL, logStr);
			mp_msg(MSGT_VO, MSGL_FATAL, logStr);
		}
		goto fail;
	}

	_glProgram = glCreateProgram();

	glAttachShader(_glProgram, _vertexShader);
	glAttachShader(_glProgram, _fragmentShader);

	glBindAttribLocation(_glProgram, 0, "position");
	glBindAttribLocation(_glProgram, 1, "texCoord");

	glLinkProgram(_glProgram);
	glGetProgramiv(_glProgram, GL_LINK_STATUS, &shaderStatus);
	if (!shaderStatus) {
		char logStr[shaderStatus];
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Program linking failed!\n");
		glGetProgramiv(_glProgram, GL_INFO_LOG_LENGTH, &shaderStatus);
		if (shaderStatus > 1) {
			glGetProgramInfoLog(_glProgram, shaderStatus, NULL, logStr);
			mp_msg(MSGT_VO, MSGL_FATAL, logStr);
		}
		goto fail;
	}

	glUseProgram(_glProgram);

	glViewport(0, 0, _modeInfo.hdisplay, _modeInfo.vdisplay);

	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	creq.height = _modeInfo.vdisplay;
	creq.width = _modeInfo.hdisplay;
	creq.bpp = 32;
	if (drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Cannot create dumb buffer: %s\n", strerror(errno));
		goto fail;
	}

	_primaryHandle = handles[0] = creq.handle;
	pitches[0] = creq.pitch;
	if (drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay, DRM_FORMAT_ARGB8888,
	                  handles, pitches, offsets, &_primaryFbId, 0) < 0) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Failed add primary buffer: %s\n", strerror(errno));
		goto fail;
	}

	mreq.handle = creq.handle;
	if (drmIoctl(_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Cannot map dumb buffer: %s\n", strerror(errno));
		goto fail;
	}

	_primaryPtr = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, mreq.offset);
	if (_primaryPtr == MAP_FAILED) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] preinit() Cannot map dumb buffer: %s\n", strerror(errno));
		goto fail;
	}
	_primarySize = creq.size;

	memset(_primaryPtr, 0, _primarySize);

	_oldCrtc = drmModeGetCrtc(_fd, _crtcId);
	if (drmModeSetCrtc(_fd, _crtcId, _primaryFbId, 0, 0, &_connectorId, 1, &_modeInfo) < 0) {
		goto fail;
	}

	omap_dce_share.handle.handle = _fd;
	omap_dce_share.getDisplayVideoBuffer = &getDisplayVideoBuffer;
	omap_dce_share.releaseDisplayVideoBuffer = &releaseDisplayVideoBuffer;

	_scaleCtx = NULL;
	_dce = 0;

	_initialized = 1;

	return 0;

fail:

	if (_vertexShader) {
		glDeleteShader(_vertexShader);
		_vertexShader = 0;
	}
	if (_fragmentShader) {
		glDeleteShader(_fragmentShader);
		_fragmentShader = 0;
	}
	if (_glProgram) {
		glDeleteProgram(_glProgram);
		_glProgram = 0;
	}
	if (_eglSurface) {
		eglDestroySurface(_eglDisplay, _eglSurface);
		_eglSurface = NULL;
	}
	if (_eglContext) {
		eglDestroyContext(_eglDisplay, _eglContext);
		_eglContext = NULL;
	}
	if (_eglDisplay) {
		eglTerminate(_eglDisplay);
		_eglDisplay = NULL;
	}
	if (_gbmSurface) {
		gbm_surface_destroy(_gbmSurface);
		_gbmSurface = NULL;
	}
	if (_gbmDevice != NULL) {
		gbm_device_destroy(_gbmDevice);
		_gbmDevice = NULL;
	}

	if (_primaryFbId) {
		drmModeRmFB(_fd, _primaryFbId);
		_primaryFbId = 0;
	}
	if (_primaryPtr) {
		munmap(_primaryPtr, _primarySize);
	}
	if (_primaryHandle > 0) {
		struct drm_mode_destroy_dumb dreq = {
			.handle = _primaryHandle,
		};
		drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	}

	if (_drmPlaneResources != NULL) {
		drmModeFreePlaneResources(_drmPlaneResources);
		_drmPlaneResources = NULL;
	}

	if (_drmResources != NULL) {
		drmModeFreeResources(_drmResources);
		_drmResources = NULL;
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

	if (_scaleCtx) {
		sws_freeContext(_scaleCtx);
		_scaleCtx = NULL;
	}

	if (_vertexShader) {
		glDeleteShader(_vertexShader);
		_vertexShader = 0;
	}

	if (_fragmentShader) {
		glDeleteShader(_fragmentShader);
		_fragmentShader = 0;
	}

	if (_glProgram) {
		glDeleteProgram(_glProgram);
		_glProgram = 0;
	}

	if (_renderTexture) {
		releaseRenderTexture(_renderTexture);
		_renderTexture = NULL;
	}

	if (_eglDisplay) {
		glFinish();
		eglMakeCurrent(_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroyContext(_eglDisplay, _eglContext);
		eglDestroySurface(_eglDisplay, _eglSurface);
		eglTerminate(_eglDisplay);
		_eglDisplay = NULL;
	}

	if (_gbmSurface) {
		gbm_surface_destroy(_gbmSurface);
		_gbmSurface = NULL;
	}

	if (_gbmDevice) {
		gbm_device_destroy(_gbmDevice);
		_gbmDevice = NULL;
	}

	if (_oldCrtc) {
		drmModeSetCrtc(_fd, _oldCrtc->crtc_id, _oldCrtc->buffer_id, _oldCrtc->x, _oldCrtc->y, &_connectorId, 1, &_oldCrtc->mode);
		drmModeFreeCrtc(_oldCrtc);
		_oldCrtc = NULL;
	}

	if (_primaryFbId) {
		drmModeRmFB(_fd, _primaryFbId);
		_primaryFbId = 0;
	}
	if (_primaryPtr) {
		munmap(_primaryPtr, _primarySize);
	}
	if (_primaryHandle > 0) {
		struct drm_mode_destroy_dumb dreq = {
			.handle = _primaryHandle,
		};
		drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	}

	if (_drmPlaneResources)
		drmModeFreePlaneResources(_drmPlaneResources);

	if (_drmResources)
		drmModeFreeResources(_drmResources);

	if (_fd != -1)
		drmClose(_fd);

	_initialized = 0;
}

static int getHandle(DisplayHandle *handle) {
	if (!_initialized || handle == NULL)
		return -1;

	handle->handle = _fd;

	return 0;
}

static int getDisplayVideoBuffer(DisplayVideoBuffer *handle, uint32_t pixelfmt, int width, int height) {
	RenderTexture *renderTexture;
	uint32_t fourcc;
	uint32_t stride;
	uint32_t fbSize;
	struct drm_mode_create_dumb creq;
	struct drm_mode_map_dumb mreq;
	struct drm_prime_handle req;
	void *map;

	if (!_initialized || handle == NULL)
		return -1;

	renderTexture = malloc(sizeof(RenderTexture));
	memset(renderTexture, 0, sizeof (RenderTexture));

	if (pixelfmt == IMGFMT_YV12 || pixelfmt == IMGFMT_NV12) {
		fourcc = DRM_FORMAT_NV12;
		stride = width;
		fbSize = width * height * 3 / 2;
	} else {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDisplayVideoBuffer() Can not handle pixel format!\n");
		return -1;
	}

	creq.height = fbSize;
	creq.width = 1;
	creq.bpp = 8;
	if (drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDisplayVideoBuffer() Cannot create dumb buffer, error: %s\n", strerror(errno));
		return -1;
	}

	mreq.handle = creq.handle;
	if (drmIoctl(_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDisplayVideoBuffer() Cannot map dumb buffer, error: %s\n", strerror(errno));
		return -1;
	}
	handle->handle = creq.handle;

	mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, mreq.offset);
	if (map == MAP_FAILED) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDisplayVideoBuffer() Cannot map dumb buffer, error: %s\n", strerror(errno));
		return -1;
	}

	req.handle = creq.handle;
	req.flags = DRM_CLOEXEC;
	if (drmIoctl(_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &req)) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDisplayVideoBuffer() Cannot exports a dma-buf, error: %s\n", strerror(errno));
		return -1;
	}
	handle->dmaBuf = dup(req.fd);

	renderTexture->handle = handle->handle;
	renderTexture->mapPtr = map;
	renderTexture->mapSize = fbSize;
	renderTexture->dmabuf = handle->dmaBuf;

	{
		EGLint attr[] = {
			EGL_WIDTH,                      (EGLint)width,
			EGL_HEIGHT,                     (EGLint)height,
			EGL_LINUX_DRM_FOURCC_EXT,       (EGLint)fourcc,
			EGL_DMA_BUF_PLANE0_FD_EXT,      (EGLint)renderTexture->dmabuf,
			EGL_DMA_BUF_PLANE0_OFFSET_EXT,  0,
			EGL_DMA_BUF_PLANE0_PITCH_EXT,   (EGLint)stride,
			EGL_DMA_BUF_PLANE1_FD_EXT,      (EGLint)renderTexture->dmabuf,
			EGL_DMA_BUF_PLANE1_OFFSET_EXT,  0,
			EGL_DMA_BUF_PLANE1_PITCH_EXT,   (EGLint)stride,
			EGL_NONE
		};
		renderTexture->image = eglCreateImageKHR(_eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)0, attr);
		if (renderTexture->image == EGL_NO_IMAGE_KHR) {
			mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDisplayVideoBuffer() failed to bind texture, error: %s\n", eglGetErrorStr(eglGetError()));
			goto fail;
		}
	}

	glGenTextures(1, &renderTexture->glTexture);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, renderTexture->glTexture);
	if (glGetError() != GL_NO_ERROR) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDisplayVideoBuffer() failed to bind texture, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, renderTexture->image);
	if (glGetError() != GL_NO_ERROR) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDisplayVideoBuffer() failed update texture, error: %s\n", eglGetErrorStr(eglGetError()));
		goto fail;
	}

	renderTexture->db = handle;
	handle->priv = renderTexture;

	return 0;

fail:

	if (renderTexture)
		releaseRenderTexture(renderTexture);

	return -1;
}

static RenderTexture *getRenderTexture(uint32_t pixelfmt, int width, int height) {
	DisplayVideoBuffer buffer;
	RenderTexture *renderTexture;

	if (getDisplayVideoBuffer(&buffer, pixelfmt, width, height) != 0) {
		return NULL;
	}

	renderTexture = (RenderTexture *)buffer.priv;
	renderTexture->db = NULL;

	return renderTexture;
}

static int releaseRenderTexture(RenderTexture *texture) {
	if (!_initialized || texture == NULL || _eglDisplay == NULL)
		return -1;

	if (texture->image) {
		eglDestroyImageKHR(_eglDisplay, texture->image);
		glDeleteTextures(1, &texture->glTexture);
	}

	if (texture->dmabuf)
		close(texture->dmabuf);

	{
		struct drm_gem_close req = {
			.handle = texture->handle,
		};
		drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);
	}

	free(texture);

	return 0;
}

static int releaseDisplayVideoBuffer(DisplayVideoBuffer *handle) {
	RenderTexture *renderTexture;

	if (!_initialized || handle == NULL)
		return -1;

	renderTexture = (RenderTexture *)handle->priv;
	if (renderTexture == NULL || _eglDisplay == NULL)
		return -1;

	if (releaseRenderTexture(renderTexture) != 0)
		return -1;

	handle->handle = 0;
	handle->priv = NULL;

	return 0;
}

static void drmFbDestroyCallback(struct gbm_bo *gbmBo, void *data) {
	DrmFb *drmFb = data;

	if (drmFb->fbId)
		drmModeRmFB(drmFb->fd, drmFb->fbId);

	free(drmFb);
}

static DrmFb *getDrmFb(struct gbm_bo *gbmBo) {
	uint32_t handles[4] = {}, pitches[4] = {}, offsets[4] = {};
	DrmFb *drmFb;
	int ret;

	drmFb = gbm_bo_get_user_data(gbmBo);
	if (drmFb)
		return drmFb;

	drmFb = malloc(sizeof(DrmFb));
	drmFb->fd = _fd;
	drmFb->gbmBo = gbmBo;

	pitches[0] = gbm_bo_get_stride(gbmBo);
	handles[0] = gbm_bo_get_handle(gbmBo).u32;
	ret = drmModeAddFB2(
	    drmFb->fd,
	    gbm_bo_get_width(gbmBo),
	    gbm_bo_get_height(gbmBo),
	    gbm_bo_get_format(gbmBo),
	    handles,
	    pitches,
	    offsets,
	    &drmFb->fbId,
	    0
	    );
	if (ret < 0) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] getDrmFb() failed add video buffer: %s\n", strerror(errno));
		free(drmFb);
		return NULL;
	}

	gbm_bo_set_user_data(gbmBo, drmFb, drmFbDestroyCallback);

	return drmFb;
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
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] config() Error wrong pixel format\n");
		return -1;
	}

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
	RenderTexture *renderTexture;
	uint32_t fourcc;
	uint32_t stride;
	uint32_t fbSize;
	float x, y;
	float cropLeft, cropRight, cropTop, cropBottom;
	int frame_width, frame_height;
	GLfloat coords[] = {
		0.0f,  1.0f,
		1.0f,  1.0f,
		0.0f,  0.0f,
		1.0f,  0.0f,
	};
	GLfloat position[8];

	frame_width = mpi->width;
	frame_height = mpi->height;

	if ((mpi->flags & 0x800000) || (mpi->w == 720 && (mpi->h == 576 || mpi->h == 480))) { // hack: anisotropic
		x = 1;
		y = 1;
	} else {
		x = (float)(mpi->w) / _modeInfo.hdisplay;
		y = (float)(mpi->h) / _modeInfo.vdisplay;
		if (x >= y) {
			y /= x;
			x = 1;
		} else {
			x /= y;
			y = 1;
		}
	}

	position[0] = -x;
	position[1] = -y;
	position[2] =  x;
	position[3] = -y;
	position[4] = -x;
	position[5] =  y;
	position[6] =  x;
	position[7] =  y;

	cropLeft = (float)(mpi->x) / frame_width;
	cropRight = (float)(mpi->w + mpi->x) / frame_width;
	cropTop = (float)(mpi->y) / frame_height;
	cropBottom = (float)(mpi->h + mpi->y) / frame_height;
	coords[0] = coords[4] = cropLeft;
	coords[2] = coords[6] = cropRight;
	coords[5] = coords[7] = cropTop;
	coords[1] = coords[3] = cropBottom;

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, position);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, coords);
	glEnableVertexAttribArray(1);

	if (_dce) {
		DisplayVideoBuffer *db = mpi->priv;
		renderTexture = (RenderTexture *)db->priv;
	} else if (!_renderTexture) {
		_renderTexture = renderTexture = getRenderTexture(mpi->imgfmt, frame_width, frame_height);
		if (!_renderTexture) {
			goto fail;
		}
	} else {
		renderTexture = _renderTexture;
	}

	if (!_dce) {
		uint8_t *srcPtr[4] = {};
		uint8_t *dstPtr[4] = {};
		int srcStride[4] = {};
		int dstStride[4] = {};
		uint8_t *dst;

		dst = renderTexture->mapPtr;
		if (mpi->imgfmt == IMGFMT_YV12) {
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
				_scaleCtx = sws_getContext(frame_width, frame_height, AV_PIX_FMT_YUV420P, frame_width, frame_height,
				                           AV_PIX_FMT_NV12, SWS_POINT, NULL, NULL, NULL);
				if (!_scaleCtx) {
					mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] put_image() Can not create scale context!\n");
					goto fail;
				}
			}
			sws_scale(_scaleCtx, (const uint8_t *const *)srcPtr, srcStride, 0, frame_height, dstPtr, dstStride);
		} else {
			mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] put_image() Not supported format!\n");
			goto fail;
		}
	}

	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, renderTexture->glTexture);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glFlush();

	return VO_TRUE;

fail:
	return VO_FALSE;
}

static void draw_osd(void) {
	// todo
}

static void flip_page() {
	struct gbm_bo *gbmBo;
	DrmFb *drmFb;

	//int old = GetTimerMS();
	eglSwapBuffers(_eglDisplay, _eglSurface);
	//printf("time: %d              \n", GetTimerMS() - old);

	gbmBo = gbm_surface_lock_front_buffer(_gbmSurface);
	drmFb = getDrmFb(gbmBo);

	if (drmModeSetPlane(_fd, _planeId, _crtcId,
	                    drmFb->fbId, 0,
	                    0, 0, _modeInfo.hdisplay, _modeInfo.vdisplay,
	                    0, 0, _modeInfo.hdisplay << 16, _modeInfo.vdisplay << 16
	                   )) {
		mp_msg(MSGT_VO, MSGL_FATAL, "[omap_drm_egl] flip() Failed set plane! %s\n", strerror(errno));
		goto fail;
	}

	gbm_surface_release_buffer(_gbmSurface, gbmBo);

	return;

fail:

	gbm_surface_release_buffer(_gbmSurface, gbmBo);
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

#endif
