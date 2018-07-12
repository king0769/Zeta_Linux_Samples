
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


	ZETA_RECT rect;
	rect.x = 0; rect.y = 0; rect.w = 480; rect.h = 854;

    static int cnt = 1;
	while(1)
	{
            
        	zeta::ZetaCamera* mZetaCamera = new zeta::ZetaCamera(rect, (cnt%2));
	    	mZetaCamera->initCamera(640, 480, 640, 480, 30, 10, 1, 90);
	    	mZetaCamera->startPreview();
        	sleep(3);
        	mZetaCamera->stopPreview();
            sleep(3);
        	delete mZetaCamera;
        	mZetaCamera = NULL;
        	cnt++;
            sleep(3);
            
            
	}
	return 0;
}

