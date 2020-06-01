
#include <unistd.h>
#include <ZetaDisplayInterface.h>
#include <hwdisplay.h>
#include <vdecoder.h>
#include <cdc_config.h>
#include <cedar/memoryAdapter.h>
using namespace zeta;
typedef struct CdxPacketS {
    void *buf;
    void *ringBuf;
    int buflen;
    int ringBufLen;
    int long pts;
    int long duration;
    int type;
    int length;
    unsigned int flags;
    int streamIndex;
    int long pcr;
    int infoVersion;
    void *info;
} CdxPacketT;

VideoDecoder* pVideoDec = NULL;
VideoStreamInfo mVideoInfo;
VideoPicture* pPrevPicture = NULL;
struct ScMemOpsS* mScMemOpsS = NULL;
HwDisplay* mHwDisplay = NULL;
int initDecoder() {
	int result;
	VConfig VideoConf;
	AddVDPlugin();
	mScMemOpsS = MemAdapterGetOpsS();
	pVideoDec = CreateVideoDecoder();
	if(pVideoDec == NULL) {
		return -1;
	}
	memset(&mVideoInfo, 0, sizeof(VideoStreamInfo));

	mVideoInfo.eCodecFormat          = VIDEO_CODEC_FORMAT_H264;
	mVideoInfo.nWidth                = 1280;
	mVideoInfo.nHeight               = 720;
	mVideoInfo.nFrameRate            = 30 * 1000;
	mVideoInfo.nFrameDuration        = (1000 * 1000 * 1000) / mVideoInfo.nFrameRate;
	mVideoInfo.nAspectRatio          = 0;
	mVideoInfo.bIs3DStream           = 0;
	mVideoInfo.nCodecSpecificDataLen = 0;
	mVideoInfo.pCodecSpecificData    = NULL;

	memset(&VideoConf, 0, sizeof(VConfig));
	
	VideoConf.eOutputPixelFormat                = PIXEL_FORMAT_NV21;
	VideoConf.nDeInterlaceHoldingFrameBufferNum = 0;
	VideoConf.nDisplayHoldingFrameBufferNum     = 3;
	VideoConf.nRotateHoldingFrameBufferNum      = 0;
	VideoConf.nDecodeSmoothFrameBufferNum       = 0;
	VideoConf.bDisable3D                        = 1;
	VideoConf.bScaleDownEn                      = 0;
	VideoConf.nFrameBufferNum                   = 0;
	VideoConf.bThumbnailMode                    = 0;
	VideoConf.bNoBFrames            	        = 0;
	VideoConf.nHorizonScaleDownRatio            = 1;
	VideoConf.nVerticalScaleDownRatio           = 1;
	VideoConf.nRotateDegree                     = 0;
	VideoConf.memops                            = mScMemOpsS;
	CdcMemOpen(VideoConf.memops);

	result = InitializeVideoDecoder(pVideoDec, &mVideoInfo, &VideoConf);
	if(result != 0) {
		DestroyVideoDecoder(pVideoDec);
		pVideoDec = NULL;
		return -1;
	}
	return 0;
}

