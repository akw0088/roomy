#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> 

#include "queue.h"
#include "types.h"


//3840 x 2160
#define FRAME_SIZE (3840 * 2160 * 4)

// defines
#define WMU_CAPTURE WM_USER + 1


// prototypes
LRESULT CALLBACK WinProc(HWND, UINT, WPARAM, LPARAM);
void draw_pixels(HDC hdc, int xoff, int yoff, int width, int height, int scalew, int scaleh, unsigned char *data);
char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

// TCP socket funcs
int listen_socket(int &sock, unsigned short port);
void read_socket(int &csock, char *buffer, unsigned int &size);
int connect_socket(char *ip_addr, unsigned short port, int &sock);
void RedirectIOToConsole(int debug);
int set_sock_options(int sock);
void handle_listen(int &sock, int &csock, char *ipstr);


static int server = 0xFFFFFFFF;

// globals (Large buffers cant be on stack)
queue_t squeue;
queue_t rqueue;

unsigned char rbuffer[FRAME_SIZE * 5];
unsigned char sbuffer[FRAME_SIZE * 5];
unsigned char recv_image[FRAME_SIZE];
unsigned char cap_image_last[FRAME_SIZE];
unsigned char data[1920 * 1080 * 2];


int GetScreenCapture(HWND hwnd, unsigned char *data)
{
	HDC hdcScreen = NULL;
	HDC hdc = NULL;
	HDC hTargetDC = NULL;
	HBITMAP hBitmap = NULL;

	hdcScreen = GetDC(NULL); // Get screen device conext
	hdc = GetDC(hwnd); // Get Window device context

	hTargetDC = CreateCompatibleDC(hdc);

	RECT rect;
	GetClientRect(hwnd, &rect);

	SetStretchBltMode(hdc, HALFTONE);

	StretchBlt(hdc, 0, 0, rect.right, rect.bottom, hdcScreen,	0, 0,
		GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SRCCOPY);

	hBitmap = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);

	SelectObject(hTargetDC, hBitmap);
	PrintWindow(hwnd, hTargetDC, PW_CLIENTONLY);

	GetBitmapBits(hBitmap, FRAME_SIZE, data);

	DeleteObject(hBitmap);
	DeleteObject(hTargetDC);
	ReleaseDC(NULL, hdcScreen);
	ReleaseDC(hwnd, hdc);

	return 0;
}

