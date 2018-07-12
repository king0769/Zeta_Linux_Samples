#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <memoryAdapter.h>
#include <vencoder.h>

static VideoEncoder *gVideoEnc;
static VencBaseConfig baseConfig;
static int gWidth = 1920;
static int gHeight = 1080;


#define ALIGN_4K(x) (((x) + (4095)) & ~(4095))
#define ALIGN_1K(x) (((x) + (1023)) & ~(1023))
#define ALIGN_32B(x) (((x) + (31)) & ~(31))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))
#define ALIGN_8B(x) (((x) + (7)) & ~(7))

#define REQ_COUNT 10

struct buffer
{
	void *start;
	size_t length;
	char *addrVirY;
	char *addrVirC;
};

static int fd = -1;
struct buffer *buffers = NULL;

struct v4l2_capability cap;
struct v4l2_format fmt;
struct v4l2_buffer buf[REQ_COUNT];
struct v4l2_requestbuffers req;
struct v4l2_buffer tmp_buf;
enum v4l2_buf_type type;

int H264EncodeOneFrame(unsigned char *AddrVirY, unsigned char *AddrVirC, FILE *fpH264)
{
	int result = 0;
	VencInputBuffer inputBuffer;
	VencOutputBuffer outputBuffer;
	int value;
	unsigned int head_num = 0;
	VencHeaderData sps_pps_data;

	VencH264Param h264Param;
	//* h264 param
	h264Param.bEntropyCodingCABAC = 1;
	h264Param.nBitrate = 6 * 1024 * 1024;
	h264Param.nFramerate = 30;
	h264Param.nCodingMode = VENC_FRAME_CODING;
	//h264Param.nCodingMode = VENC_FIELD_CODING;
	h264Param.nMaxKeyInterval = 30;
	h264Param.sProfileLevel.nProfile = VENC_H264ProfileMain;
	h264Param.sProfileLevel.nLevel = VENC_H264Level31;
	h264Param.sQPRange.nMinqp = 10;
	h264Param.sQPRange.nMaxqp = 40;
	memset(&baseConfig, 0, sizeof(VencBaseConfig));

	if (baseConfig.memops == NULL)
	{
		baseConfig.memops = MemAdapterGetOpsS();
		if (baseConfig.memops == NULL)
		{
			printf("MemAdapterGetOpsS failed\n");

			return -1;
		}
		CdcMemOpen(baseConfig.memops);
	}

	baseConfig.nInputWidth = gWidth;
	baseConfig.nInputHeight = gHeight;
	baseConfig.nStride = gWidth;
	baseConfig.nDstWidth = gWidth;
	baseConfig.nDstHeight = gHeight;
	baseConfig.eInputFormat = VENC_PIXEL_YVU420SP;

	if (gVideoEnc == NULL)
	{
		printf("get SPS PPS\n");
		gVideoEnc = VideoEncCreate((VENC_CODEC_TYPE)VENC_CODEC_H264);
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamH264Param, &h264Param);
		value = 0;
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamIfilter, &value);
		value = 0; //degree
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamRotation, &value);
		value = 0;
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamSetPSkip, &value);
		VideoEncInit(gVideoEnc, &baseConfig);
	}
	VideoEncGetParameter(gVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);

	fwrite(sps_pps_data.pBuffer, 1, sps_pps_data.nLength, fpH264);

	VencAllocateBufferParam bufferParam;
	memset(&bufferParam, 0, sizeof(VencAllocateBufferParam));
	memset(&inputBuffer, 0, sizeof(VencInputBuffer));

	bufferParam.nSizeY = baseConfig.nInputWidth * baseConfig.nInputHeight;
	bufferParam.nSizeC = baseConfig.nInputWidth * baseConfig.nInputHeight / 2;
	bufferParam.nBufferNum = 1;
	AllocInputBuffer(gVideoEnc, &bufferParam);

	GetOneAllocInputBuffer(gVideoEnc, &inputBuffer);

	memcpy(inputBuffer.pAddrVirY, AddrVirY, baseConfig.nInputWidth * baseConfig.nInputHeight);
	memcpy(inputBuffer.pAddrVirC, AddrVirC, baseConfig.nInputWidth * baseConfig.nInputHeight / 2);
	inputBuffer.bEnableCorp = 0;
	inputBuffer.sCropInfo.nLeft = 240;
	inputBuffer.sCropInfo.nTop = 240;
	inputBuffer.sCropInfo.nWidth = 240;
	inputBuffer.sCropInfo.nHeight = 240;
	FlushCacheAllocInputBuffer(gVideoEnc, &inputBuffer);
	AddOneInputBuffer(gVideoEnc, &inputBuffer);
	if (VENC_RESULT_OK != VideoEncodeOneFrame(gVideoEnc))
	{
		printf("VideoEncodeOneFrame failed.\n");
		return -1;
	}
	AlreadyUsedInputBuffer(gVideoEnc, &inputBuffer);
	ReturnOneAllocInputBuffer(gVideoEnc, &inputBuffer);

	GetOneBitstreamFrame(gVideoEnc, &outputBuffer);
	if (outputBuffer.nSize0 > 0)
	{
		printf("write pData0\n");
		fwrite(outputBuffer.pData0, 1, outputBuffer.nSize0, fpH264);
	}
	if (outputBuffer.nSize1 > 0)
	{
		printf("write pData1\n");
		fwrite(outputBuffer.pData1, 1, outputBuffer.nSize1, fpH264);
	}
	//	outputBuffer.pData0;
	//	outputBuffer.nSize0;
	//	outputBuffer.pData1;
	//	outputBuffer.nSize1;

	FreeOneBitStreamFrame(gVideoEnc, &outputBuffer);

	if (baseConfig.memops != NULL)
	{
		CdcMemClose(baseConfig.memops);
		baseConfig.memops = NULL;
	}
	VideoEncDestroy(gVideoEnc);
	gVideoEnc = NULL;

	return 0;
}


