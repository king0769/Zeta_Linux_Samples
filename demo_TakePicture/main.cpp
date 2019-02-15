#include <signal.h>
#include <ZetaCameraInterface.h>
#include <thread>
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
	std::thread t_freememory(
        []() {
            while(true) {
                system("echo 1 > /proc/sys/vm/overcommit_memory");
                system("echo 0 > /proc/sys/vm/overcommit_ratio");
                system("echo 0 > /proc/sys/vm/panic_on_oom");
                system("sync && echo 3 > /proc/sys/vm/drop_caches");
                sleep(3);
            }
        }
    );
    t_freememory.detach();

	struct view_info sur; 
    struct src_info src;
    HwDisplay *mHwDisplay;
    int mHlay;
    int mLayerOpened;
	system("mount /dev/mmcblk0 /mnt/extsd/");
	system("mount /dev/mmcblk0p1 /mnt/extsd/");
	system("mkdir /mnt/extsd/video");
	signal(SIGALRM, signal_handler);

	ZETA_RECT rect;
	rect.x = 0; rect.y = 0; rect.w = 480; rect.h = 270;
	mZetaCamera = new zeta::ZetaCamera(rect, 0);
	mZetaCamera->initCamera(1920, 1080, 480, 270, 30, 5, 1, 0);
	mZetaCamera->startPreview();
	sleep(1);
	mZetaCamera->takePicture();
	
	while(1)
	{
        sleep(1);
        system("sync");
	}

}