int CaptureAnImage(HWND hWnd)
{
	HDC hdcScreen;
	HDC hdcWindow;
	HDC hdcMemDC = NULL;
	HBITMAP hbmScreen = NULL;
	BITMAP bmpScreen;
	DWORD dwBytesWritten = 0;
	DWORD dwSizeofDIB = 0;
	HANDLE hFile = NULL;
	char* lpbitmap = NULL;
	HANDLE hDIB = NULL;
	DWORD dwBmpSize = 0;

	// Retrieve the handle to a display device context for the client 
	// area of the window. 
	hdcScreen = GetDC(NULL);
	hdcWindow = GetDC(hWnd);

	// Create a compatible DC, which is used in a BitBlt from the window DC.
	hdcMemDC = CreateCompatibleDC(hdcWindow);

	if (!hdcMemDC)
	{
		MessageBox(hWnd, "CreateCompatibleDC has failed", "Failed", MB_OK);
		goto done;
	}

	// Get the client area for size calculation.
	RECT rcClient;
	GetClientRect(hWnd, &rcClient);

	// This is the best stretch mode.
	SetStretchBltMode(hdcWindow, HALFTONE);

	// The source DC is the entire screen, and the destination DC is the current window (HWND).
	if (!StretchBlt(hdcWindow,
		0, 0,
		rcClient.right, rcClient.bottom,
		hdcScreen,
		0, 0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		SRCCOPY))
	{
		MessageBox(hWnd, "StretchBlt has failed", "Failed", MB_OK);
		goto done;
	}

	// Create a compatible bitmap from the Window DC.
	hbmScreen = CreateCompatibleBitmap(hdcWindow, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);

	if (!hbmScreen)
	{
		MessageBox(hWnd, "CreateCompatibleBitmap Failed", "Failed", MB_OK);
		goto done;
	}

	// Select the compatible bitmap into the compatible memory DC.
	SelectObject(hdcMemDC, hbmScreen);

	// Bit block transfer into our compatible memory DC.
	if (!BitBlt(hdcMemDC,
		0, 0,
		rcClient.right - rcClient.left, rcClient.bottom - rcClient.top,
		hdcWindow,
		0, 0,
		SRCCOPY))
	{
		MessageBox(hWnd, "BitBlt has failed", "Failed", MB_OK);
		goto done;
	}

	// Get the BITMAP from the HBITMAP.
	GetObject(hbmScreen, sizeof(BITMAP), &bmpScreen);

	BITMAPFILEHEADER   bmfHeader;
	BITMAPINFOHEADER   bi;

	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = bmpScreen.bmWidth;
	bi.biHeight = bmpScreen.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	dwBmpSize = ((bmpScreen.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmpScreen.bmHeight;

	// Starting with 32-bit Windows, GlobalAlloc and LocalAlloc are implemented as wrapper functions that 
	// call HeapAlloc using a handle to the process's default heap. Therefore, GlobalAlloc and LocalAlloc 
	// have greater overhead than HeapAlloc.
	hDIB = GlobalAlloc(GHND, dwBmpSize);
	lpbitmap = (char*)GlobalLock(hDIB);


	// Gets the "bits" from the bitmap, and copies them into a buffer 
	// that's pointed to by lpbitmap.
	GetDIBits(hdcWindow, hbmScreen, 0,
		(UINT)bmpScreen.bmHeight,
		lpbitmap,
		(BITMAPINFO*)&bi, DIB_RGB_COLORS);






	// Add the size of the headers to the size of the bitmap to get the total file size.
	dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	// Offset to where the actual bitmap bits start.
	bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);

	// Size of the file.
	bmfHeader.bfSize = dwSizeofDIB;

	// bfType must always be BM for Bitmaps.
	bmfHeader.bfType = 0x4D42; // BM.

	memcpy(data, lpbitmap, dwBmpSize);


	// Unlock and Free the DIB from the heap.
	GlobalUnlock(hDIB);
	GlobalFree(hDIB);

	// Clean up.
done:
	DeleteObject(hbmScreen);
	DeleteObject(hdcMemDC);
	ReleaseDC(NULL, hdcScreen);
	ReleaseDC(hWnd, hdcWindow);

	return 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HWND hwnd;
	MSG msg;
	char szAppName[] = TEXT("roomy");

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


	char path[MAX_PATH] = { 0 };
	GetCurrentDirectory(MAX_PATH, path);
	lstrcat(path, TEXT("\\roomy.ini"));

	server = GetPrivateProfileInt(TEXT("roomy"), TEXT("server"), 1, path);

	if (server)
	{
		// Note, hiding the window screws with the screen capture
		// might grab random hwnd through hwnd search or something
		ShowWindow(hwnd, SW_SHOW);
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
	static unsigned int screen_width;
	static unsigned int screen_height;
	static unsigned int screen_size;

	static int server_sock = -1;	// listen socket
	static int client_sock = -1;	// client socket from listen
	static int connect_sock = -1;	// outgoing connect sock
	static client_state_t connect_state;
	static client_state_t client_state;
	static int listen_port = 65535;
	static int connect_port = 65534;
	static int listen_mode = 0;
	static char connect_ip[MAX_PATH] = "127.0.0.1";
	static char listen_ip[MAX_PATH] = "127.0.0.1";
	static char client_ip[MAX_PATH] = "";


	switch (message)
	{
	case WM_CREATE:
	{
		WSAStartup(MAKEWORD(2, 0), &WSAData);
		RedirectIOToConsole(true);
		SetTimer(hwnd, 0, 500, NULL);


		char path[MAX_PATH] = { 0 };
		GetCurrentDirectory(MAX_PATH, path);
		lstrcat(path, TEXT("\\roomy.ini"));

		listen_port = GetPrivateProfileInt(TEXT("roomy"), TEXT("listen"), 65535, path);
		connect_port = GetPrivateProfileInt(TEXT("roomy"), TEXT("connect"), 65535, path);
		GetPrivateProfileString(TEXT("roomy"), TEXT("ip"), "127.0.0.1", connect_ip, MAX_PATH, path);
		listen_mode = GetPrivateProfileInt(TEXT("roomy"), TEXT("listen_mode"), 1, path);

		printf("listen_mode is %d\r\n", listen_mode);
		if (listen_mode == 1)
		{
			listen_socket(server_sock, listen_port);
			set_sock_options(server_sock);
		}

		break;
	}
	case WMU_CAPTURE:
	{
		unsigned int rsize = 0;

		screen_width = GetSystemMetrics(SM_CXSCREEN);
		screen_height = GetSystemMetrics(SM_CYSCREEN);
		screen_size = screen_width * screen_height * 4;



		if (server)
		{
			if (listen_mode)
			{
				if (client_sock == SOCKET_ERROR)
				{
					handle_listen(server_sock, client_sock, client_ip);
				}

				while (rqueue.size >= screen_size)
				{
					dequeue(&rqueue, rbuffer, screen_size);

					int ret = send(client_sock, (char *)rbuffer, screen_size, 0);
					if (ret == -1)
					{
						int err = WSAGetLastError();

						if (err != WSAEWOULDBLOCK)
						{
							printf("send returned -1 error %d\r\n", err);
							connect_state = DISCONNECTED;
							closesocket(connect_sock);
							connect_sock = -1;
						}
						break;
					}
					else if (ret > 0 && ret < screen_size)
					{
						// partial send occurred (full buffer?)
						enqueue_front(&rqueue, &rbuffer[ret], screen_size - ret);
					}
				}

				while (squeue.size >= screen_size && connect_sock != SOCKET_ERROR)
				{
					dequeue(&squeue, sbuffer, screen_size);

					printf("Atempting to send %d bytes\r\n", screen_size);
					int ret = send(connect_sock, (char *)sbuffer, screen_size, 0);
					if (ret == -1)
					{
						int err = WSAGetLastError();

						if (err != WSAEWOULDBLOCK)
						{
							printf("send returned -1 error %d\r\n", err);
							connect_state = DISCONNECTED;
							closesocket(connect_sock);
							connect_sock = -1;
						}
						break;
					}
					else if (ret > 0 && ret < screen_size)
					{
						// partial send occurred (full buffer?)
						enqueue_front(&squeue, &sbuffer[ret], screen_size - ret);
					}
				}

			}
		}
		else
		{
			// client mode
			if (client_sock == SOCKET_ERROR)
			{
				handle_listen(server_sock, client_sock, client_ip);
			}
			else
			{
				read_socket(client_sock, (char *)rbuffer, rsize);
				enqueue(&rqueue, rbuffer, rsize);
			}


			if (rqueue.size >= screen_size)
			{
				printf("Adding to view buffer\r\n");
				dequeue(&rqueue, data, screen_size);
				InvalidateRect(hwnd, NULL, 0);
				// client draws from data buffer, so thats it
			}

			if (connect_sock != SOCKET_ERROR)
			{
				read_socket(connect_sock, (char *)sbuffer, rsize);
				enqueue(&squeue, sbuffer, rsize);
			}

			if (squeue.size >= screen_size && connect_sock != SOCKET_ERROR)
			{
				printf("Adding to view buffer\r\n");
				dequeue(&squeue, data, screen_size);
				InvalidateRect(hwnd, NULL, 0);
			}
		}
		break;
	}
	case WM_TIMER:
	{
		// Dont attempt anything until video is streaming

		if (server)
		{
//
			static bool once = false;
			if (once == false)
			{
//				GetScreenCapture(hwnd, data);
				CaptureAnImage(hwnd);
				once = true;
			}

			// prevent duplicate frames
//			if (memcmp(data, cap_image_last, screen_size) != 0)
			{
				// send to both connect and listen queues
				printf("Adding frame to queue\r\n");
				enqueue(&squeue, data, screen_size);
				enqueue(&rqueue, data, screen_size);
				memcpy(cap_image_last, data, screen_size);
			}
//			else
			{
//				printf("Duplicate frame\r\n");
			}
		}


		if (listen_mode == 0)
		{
			if (connect_sock == -1 || connect_state == DISCONNECTED)
			{
				int ret = connect_socket(connect_ip, connect_port, connect_sock);
				if (ret == 0)
				{
					connect_state = CONNECTED;
				}
				else
				{
					connect_state = DISCONNECTED;
				}
			}
		}

		if (client_sock == SOCKET_ERROR)
		{
			client_state = DISCONNECTED;
		}
		else
		{
			client_state = CONNECTED;
		}

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

		hdc = BeginPaint(hwnd, &ps);

		GetClientRect(hwnd, &client_area);
		if (server == false)
		{
			draw_pixels(hdc, 0, 0,
				client_area.right, client_area.bottom,
				1920, 1080,
				data);

#if 0
			char state[MAX_PATH];

			sprintf(state, "Connect socket %d, connected=%d. client socket %d, connected=%d listen_mode=%d",
				connect_sock,
				connect_state == CONNECTED,
				client_sock,
				client_state == CONNECTED,
				listen_mode
			);
			TextOut(hdc, 50, 500, state, strlen(state));

			sprintf(state, "ip %s local %s client %s",
				connect_ip,
				listen_ip,
				client_ip
			);
			TextOut(hdc, 50, 550, state, strlen(state));
#endif
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


int listen_socket(int &sock, unsigned short port)
{
	struct sockaddr_in	servaddr;

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&servaddr, 0, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	bind(sock, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));
	listen(sock, 3);
	printf("listening on port %d\n", port);
	return 0;
}

int set_sock_options(int sock)
{
	unsigned long nonblock = 1;
	ioctlsocket(sock, FIONBIO, &nonblock);
	unsigned int sndbuf;
	unsigned int rcvbuf;

	socklen_t arglen = sizeof(int);

	getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &arglen);
	printf("SO_SNDBUF = %d\n", sndbuf);
	if (sndbuf < 65507)
	{
		sndbuf = 65507; //default 8192
		setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, sizeof(sndbuf));
		printf("Setting SO_SNDBUF to %d\n", sndbuf);
		getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, &arglen);
		printf("SO_SNDBUF = %d\n", sndbuf);
	}

	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &arglen);
	printf("SO_RCVBUF = %d\n", rcvbuf);
	if (rcvbuf < 65507)
	{
		rcvbuf = 65507; //default 8192
		setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, sizeof(rcvbuf));
		printf("Setting SO_RCVBUF to %d\n", rcvbuf);
		getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&rcvbuf, &arglen);
		printf("SO_RCVBUF = %d\n", rcvbuf);
	}


	int flag = 1;
	int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

	return 0;
}


