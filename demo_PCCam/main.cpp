
#include <ZetaCameraInterface.h>
#include <ZetaMediaPlayInterface.h>


using namespace zeta;

int main (int argc, const char* argv[])
{
    struct view_info sur; 
    struct src_info src;
    HwDisplay *mHwDisplay;
    int mHlay;
    int mLayerOpened;

    sur.x = 0;
    sur.y = 0;
    sur.w = 480;
    sur.h = 854;
    mHwDisplay = HwDisplay::getInstance();
    mHlay = mHwDisplay->hwd_layer_request(&sur);
    mLayerOpened = true; 
	
    src.w = 272;
    src.h = 480;
    src.crop_x = 0;
    src.crop_y = 0;
    src.crop_w = 272;
    src.crop_h = 480;
    //src.color_space = DISP_YCC;
    src.format = HWC_FORMAT_YUV420PLANAR;//HWC_FORMAT_YUV420UVC
    mHwDisplay->hwd_layer_set_src(mHlay, &src);
    mHwDisplay->hwd_layer_open(mHlay); 

	ZETA_RECT rect;
	rect.x = 0; rect.y = 0; rect.w = 480; rect.h = 854;
	zeta::ZetaCamera* mZetaCamera = new zeta::ZetaCamera(rect, 0);
	mZetaCamera->initCamera(1920, 1080, 640, 360, 30, 10, 1, 90);
	mZetaCamera->startPreview();

	system("echo 0 > /sys/class/android_usb/android0/enable");
	sleep(1);
	system("echo 1d6b > /sys/class/android_usb/android0/idVendor");
	sleep(1);
	system("echo 0102 > /sys/class/android_usb/android0/idProduct");
	sleep(1);
	system("echo webcam > /sys/class/android_usb/android0/functions");
	sleep(1);
	system("echo 1 > /sys/class/android_usb/android0/enable");
	sleep(1);
	system("chmod 0666 /dev/video1");
	sleep(2);
	mZetaCamera->setUvcMode(1);

	while(1)
	{
		sleep(1);
	}
	return 0;
}