int decodeOneFrameAndDisplay(char* buf, int size, unsigned int display_hdl) {
	int result;
	CdxPacketT mPacket;
	char *pSampleBuf = NULL;
	char *pTempBuf = NULL;
	VideoStreamDataInfo  mDataInfo;
	memset(&mPacket, 0, sizeof(mPacket));
	result = RequestVideoStreamBuffer(pVideoDec, size, (char **)&mPacket.buf, &mPacket.buflen, (char **)&mPacket.ringBuf, &mPacket.ringBufLen, 0);
	if(result != 0) {
		return -1;
	}
	
	if(mPacket.buflen == size) {
		pSampleBuf = (char *)mPacket.buf;
		memcpy(pSampleBuf, buf, size);
		mPacket.buflen = size;
		memset(&mDataInfo, 0, sizeof(VideoStreamDataInfo));
		mDataInfo.pData        = (char *)mPacket.buf;
		mDataInfo.nLength      = mPacket.buflen;
		mDataInfo.nPts         = 0;
		mDataInfo.nPcr         = 0;
		mDataInfo.bIsFirstPart = 1;
		mDataInfo.bIsLastPart  = 1;
		mDataInfo.nStreamIndex = 0;
		result = SubmitVideoStreamData(pVideoDec, &mDataInfo, mDataInfo.nStreamIndex);
		if(result != 0) {
			return -1;
		}
	} else {
		memcpy((char *)mPacket.buf, buf, size - mPacket.ringBufLen);
		memset(&mDataInfo, 0, sizeof(VideoStreamDataInfo));
		mDataInfo.pData        = (char *)mPacket.buf;
		mDataInfo.nLength      = mPacket.buflen;
		mDataInfo.nPts         = 0;
		mDataInfo.nPcr         = 0;
		mDataInfo.bIsFirstPart = 1;
		mDataInfo.bIsLastPart  = 0;
		mDataInfo.nStreamIndex = 0;

		result = SubmitVideoStreamData(pVideoDec, &mDataInfo, mDataInfo.nStreamIndex);
		if(result != 0) {
			return -1;
		}
		memcpy((char *)mPacket.ringBuf, buf + (size - mPacket.ringBufLen), mPacket.ringBufLen);

		memset(&mDataInfo, 0, sizeof(VideoStreamDataInfo));
		mDataInfo.pData        = (char *)mPacket.ringBuf;
		mDataInfo.nLength      = mPacket.ringBufLen;
		mDataInfo.nPts         = 0;
		mDataInfo.nPcr         = 0;
		mDataInfo.bIsFirstPart = 0;
		mDataInfo.bIsLastPart  = 1;
		mDataInfo.nStreamIndex = 0;

		result = SubmitVideoStreamData(pVideoDec, &mDataInfo, mDataInfo.nStreamIndex);
		if(result != 0) {
			return -1;
		}
		free(pSampleBuf);
		pSampleBuf = NULL;
	}
	DecodeVideoStream(pVideoDec, 0, 0, 0, 0);

	VideoPicture *pTempPicture = NULL;
	pTempPicture = RequestPicture(pVideoDec, 0);

	if(pTempPicture != NULL)
	{
		struct src_info mSrcInfo;
		libhwclayerpara_t mDisplayPicture;
		memset(&mSrcInfo, 0, sizeof(struct src_info));
		
		mSrcInfo.w      = pTempPicture->nWidth;
		mSrcInfo.h      = pTempPicture->nHeight;
		mSrcInfo.crop_x = pTempPicture->nLeftOffset;
		mSrcInfo.crop_y = pTempPicture->nTopOffset;
		mSrcInfo.crop_w = pTempPicture->nRightOffset - pTempPicture->nLeftOffset;
		mSrcInfo.crop_h = pTempPicture->nBottomOffset - pTempPicture->nTopOffset;
		mSrcInfo.format = HWC_FORMAT_YUV420VUC;
		mHwDisplay->hwd_layer_set_src(display_hdl, &mSrcInfo);

		memset(&mDisplayPicture, 0, sizeof(libhwclayerpara_t));
		mDisplayPicture.top_y    = (unsigned long)pTempPicture->phyYBufAddr;
		mDisplayPicture.top_c    = (unsigned long)pTempPicture->phyCBufAddr;
		mDisplayPicture.bottom_y = mDisplayPicture.top_c + (pTempPicture->nWidth * pTempPicture->nHeight / 4);
		mHwDisplay->hwd_layer_render(display_hdl, &mDisplayPicture);
		
		ReturnPicture(pVideoDec, pTempPicture);		
	}

	return 0;
}

int main(int argc, char **argv) {
	ViewInfo sur;
	sur.x = 0;
	sur.y = 0;
	sur.w = 480;
	sur.h = 854;
	ZetaDisplay* zeta_disp_ = new ZetaDisplay(0, &sur);
	mHwDisplay = zeta::HwDisplay::getInstance();
	mHwDisplay->hwd_layer_open(zeta_disp_->getHandle());

	initDecoder();
	char buf[1024*1024] = {0};
	FILE* fp = fopen("/tmp/test.h264", "rb");
	int frame_size = fread(buf, sizeof(char), 1024*1024, fp);

	decodeOneFrameAndDisplay(buf, frame_size, zeta_disp_->getHandle());

	return 0;
}
