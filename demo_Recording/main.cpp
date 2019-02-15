#include <signal.h>
#include <ZetaCameraInterface.h>

using namespace zeta;

zeta::ZetaCamera* mZetaCamera = NULL;

void signal_handler(int dunno)
{
	switch(dunno)
	{
		case SIGALRM:
		{
			
			mZetaCamera->stopRecord(0);
			sleep(1);
			system("sync");
			sleep(1);
			printf("[kinglaw] :::::: stop recording.\n");
		}
		break;
	}
}

int main()
{
	struct view_info sur; 
    struct src_info src;
    HwDisplay *mHwDisplay;
    int mHlay;
    int mLayerOpened;
	
	signal(SIGALRM, signal_handler);

	ZETA_RECT rect;
	rect.x = 0; rect.y = 0; rect.w = 480; rect.h = 270;
	mZetaCamera = new zeta::ZetaCamera(rect, 0);
        mZetaCamera->initCamera(1920, 1080, 480, 270, 30, 5, 1, 0);
	mZetaCamera->startPreview();
	sleep(1);
        mZetaCamera->setVideoRecordParam(1920, 1080, 30, 16*1024*1024);
        mZetaCamera->startRecord(0);
        sleep(60);
        mZetaCamera->loopRecord(0);
        sleep(60);
        mZetaCamera->stopRecord(0);
	while(1)
	{
            sleep(3);
            system("sync");
	}

}
