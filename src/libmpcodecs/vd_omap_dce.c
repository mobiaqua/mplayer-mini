/*
 * OMAP DCE hardware decoding for following video codecs:
 * h264, MPEG4, DivX 5, XviD, MPEG1, MPEG2, WMV9, VC-1
 *
 * Copyright (C) 2020 Pawel Kolodziejski
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#define xdc_target_types__ gnu/targets/std.h
#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/xdais/dm/xdm.h>
#include <ti/sdo/ce/video3/viddec3.h>
#include <ti/sdo/codecs/h264vdec/ih264vdec.h>
#include <ti/sdo/codecs/mpeg4vdec/impeg4vdec.h>
#include <ti/sdo/codecs/mpeg2vdec/impeg2vdec.h>
#include <ti/sdo/codecs/vc1vdec/ivc1vdec.h>
#include <libdce.h>
#include <libdrm/omap_drmif.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "vd_internal.h"
#include "dec_video.h"
#include "../mp_core.h"
#include "osdep/timer.h"
#include "../libvo/video_out.h"
#include "libavcodec/avcodec.h"

#define ALIGN2(value, align) (((value) + ((1 << (align)) - 1)) & ~((1 << (align)) - 1))

static const vd_info_t info = {
	"OMAP DCE decoder",
	"omap_dce",
	"",
	"",
	""
};

LIBVD_EXTERN(omap_dce)

typedef struct {
	int     handle;
} DisplayHandle;

typedef struct {
	void           *priv;
	struct omap_bo *bo;
	uint32_t       boHandle;
	int            dmaBuf;
	int            locked;
} DisplayVideoBuffer;

typedef struct {
	uint8_t  *data[4]; // array of pointers for video planes
	uint32_t stride[4]; // array of widths of video planes in bytes
	uint32_t pixelfmt; // pixel format of decoded video frame
	uint32_t width, height; // target aligned width and height
	uint32_t dx, dy, dw, dh; // border of decoded frame data
	int      interlaced;
	int      anistropicDVD;
} VideoFrame;

typedef struct {
	DisplayVideoBuffer      buffer;
	int                     index;
	int                     locked;
} FrameBuffer;

typedef struct {
	DisplayHandle handle;
	int (*getDisplayVideoBuffer)(DisplayVideoBuffer *handle, uint32_t pixelfmt, int width, int height);
	int (*releaseDisplayVideoBuffer)(DisplayVideoBuffer *handle);
} omap_dce_share_t;

omap_dce_share_t omap_dce_share;

static Engine_Handle              _codecEngine;
static VIDDEC3_Handle             _codecHandle;
static VIDDEC3_Params             *_codecParams;
static VIDDEC3_DynamicParams      *_codecDynParams;
static VIDDEC3_Status             *_codecStatus;
static XDM2_BufDesc               *_codecInputBufs;
static XDM2_BufDesc               *_codecOutputBufs;
static VIDDEC3_InArgs             *_codecInputArgs;
static VIDDEC3_OutArgs            *_codecOutputArgs;
static struct omap_device         *_omapDev;
static int                        _frameWidth;
static int                        _frameHeight;
static void                       *_inputBufPtr;
static int                        _inputBufSize;
static struct omap_bo             *_inputBufBo;
static int                        _numFrameBuffers;
static FrameBuffer                **_frameBuffers;
static unsigned int               _codecId;
static int                        _decoderLag;
static mp_image_t                 *_mpi;

static int init(sh_video_t *sh) {
	Engine_Error engineError;
	DisplayHandle displayHandle;
	Int32 codecError;
	int i;
	int dpbSizeInFrames = 0;
	_codecId = AV_CODEC_ID_NONE;
	_decoderLag = 0;

	sh->context = &omap_dce_share;
	displayHandle.handle = omap_dce_share.handle.handle;

	switch (sh->format) {
	case 0x10000005:
	case 0x00000005:
	case MKTAG('H','2','6','4'):
	case MKTAG('h','2','6','4'):
	case MKTAG('X','2','6','4'):
	case MKTAG('x','2','6','4'):
	case MKTAG('A','V','C','1'):
	case MKTAG('a','v','c','1'):
		_codecId = AV_CODEC_ID_H264;
		break;
	case 0x10000004:
	case 0x00000004:
	case MKTAG('F','M','P','4'):
	case MKTAG('f','m','p','4'):
	case MKTAG('M','P','4','V'):
	case MKTAG('m','p','4','v'):
	case MKTAG('X','V','I','D'):
	case MKTAG('x','v','i','d'):
	case MKTAG('X','v','i','D'):
	case MKTAG('X','V','I','X'):
	case MKTAG('D','X','5','0'):
	case MKTAG('D','X','G','M'):
		_codecId = AV_CODEC_ID_MPEG4;
		break;
	case 0x10000002:
	case 0x00000002:
	case MKTAG('m','p','g','2'):
	case MKTAG('M','P','G','2'):
	case MKTAG('M','7','0','1'):
	case MKTAG('m','2','v','1'):
	case MKTAG('m','2','2','v'):
	case MKTAG('m','p','g','v'):
		_codecId = AV_CODEC_ID_MPEG2VIDEO;
		break;
	case 0x10000001:
	case 0x00000001:
	case MKTAG('m','p','g','1'):
	case MKTAG('M','P','G','1'):
	case MKTAG('m','1','v','1'):
		_codecId = AV_CODEC_ID_MPEG1VIDEO;
		break;
	case MKTAG('W','V','C','1'):
	case MKTAG('w','v','c','1'):
	case MKTAG('V','C','-','1'):
	case MKTAG('v','c','-','1'):
		_codecId = AV_CODEC_ID_VC1;
		break;
	case MKTAG('W','M','V','3'):
		_codecId = AV_CODEC_ID_WMV3;
		break;
	default:
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_omap_dce] ------ Unsupported codec id: %08x, tag: '%4s' ------\n",
		       sh->format, (char *)&sh->format);
		return 0;
	}

	_frameWidth  = ALIGN2(sh->disp_w, 4);
	_frameHeight = ALIGN2(sh->disp_h, 4);

	dce_set_fd(displayHandle.handle);
	_omapDev = dce_init();
	if (!_omapDev) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Failed init dce!\n");
		goto fail;
	}

	_codecEngine = Engine_open((String)"ivahd_vidsvr", NULL, &engineError);
	if (!_codecEngine) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Failed open codec engine!\n");
		goto fail;
	}

	_numFrameBuffers = 3;

	switch (_codecId) {
	case AV_CODEC_ID_H264: {
		int maxDpb;

		_codecParams = (VIDDEC3_Params *)dce_alloc(sizeof(IH264VDEC_Params));

		switch (sh->level) {
			case 30:
				maxDpb = 8100;
				break;
			case 31:
				maxDpb = 18100;
				break;
			case 32:
				maxDpb = 20480;
				break;
			case 40:
			case 41:
				maxDpb = 32768;
				break;
			case 42:
				maxDpb = 34816;
				break;
			case 50:
				maxDpb = 110400;
				break;
			case 51:
			case 52:
				maxDpb = 184320;
				break;
			default:
				mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Not supported profile level: %d\n", sh->level);
				goto fail;
			}
			dpbSizeInFrames = FFMIN(16, maxDpb / ((sh->disp_w / 16) * (sh->disp_h / 16)));
			_numFrameBuffers = IVIDEO2_MAX_IO_BUFFERS;
			break;
		}
	case AV_CODEC_ID_MPEG4:
		_codecParams = (VIDDEC3_Params *)dce_alloc(sizeof(IMPEG4VDEC_Params));
		_numFrameBuffers = 4;
		break;
	case AV_CODEC_ID_MPEG1VIDEO:
	case AV_CODEC_ID_MPEG2VIDEO:
		_codecParams = (VIDDEC3_Params *)dce_alloc(sizeof(IMPEG2VDEC_Params));
		break;
	case AV_CODEC_ID_WMV3:
	case AV_CODEC_ID_VC1:
		_codecParams = (VIDDEC3_Params *)dce_alloc(sizeof(IVC1VDEC_Params));
		_numFrameBuffers = 4;
		break;
	default:
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Unsupported codec %d\n", _codecId);
		goto fail;
	}

	if (!_codecParams) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Error allocation with dce_alloc()\n");
		goto fail;
	}

	_numFrameBuffers += 2; // for display buffering

	_codecParams->maxWidth = _frameWidth;
	_codecParams->maxHeight = _frameHeight;
	_codecParams->maxFrameRate = 30000;
	_codecParams->maxBitRate = 10000000;
	_codecParams->dataEndianness = XDM_BYTE;
	_codecParams->forceChromaFormat = XDM_YUV_420SP;
	_codecParams->operatingMode = IVIDEO_DECODE_ONLY;
	_codecParams->displayDelay = IVIDDEC3_DISPLAY_DELAY_AUTO;
	_codecParams->displayBufsMode = IVIDDEC3_DISPLAYBUFS_EMBEDDED;
	_codecParams->inputDataMode = IVIDEO_ENTIREFRAME;
	_codecParams->outputDataMode = IVIDEO_ENTIREFRAME;
	_codecParams->numInputDataUnits = 0;
	_codecParams->numOutputDataUnits = 0;
	_codecParams->errorInfoMode = IVIDEO_ERRORINFO_OFF;
	_codecParams->metadataType[0] = IVIDEO_METADATAPLANE_NONE;
	_codecParams->metadataType[1] = IVIDEO_METADATAPLANE_NONE;
	_codecParams->metadataType[2] = IVIDEO_METADATAPLANE_NONE;

	switch (_codecId) {
	case AV_CODEC_ID_H264:
		_frameWidth = ALIGN2(_frameWidth + (32 * 2), 7);
		_frameHeight = _frameHeight + 4 * 24;
		_codecParams->size = sizeof(IH264VDEC_Params);
		((IH264VDEC_Params *)_codecParams)->dpbSizeInFrames = dpbSizeInFrames;//IH264VDEC_DPB_NUMFRAMES_AUTO;
		((IH264VDEC_Params *)_codecParams)->pConstantMemory = 0;
		((IH264VDEC_Params *)_codecParams)->bitStreamFormat = IH264VDEC_BYTE_STREAM_FORMAT;
		((IH264VDEC_Params *)_codecParams)->errConcealmentMode = IH264VDEC_APPLY_CONCEALMENT;
		((IH264VDEC_Params *)_codecParams)->temporalDirModePred = IH264VDEC_ENABLE_TEMPORALDIRECT;
		((IH264VDEC_Params *)_codecParams)->svcExtensionFlag = IH264VDEC_DISABLE_SVCEXTENSION;
		((IH264VDEC_Params *)_codecParams)->svcTargetLayerDID = IH264VDEC_TARGET_DID_DEFAULT;
		((IH264VDEC_Params *)_codecParams)->svcTargetLayerTID = IH264VDEC_TARGET_TID_DEFAULT;
		((IH264VDEC_Params *)_codecParams)->svcTargetLayerQID = IH264VDEC_TARGET_QID_DEFAULT;
		((IH264VDEC_Params *)_codecParams)->presetLevelIdc = IH264VDEC_MAXLEVELID;
		((IH264VDEC_Params *)_codecParams)->presetProfileIdc = IH264VDEC_PROFILE_ANY;
		((IH264VDEC_Params *)_codecParams)->detectCabacAlignErr = IH264VDEC_DISABLE_CABACALIGNERR_DETECTION;
		((IH264VDEC_Params *)_codecParams)->detectIPCMAlignErr = IH264VDEC_DISABLE_IPCMALIGNERR_DETECTION;
		((IH264VDEC_Params *)_codecParams)->debugTraceLevel = IH264VDEC_DEBUGTRACE_LEVEL0; // 0 - 3
		((IH264VDEC_Params *)_codecParams)->lastNFramesToLog = 0;
		((IH264VDEC_Params *)_codecParams)->enableDualOutput = IH264VDEC_DUALOUTPUT_DISABLE;
		((IH264VDEC_Params *)_codecParams)->processCallLevel = FALSE; // TRUE - for interlace
		((IH264VDEC_Params *)_codecParams)->enableWatermark = IH264VDEC_WATERMARK_DISABLE;
		((IH264VDEC_Params *)_codecParams)->decodeFrameType = IH264VDEC_DECODE_ALL;
		_codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_h264dec", _codecParams);
		mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[vd_omap_dce] Using ivahd_h264dec\n");
		break;
	case AV_CODEC_ID_MPEG4:
		_frameWidth = ALIGN2(_frameWidth + 32, 7);
		_frameHeight = _frameHeight + 32;
		_codecParams->size = sizeof(IMPEG4VDEC_Params);
		((IMPEG4VDEC_Params *)_codecParams)->outloopDeBlocking = IMPEG4VDEC_ENHANCED_DEBLOCK_ENABLE;
		((IMPEG4VDEC_Params *)_codecParams)->errorConcealmentEnable = IMPEG4VDEC_EC_ENABLE;
		((IMPEG4VDEC_Params *)_codecParams)->sorensonSparkStream = FALSE;
		((IMPEG4VDEC_Params *)_codecParams)->debugTraceLevel = IMPEG4VDEC_DEBUGTRACE_LEVEL0; // 0 - 2
		((IMPEG4VDEC_Params *)_codecParams)->lastNFramesToLog = IMPEG4VDEC_MINNUM_OF_FRAME_LOGS;
		((IMPEG4VDEC_Params *)_codecParams)->paddingMode = IMPEG4VDEC_MPEG4_MODE_PADDING;//IMPEG4VDEC_DIVX_MODE_PADDING;
		((IMPEG4VDEC_Params *)_codecParams)->enhancedDeBlockingQp = 15; // 1 - 31
		((IMPEG4VDEC_Params *)_codecParams)->decodeOnlyIntraFrames = IMPEG4VDEC_DECODE_ONLY_I_FRAMES_DISABLE;
		_codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_mpeg4dec", _codecParams);
		mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[vd_omap_dce] Using ivahd_mpeg4dec\n");
		break;
	case AV_CODEC_ID_MPEG1VIDEO:
	case AV_CODEC_ID_MPEG2VIDEO:
		_codecParams->size = sizeof(IMPEG2VDEC_Params);
		((IMPEG2VDEC_Params *)_codecParams)->ErrorConcealmentON = IMPEG2VDEC_EC_DISABLE; // IMPEG2VDEC_EC_ENABLE
		((IMPEG2VDEC_Params *)_codecParams)->outloopDeBlocking = IMPEG2VDEC_DEBLOCK_ENABLE;
		((IMPEG2VDEC_Params *)_codecParams)->debugTraceLevel = 0; // 0 - 4
		((IMPEG2VDEC_Params *)_codecParams)->lastNFramesToLog = 0;
		_codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_mpeg2vdec", _codecParams);
		mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[vd_omap_dce] Using ivahd_mpeg2vdec\n");
		break;
	case AV_CODEC_ID_WMV3:
	case AV_CODEC_ID_VC1:
		_frameWidth = ALIGN2(_frameWidth + (32 * 2), 7);
		_frameHeight = (ALIGN2(_frameHeight / 2, 4) * 2) + 2 * 40;
		_codecParams->size = sizeof(IVC1VDEC_Params);
		((IVC1VDEC_Params *)_codecParams)->errorConcealmentON = TRUE;
		((IVC1VDEC_Params *)_codecParams)->frameLayerDataPresentFlag = FALSE;
		((IVC1VDEC_Params *)_codecParams)->debugTraceLevel = 0; // 0 - 4
		((IVC1VDEC_Params *)_codecParams)->lastNFramesToLog = 0;
		_codecHandle = VIDDEC3_create(_codecEngine, (String)"ivahd_vc1vdec", _codecParams);
		mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[vd_omap_dce] Using ivahd_vc1dec\n");
		break;
	default:
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Unsupported codec %d\n", _codecId);
		goto fail;
	}

	if (!_codecHandle) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Error: VIDDEC3_create() failed\n");
		goto fail;
	}

	_codecStatus = (VIDDEC3_Status *)dce_alloc(sizeof(VIDDEC3_Status));
	_codecDynParams = (VIDDEC3_DynamicParams *)dce_alloc(sizeof(VIDDEC3_DynamicParams));
	_codecInputBufs = (XDM2_BufDesc *)dce_alloc(sizeof(XDM2_BufDesc));
	_codecOutputBufs = (XDM2_BufDesc *)dce_alloc(sizeof(XDM2_BufDesc));
	_codecInputArgs = (VIDDEC3_InArgs *)dce_alloc(sizeof(VIDDEC3_InArgs));
	_codecOutputArgs = (VIDDEC3_OutArgs *)dce_alloc(sizeof(VIDDEC3_OutArgs));
	if (!_codecDynParams || !_codecStatus || !_codecInputBufs || !_codecOutputBufs || !_codecInputArgs || !_codecOutputArgs) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Failed allocation with dce_alloc()\n");
		goto fail;
	}

	_codecDynParams->size = sizeof(VIDDEC3_DynamicParams);
	_codecDynParams->decodeHeader = XDM_DECODE_AU;
	_codecDynParams->displayWidth = 0;
	_codecDynParams->frameSkipMode = IVIDEO_NO_SKIP;
	_codecDynParams->newFrameFlag = XDAS_TRUE;
	_codecDynParams->lateAcquireArg = 0;
	if (_codecId == AV_CODEC_ID_MPEG4 || _codecId == AV_CODEC_ID_VC1 || _codecId == AV_CODEC_ID_WMV3) {
		_codecDynParams->lateAcquireArg = -1;
	}
	if (_codecId == AV_CODEC_ID_H264) {
		((IH264VDEC_DynamicParams *)_codecDynParams)->deblockFilterMode = IH264VDEC_DEBLOCK_DEFAULT;
	}

	_codecStatus->size = sizeof(VIDDEC3_Status);
	_codecInputArgs->size = sizeof(VIDDEC3_InArgs);
	_codecOutputArgs->size = sizeof(VIDDEC3_OutArgs);

	codecError = VIDDEC3_control(_codecHandle, XDM_SETPARAMS, _codecDynParams, _codecStatus);
	if (codecError != VIDDEC3_EOK) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() VIDDEC3_control(XDM_SETPARAMS) failed %d\n", codecError);
		goto fail;
	}
	codecError = VIDDEC3_control(_codecHandle, XDM_GETBUFINFO, _codecDynParams, _codecStatus);
	if (codecError != VIDDEC3_EOK) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() VIDDEC3_control(XDM_GETBUFINFO) failed %d\n", codecError);
		goto fail;
	}

	_inputBufBo = omap_bo_new(_omapDev, _frameWidth * _frameHeight, OMAP_BO_WC);
	if (!_inputBufBo) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Failed create input buffer\n");
		goto fail;
	}
	_inputBufPtr = omap_bo_map(_inputBufBo);
	_inputBufSize = omap_bo_size(_inputBufBo);

	_codecInputBufs->numBufs = 1;
	_codecInputBufs->descs[0].memType = XDM_MEMTYPE_RAW;
	_codecInputBufs->descs[0].buf = (XDAS_Int8 *)omap_bo_dmabuf(_inputBufBo);
	_codecInputBufs->descs[0].bufSize.bytes = omap_bo_size(_inputBufBo);
	dce_buf_lock(1, (size_t *)&(_codecInputBufs->descs[0].buf));

	_codecOutputBufs->numBufs = 2;
	_codecOutputBufs->descs[0].memType = XDM_MEMTYPE_RAW;
	_codecOutputBufs->descs[0].bufSize.bytes = _frameWidth * _frameHeight;
	_codecOutputBufs->descs[1].memType = XDM_MEMTYPE_RAW;
	_codecOutputBufs->descs[1].bufSize.bytes = _frameWidth * (_frameHeight / 2);

	_frameBuffers = (FrameBuffer **)calloc(_numFrameBuffers, sizeof(FrameBuffer *));
	for (i = 0; i < _numFrameBuffers; i++) {
		_frameBuffers[i] = (FrameBuffer *)calloc(1, sizeof(FrameBuffer));
		if (omap_dce_share.getDisplayVideoBuffer(&_frameBuffers[i]->buffer, IMGFMT_NV12, _frameWidth, _frameHeight) != 0) {
			mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Failed create output buffer\n");
			goto fail;
		}
		_frameBuffers[i]->index = i;
		_frameBuffers[i]->locked = 0;
	}

	_mpi = new_mp_image(_frameWidth, _frameHeight);
	if (!_mpi) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] init() Error: mpcodecs_get_image() failed\n");
		goto fail;
	}

	return mpcodecs_config_vo(sh, _frameWidth, _frameHeight, IMGFMT_NV12);

fail:

	uninit(sh);
	return 0;
}

static void uninit(sh_video_t *sh) {
	int i;

	if (_mpi) {
		free_mp_image(_mpi);
		_mpi = NULL;
	}

	if (_frameBuffers) {
		for (i = 0; i < _numFrameBuffers; i++) {
			if (_frameBuffers[i]) {
				if (_frameBuffers[i]->buffer.priv) {
					omap_dce_share.releaseDisplayVideoBuffer(&_frameBuffers[i]->buffer);
				}
				free(_frameBuffers[i]);
			}
		}
		free(_frameBuffers);
		_frameBuffers = NULL;
	}

	if (_inputBufBo) {
		omap_bo_del(_inputBufBo);
		_inputBufBo = NULL;
	}

	if (_codecHandle && _codecDynParams && _codecParams) {
		VIDDEC3_control(_codecHandle, XDM_FLUSH, _codecDynParams, _codecStatus);
	}

	if (_codecHandle) {
		VIDDEC3_delete(_codecHandle);
		_codecHandle = NULL;
	}
	if (_codecParams) {
		dce_free(_codecParams);
		_codecParams = NULL;
	}
	if (_codecStatus) {
		dce_free(_codecStatus);
		_codecStatus = NULL;
	}
	if (_codecDynParams) {
		dce_free(_codecDynParams);
		_codecDynParams = NULL;
	}
	if (_codecInputBufs) {
		dce_buf_unlock(1, (size_t *)&(_codecInputBufs->descs[0].buf));
		close((int)_codecInputBufs->descs[0].buf);
		dce_free(_codecInputBufs);
		_codecInputBufs = NULL;
	}
	if (_codecOutputBufs) {
		dce_free(_codecOutputBufs);
		_codecOutputBufs = NULL;
	}
	if (_codecInputArgs) {
		dce_free(_codecInputArgs);
		_codecInputArgs = NULL;
	}
	if (_codecOutputArgs) {
		dce_free(_codecOutputArgs);
		_codecOutputArgs = NULL;
	}

	if (_codecEngine) {
		Engine_close(_codecEngine);
		_codecEngine = NULL;
	}

	if (_omapDev) {
		dce_deinit(_omapDev);
		_omapDev = NULL;
	}
}

static int control(sh_video_t *sh, int cmd, void *arg, ...) {
	Int32 codecError;
	int i;

	switch (cmd) {
	case VDCTRL_QUERY_FORMAT: {
		int format = (*((int *)arg));
		if (format == IMGFMT_NV12) {
			return CONTROL_TRUE;
		} else {
			return CONTROL_FALSE;
		}
	}
	case VDCTRL_RESYNC_STREAM: {
		// flush codec engine
		//mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[vd_omap_dce] control: VDCTRL_RESYNC_STREAM\n");
		codecError = VIDDEC3_control(_codecHandle, XDM_FLUSH, _codecDynParams, _codecStatus);
		if (codecError != VIDDEC3_EOK) {
			mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_omap_dce] Error: VIDDEC3_control(XDM_FLUSH) failed %d\n", codecError);
			return CONTROL_ERROR;
		}
		_codecInputArgs->inputID = 0;
		_codecInputBufs->numBufs = 0;
		_codecInputBufs->descs[0].bufSize.bytes = 0;
		do {
			codecError = VIDDEC3_process(_codecHandle, _codecInputBufs, _codecOutputBufs,
			                             _codecInputArgs, _codecOutputArgs);
			if ((codecError == DCE_EXDM_UNSUPPORTED) ||
				(codecError == DCE_EIPC_CALL_FAIL) ||
				(codecError == DCE_EINVALID_INPUT))
			{
				break;
			}
		} while (codecError != XDM_EFAIL);

		for (i = 0; i < _numFrameBuffers; i++) {
			if (_frameBuffers[i]->buffer.priv && _frameBuffers[i]->locked) {
				dce_buf_unlock(1, (size_t *)&(_frameBuffers[i]->buffer.dmaBuf));
				_frameBuffers[i]->locked = 0;
			}
		}
		_decoderLag = 0;
		return CONTROL_OK;
	}
	case VDCTRL_QUERY_UNSEEN_FRAMES: {
		//mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[vd_omap_dce] control: VDCTRL_QUERY_UNSEEN_FRAMES: lag: %d\n", _decoderLag);
		return 0;//_decoderLag;
	}
	}

	return CONTROL_UNKNOWN;
}

static FrameBuffer *getBuffer(void) {
	int i;

	for (i = 0; i < _numFrameBuffers; i++) {
		if (_frameBuffers[i]->buffer.priv && !_frameBuffers[i]->locked) {
			if (!_frameBuffers[i]->buffer.locked) {
				dce_buf_lock(1, (size_t *)&(_frameBuffers[i]->buffer.dmaBuf));
				_frameBuffers[i]->locked = 1;
				return _frameBuffers[i];
			}
		}
	}

	mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] getBuffer() No free slots for output buffer\n");
	return NULL;
}

static void lockBuffer(FrameBuffer *fb) {
	if (_frameBuffers[fb->index]->locked) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] lockBuffer() Already locked frame buffer at index: %d\n", fb->index);
		return;
	}

	if (!_frameBuffers[fb->index]->buffer.priv) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] lockBuffer() Missing frame buffer at index: %d\n", fb->index);
		return;
	}

	dce_buf_lock(1, (size_t *)&(_frameBuffers[fb->index]->buffer.dmaBuf));

	_frameBuffers[fb->index]->locked = 1;
}

static void unlockBuffer(FrameBuffer *fb) {
	if (!fb) {
		return;
	}

	if (!_frameBuffers[fb->index]->locked) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] unlockBuffer() Already unlocked frame buffer at index: %d\n", fb->index);
		return;
	}

	if (!_frameBuffers[fb->index]->buffer.priv) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] unlockBuffer() Missing frame buffer at index: %d\n", fb->index);
		return;
	}

	dce_buf_unlock(1, (size_t *)&(_frameBuffers[fb->index]->buffer.dmaBuf));

	_frameBuffers[fb->index]->locked = 0;
}

static mp_image_t *decode(sh_video_t *sh, void *data, int len, int flags) {
	FrameBuffer *fb;
	Int32 codecError;
	int foundIndex = -1;
	int i;
	XDM_Rect *r;

	if (len <= 0)
		return NULL; // skipped frame

	fb = getBuffer();
	if (!fb) {
		mp_msg(MSGT_DECVIDEO, MSGL_FATAL, "[vd_omap_dce] decode() Failed get video buffer\n");
		return NULL;
	}

	memcpy(_inputBufPtr, data, len);

	_codecInputArgs->inputID = (XDAS_Int32)fb;
	_codecInputArgs->numBytes = len;

	_codecInputBufs->numBufs = 1;
	_codecInputBufs->descs[0].bufSize.bytes = len;

	_codecOutputBufs->descs[0].buf = (XDAS_Int8 *)fb->buffer.dmaBuf;
	_codecOutputBufs->descs[1].buf = (XDAS_Int8 *)fb->buffer.dmaBuf;

	memset(_codecOutputArgs->outputID, 0, sizeof(_codecOutputArgs->outputID));
	memset(_codecOutputArgs->freeBufID, 0, sizeof(_codecOutputArgs->freeBufID));

	//int old = GetTimerMS();
	codecError = VIDDEC3_process(_codecHandle, _codecInputBufs, _codecOutputBufs, _codecInputArgs, _codecOutputArgs);
	if (codecError != VIDDEC3_EOK) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_omap_dce] decode() VIDDEC3_process() status: %d, extendedError: %08x\n",
		       codecError, _codecOutputArgs->extendedError);
		if (XDM_ISFATALERROR(_codecOutputArgs->extendedError) ||
			(codecError == DCE_EXDM_UNSUPPORTED) ||
			(codecError == DCE_EIPC_CALL_FAIL) ||
			(codecError == DCE_EINVALID_INPUT)) {
			unlockBuffer(fb);
			return NULL;
		}
	}
	//printf("time: %d\n", GetTimerMS() - old);

	if (_codecOutputArgs->outBufsInUseFlag) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_omap_dce] decode() VIDDEC3_process() status: outBufsInUseFlag\n");
	}

	for (i = 0; _codecOutputArgs->freeBufID[i]; i++) {
		unlockBuffer((FrameBuffer *)_codecOutputArgs->freeBufID[i]);
	}

	for (i = 0; _codecOutputArgs->outputID[i]; i++) {
		foundIndex = i;
		break;
	}
	if (foundIndex == -1) {
		_decoderLag++;
		return NULL;
	}

	r = &_codecOutputArgs->displayBufs.bufDesc[0].activeFrameRegion;

	fb = (FrameBuffer *)_codecOutputArgs->outputID[foundIndex];

	_mpi->type = MP_IMGTYPE_TEMP;
	_mpi->imgfmt = IMGFMT_NV12;
	_mpi->width = _frameWidth;
	_mpi->height = _frameHeight;
	_mpi->x = r->topLeft.x;
	_mpi->y = r->topLeft.y;
	_mpi->w = r->bottomRight.x - r->topLeft.x;
	_mpi->h = r->bottomRight.y - r->topLeft.y;
	_mpi->priv = (void *)&fb->buffer;

	if (_codecId == AV_CODEC_ID_MPEG2VIDEO && _frameWidth == 720 && (_frameHeight == 576 || _frameHeight == 480)) {
		_mpi->flags |= 0x800000;
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].contentType == IVIDEO_INTERLACED) {
		_mpi->flags |= MP_IMGFIELD_INTERLACED;
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].contentType == IVIDEO_INTERLACED_TOPFIELD) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_omap_dce] decode() IVIDEO_INTERLACED_TOPFIELD\n");
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].contentType == IVIDEO_INTERLACED_BOTTOMFIELD) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_omap_dce] decode() IVIDEO_INTERLACED_BOTTOMFIELD\n");
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].topFieldFirstFlag) {
	}
	if (_codecOutputArgs->displayBufs.bufDesc[0].repeatFirstFieldFlag) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[vd_omap_dce] decode() repeatFirstFieldFlag\n");
	}

	return _mpi;
}
