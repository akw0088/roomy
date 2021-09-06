#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 

#include "queue.h"
#include "types.h"
#include "roomy.h"

// defines
#define WMU_CAPTURE WM_USER + 1


// prototypes
LRESULT CALLBACK WinProc(HWND, UINT, WPARAM, LPARAM);
void draw_pixels(HDC hdc, int xoff, int yoff, int width, int height, int scalew, int scaleh, unsigned char *data);
void RedirectIOToConsole(int debug);

int GetScreenCapture(HWND hwnd, unsigned char *data, unsigned int &size)
{
	HDC hdcScreen = NULL;
	HDC hdc = NULL;
	HDC hTargetDC = NULL;
	HBITMAP hBitmap = NULL;
	HWND hDesktop;

	hDesktop = GetDesktopWindow();

	hdcScreen = GetDC(NULL); // Get screen device conext
	hdc = GetDC(hDesktop); // Get Window device context

	hTargetDC = CreateCompatibleDC(hdc);

	RECT rect;
	GetClientRect(hDesktop, &rect);

	SetStretchBltMode(hdc, HALFTONE);

	StretchBlt(hdc, 0, 0, rect.right, rect.bottom, hdcScreen,	0, 0,
		GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SRCCOPY);

	hBitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);

	SelectObject(hTargetDC, hBitmap);

	// Bit block transfer into our compatible memory DC.
	BitBlt(hTargetDC, 0, 0,
		rect.right - rect.left,
		rect.bottom - rect.top,
		hdc, 0, 0, SRCCOPY);


	BITMAPINFOHEADER   bi;

	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = (rect.right - rect.left);
	bi.biHeight = (rect.bottom - rect.top);
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	// GetBitmapBits is device dependent, so might give some weirdness across networks
	//GetBitmapBits(hBitmap, (rect.right - rect.left) * (rect.bottom - rect.top) * 4, data);

	GetDIBits(hdc, hBitmap, 0, (rect.bottom - rect.top), data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
	size = (rect.right - rect.left) * (rect.bottom - rect.top) * bi.biBitCount / 8;
	DeleteObject(hBitmap);
	DeleteObject(hTargetDC);
	ReleaseDC(NULL, hdcScreen);
	ReleaseDC(hwnd, hdc);

	return 0;
}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HWND hwnd;
	MSG msg;
	char szAppName[MAX_PATH] = TEXT("roomy");
	char path[MAX_PATH] = { 0 };

	GetCurrentDirectory(MAX_PATH, path);
	lstrcat(path, TEXT("\\roomy.ini"));
	int server = GetPrivateProfileInt(TEXT("roomy"), TEXT("server"), 1, path);
	int debug = GetPrivateProfileInt(TEXT("roomy"), TEXT("debug"), 0, path);

	// 2 will suppress the logging also
	if (debug != 2)
	{
		RedirectIOToConsole(debug);
	}

	if (server)
	{
		lstrcat(szAppName, " - server");
	}
	else
	{
		lstrcat(szAppName, " - client");
	}

	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WinProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(GetModuleHandle(NULL), IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = szAppName;

	RegisterClass(&wc);

	// Create the window
	hwnd = CreateWindow(szAppName, szAppName,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, hInstance, 0);

	if (server)
	{
		// Note, hiding the window screws with the screen capture
		// might grab random hwnd through hwnd search or something
		ShowWindow(hwnd, 0);
	}
	else
	{
		ShowWindow(hwnd, SW_SHOW);
	}

	UpdateWindow(hwnd);

	while (TRUE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (GetMessage(&msg, NULL, 0, 0) > 0)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				break;
			}
		}
		else
		{
			SendMessage(hwnd, WMU_CAPTURE, 0, 0);
		}
	}
	return msg.wParam;
}


LRESULT CALLBACK WinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static WSADATA	WSAData;
	static RECT		client_area;
	static Roomy roomy;


	switch (message)
	{
	case WM_CREATE:
	{
		int screen_width = GetSystemMetrics(SM_CXSCREEN);
		int screen_height = GetSystemMetrics(SM_CYSCREEN);



		WSAStartup(MAKEWORD(2, 0), &WSAData);
		roomy.init((void *)hwnd, NULL, screen_width, screen_height);
		SetTimer(hwnd, 0, 500, NULL);
		break;
	}
	case WMU_CAPTURE:
	{
		roomy.capture();
		InvalidateRect(hwnd, NULL, 0);
		break;
	}
	case WM_TIMER:
	{
		unsigned int data_size = 0;

		if (roomy.server)
		{
			GetScreenCapture(hwnd, roomy.get_data(), data_size);
		}
		roomy.step(data_size);
		break;
	}
	case WM_SIZE:
	{
		int	width, height;

		width = LOWORD(lParam);
		height = HIWORD(lParam);

		break;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC		hdc;

		GetClientRect(hwnd, &client_area);
		hdc = BeginPaint(hwnd, &ps);

		if (roomy.server == false)
		{
			draw_pixels(hdc, 0, 0,
				GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
				client_area.right, client_area.bottom,
				roomy.get_data());
		}
		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}




void draw_pixels(HDC hdc, int xoff, int yoff, int width, int height, int scalew, int scaleh, unsigned char *data)
{
	HBITMAP hBitmap, hOldBitmap;
	HDC hdcMem;

	hBitmap = CreateCompatibleBitmap(hdc, width, height);
	SetBitmapBits(hBitmap, sizeof(int) * width * height, data);
	hdcMem = CreateCompatibleDC(hdc);
	hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

	// This scaling is a little strange because Stretch maintains aspect ratios
	StretchBlt(hdc, xoff, scaleh, scalew, -scaleh, hdcMem, 0, 0, width, height, SRCCOPY);
	SelectObject(hdcMem, hOldBitmap);
	DeleteDC(hdcMem);
	DeleteObject(hBitmap);
}