int connect_socket(char *ip_addr, unsigned short port, int &sock)
{
	struct sockaddr_in	servaddr;
	int ret;

	if (sock == -1)
	{
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		set_sock_options(sock);

		memset(&servaddr, 0, sizeof(struct sockaddr_in));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = inet_addr(ip_addr);
		servaddr.sin_port = htons(port);

		// 3 way handshake
		printf("Attempting to connect to %s:%d\n", ip_addr, port);
		ret = connect(sock, (struct sockaddr *)&servaddr, sizeof(struct sockaddr_in));

		if (ret == SOCKET_ERROR)
		{
			ret = WSAGetLastError();

			switch (ret)
			{
			case WSAETIMEDOUT:
				printf("Fatal Error: Connecting to %s timed out.\n", ip_addr);
				break;
			case WSAECONNREFUSED:
				printf("Fatal Error: %s refused connection.\n(Server program is not running)\n", ip_addr);
				break;
			case WSAEHOSTUNREACH:
				printf("Fatal Error: router sent ICMP packet (destination unreachable)\n");
				break;
			case WSAEWOULDBLOCK:
				printf("Would block, using select()\r\n");
				return -1;
			default:
				printf("Fatal Error: %d\n", ret);
				break;
			}

			return -1;
		}

		printf("TCP handshake completed with %s\n", ip_addr);
		return 0;
	}
	else
	{

		struct timeval timeout;
		fd_set read_set;
		fd_set write_set;

		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
		FD_SET(sock, &read_set);
		FD_SET(sock, &write_set);

		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		int ret = select(sock + 1, &read_set, &write_set, NULL, &timeout);
		if (ret < 0)
		{
			printf("select() failed ");
			return -1;
		}
		else if (ret == 0)
		{
			static int count = 0;
			printf("select() timed out\r\n");
			count++;

			if (count == 5 * (200000))
			{
				closesocket(sock);
				sock = -1;
			}
			return -1;
		}

		if (FD_ISSET(sock, &read_set) || FD_ISSET(sock, &write_set))
		{
			unsigned int err = -1;
			int len = 4;
			int ret = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
			if (ret != 0)
			{
				printf("getsockopt failed\r\n");
			}

			if (err == 0)
			{
				printf("TCP handshake completed with %s\n", ip_addr);
				return 0;
			}
			else
			{
				closesocket(sock);
				sock = -1;
			}

		}
	}

	return -1;
}