int main(int argc, char **argv)
{
	int iCounterCamera = 0;
	int iCounter100frame = 0;
	struct v4l2_fmtdesc fmtd;
	int ret = 0;
	int index = 0;
	struct v4l2_format fmt2;

	if ((fd = open("/dev/video0", O_RDWR | O_NONBLOCK, 0)) < 0)
	{
		printf("open video0 failed.\n");
		return -1;
	}

	memset(&fmtd, 0, sizeof(fmtd));
	fmtd.index = 0;
	fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	while ((ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtd)) == 0)
	{
		fmtd.index++;
	}
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
	{
		printf("Error:VIDIOC_QUERYCAP\n");
		return -1;
	}

	if (ioctl(fd, VIDIOC_S_INPUT, &index) < 0)
	{
		printf("Error:VIDIOC_S_INPUT\n");
		return -1;
	}

	fmt2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_G_FMT, &fmt2);
	printf("VIDIOC_G_FMT ret=%d \n", ret);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = gWidth;
	fmt.fmt.pix.height = gHeight;

	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
	if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
	{
		printf("Error:VIDIOC_S_FMT\n");
		return -1;
	}

	req.count = REQ_COUNT;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0)
	{
		printf("Error:VIDIOC_REQBUFS\n");
		return -1;
	}

	buffers = calloc(req.count, sizeof(*buffers));

	for (int i = 0; i < req.count; i++)
	{
		buf[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf[i].memory = V4L2_MEMORY_MMAP;
		buf[i].index = i;
		if (ioctl(fd, VIDIOC_QUERYBUF, buf + i) < 0)
		{
			printf("Error:VIDIOC_QUERYBUF\n");
			return -1;
		}

		buffers[i].length = buf[i].length;
		buffers[i].start = mmap(NULL, buf[i].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf[i].m.offset);

		buf[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf[i].memory = V4L2_MEMORY_MMAP;
		buf[i].index = i;

		if (ioctl(fd, VIDIOC_QBUF, buf + i) < 0)
		{
			printf("Error: VIDIOC_QBUF\n");
			return -1;
		}

		buffers[i].addrVirY = buffers[i].start;
		buffers[i].addrVirC = buffers[i].addrVirY + ALIGN_16B(gWidth) * ALIGN_16B(gHeight);
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_STREAMON, &type) < 0)
	{
		printf("Error: VIDIOC_STREAMON\n");
		return -1;
	}

	FILE *fpYUV = NULL;
	FILE *fpH264 = NULL;
	char yuv_path[128];
	char h264_path[128];
	for (int i = 0; i < req.count; i++)
	{
		struct v4l2_buffer buf;

		/*帧出列*/
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		ioctl(fd, VIDIOC_DQBUF, &buf);

		memset(yuv_path, 0, 128);
		memset(h264_path, 0, 128);
		sprintf(yuv_path, "/mnt/extsd/src%04d.yuv", buf.index);
		sprintf(h264_path, "/mnt/extsd/dst%04d.h264", buf.index);
		fpYUV = fopen(yuv_path, "w");
		fwrite(buffers[buf.index].addrVirY, 1, gWidth * gHeight, fpYUV);
		fwrite(buffers[buf.index].addrVirC, 1, gWidth * gHeight / 2, fpYUV);
		fpH264 = fopen(h264_path, "w");
		H264EncodeOneFrame(buffers[buf.index].addrVirY, buffers[buf.index].addrVirC, fpH264);
		fclose(fpYUV);
		fpYUV = NULL;
		fclose(fpH264);
		fpH264 = NULL;

		/*buf入列*/
		ioctl(fd, VIDIOC_QBUF, &buf);
	}

	if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
	{
		printf("Error:VIDIOC_STREAMOFF\n");

		// return 0;
		return;
	}

	for (int i = 0; i < req.count; i++)
	{
		munmap(buffers[i].start, buf[i].length);
	}

	close(fd);

	return 0;
}
