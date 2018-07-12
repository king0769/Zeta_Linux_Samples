#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <sys/time.h>
#include <ZetaDisplayInterface.h>
#include <ZetaCameraInterface.h>


using namespace zeta;
static BITMAP bmp_bkgnd;

static int HelloWinProc (HWND hWnd, int message, WPARAM wParam, LPARAM lParam)
{
HDC hdc;
static HWND hwnd;
static int i = 0;
switch (message) {
case MSG_PAINT:
hdc = BeginPaint (hWnd);
TextOut (hdc, 320 / 2 - 50, 240 / 2 - 10, "Hello world!");
EndPaint (hWnd, hdc);
return 0;
case 888888888888888888:
        {

            HDC hdc2 = (HDC)wParam;
            const RECT* clip = (const RECT*) lParam;
            BOOL fGetDC = FALSE;
            RECT rcTemp;

            if (hdc2 == 0) {
                hdc2 = GetClientDC (hWnd);
                fGetDC = TRUE;
            }

            if (clip) {
                rcTemp = *clip;
                ScreenToClient (hWnd, &rcTemp.left, &rcTemp.top);
                ScreenToClient (hWnd, &rcTemp.right, &rcTemp.bottom);
                IncludeClipRect (hdc2, &rcTemp);
            }

            FillBoxWithBitmap (HDC_SCREEN, 0, 0, 0, 0, &bmp_bkgnd);

            if (fGetDC)
                ReleaseDC (hdc2);
            return 0;
        }
        break;
case MSG_CREATE:
	hwnd = CreateWindowEx("static", "",
			WS_CHILD | WS_VISIBLE,
			WS_EX_NONE,
			123,
			0, 0, 50, 50,
			hWnd, NULL);
	SetWindowBkColor(hwnd, RGBA2Pixel(HDC_SCREEN, 0x00, 0x00, 0xff, 0x10));
break;
case MSG_KEYDOWN:
	if (i) {
	SetWindowBkColor(hwnd, RGBA2Pixel(HDC_SCREEN, 0x00, 0x00, 0xff, 0x10));
	} else {
	SetWindowBkColor(hwnd, RGB2Pixel(HDC_SCREEN, 0x00, 0xff, 0xff));
	}
	i = 1-i;
break;
case MSG_CLOSE:
DestroyMainWindow (hWnd);
PostQuitMessage (hWnd);
return 0;

}
return DefaultMainWinProc (hWnd, message, wParam, lParam);
}
int MiniGUIMain (int argc, const char* argv[])
{
MSG Msg;
HWND hMainWnd;
MAINWINCREATE CreateInfo;
#ifdef _MGRM_PROCESSES
JoinLayer (NAME_DEF_LAYER , "helloworld" , 0 , 0);
#endif
CreateInfo.dwStyle = WS_VISIBLE | WS_BORDER | WS_CAPTION;
CreateInfo.dwExStyle = WS_EX_NONE;
CreateInfo.spCaption = "HelloWorld";
CreateInfo.hMenu = 0;
CreateInfo.hCursor = GetSystemCursor (0);
CreateInfo.hIcon = 0;
CreateInfo.MainWindowProc = HelloWinProc;
CreateInfo.lx = 0;
CreateInfo.ty = 0;
CreateInfo.rx = 320;
CreateInfo.by = 240;
CreateInfo.iBkColor = RGBA2Pixel(HDC_SCREEN, 0xff, 0xff, 0xff, 0x00);
CreateInfo.dwAddData = 0;
CreateInfo.hHosting = HWND_DESKTOP;
hMainWnd = CreateMainWindow (&CreateInfo);
//if (LoadBitmap (HDC_SCREEN, &bmp_bkgnd, "/res/topbar/playback.png"))
//         return 1;
if (hMainWnd == HWND_INVALID)
return -1;
ShowWindow (hMainWnd, SW_SHOWNORMAL);

while (GetMessage (&Msg, hMainWnd)) {
	fprintf(stderr, "msg\n");
	TranslateMessage (&Msg);
	DispatchMessage (&Msg);
}
UnloadBitmap(&bmp_bkgnd);
MainWindowThreadCleanup (hMainWnd);
return 0;
}
#ifndef _MGRM_PROCESSES
#include <minigui/dti.c>
#endif