void handle_listen(int &sock, int &csock, char *ipstr)
{
	struct sockaddr_in csockaddr;
	int addrlen = sizeof(sockaddr);

	csock = accept(sock, (sockaddr *)&csockaddr, &addrlen);
	if (csock != -1)
	{
		inet_ntop(AF_INET, &(csockaddr.sin_addr), ipstr, MAX_PATH);
		printf("Accepted connection from %s\n", ipstr);
		set_sock_options(csock);
	}
}


void read_socket(int &csock, char *buffer, unsigned int &size)
{
	if (csock == -1)
		return;

	size = 0;
	while (1)
	{
		int ret = 0;
		ret = recv(csock, buffer, FRAME_SIZE, 0);
		if (ret > 0)
		{
			printf("Read %d bytes from socket\r\n", ret);
			size += ret;
			if (size > 1920 * 1080 * 4)
			{
				printf("Read a frame already\r\n");
				break;
			}
		}
		else if (ret == 0)
		{
			break;
		}
		else if (ret < 0)
		{
			int ret = WSAGetLastError();

			if (ret == WSAEWOULDBLOCK)
			{
				return;
			}

			switch (ret)
			{
			case WSAETIMEDOUT:
				break;
			case WSAECONNREFUSED:
				break;
			case WSAEHOSTUNREACH:
				break;
			default:
				printf("Fatal Error: %d\n", ret);
				break;
			}

			csock = -1;
			csock = SOCKET_ERROR;
			break;
		}
	}
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
	StretchBlt(hdc, xoff, height, scalew, -scaleh, hdcMem, 0, 0, width, height, SRCCOPY);
	SelectObject(hdcMem, hOldBitmap);
	DeleteDC(hdcMem);
	DeleteObject(hBitmap);
}





