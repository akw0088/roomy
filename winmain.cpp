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
void handle_accepted(int &csock, char *buffer, int &size);
int connect_socket(char *ip_addr, unsigned short port, int &sock);
void RedirectIOToConsole(int debug);
int set_sock_options(int sock);
void handle_listen(int &sock, int &csock, char *ipstr);

// globals (Large buffers cant be on stack)
queue_t squeue;
queue_t rqueue;

unsigned char rbuffer[FRAME_SIZE * 10];
unsigned char sbuffer[FRAME_SIZE * 10];
unsigned char recv_image[FRAME_SIZE];
unsigned char cap_image_last[FRAME_SIZE];
unsigned char data[FRAME_SIZE];
BOOL server = true;


int GetScreenCapture(HWND hwnd, unsigned char *data)
{
	HDC hdcScreen;
	HDC hdc;
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
	static unsigned int screen_width;
	static unsigned int screen_height;
	static unsigned int screen_size;
	static bool init = false;

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
		RedirectIOToConsole(false);
		SetTimer(hwnd, 0, 500, NULL);

		screen_width = GetSystemMetrics(SM_CXSCREEN);
		screen_height = GetSystemMetrics(SM_CYSCREEN);
		screen_size = screen_width * screen_height * 4;

		char path[MAX_PATH] = { 0 };
		GetCurrentDirectory(MAX_PATH, path);
		lstrcat(path, TEXT("\\roomy.ini"));

		listen_port = GetPrivateProfileInt(TEXT("roomy"), TEXT("listen"), 65535, path);
		connect_port = GetPrivateProfileInt(TEXT("roomy"), TEXT("connect"), 65534, path);
		GetPrivateProfileString(TEXT("roomy"), TEXT("ip"), "127.0.0.1", connect_ip, MAX_PATH, path);
		listen_mode = GetPrivateProfileInt(TEXT("roomy"), TEXT("listen_mode"), 0, path);

		printf("listen_mode is %d\r\n", listen_mode);
		if (listen_mode == 0)
		{
			listen_socket(server_sock, listen_port);
			set_sock_options(server_sock);
		}
		break;
	}
	case WMU_CAPTURE:
	{
		int rsize = 0;

		if (init == false)
			return 0;

		if (server)
		{
			if (listen_mode)
			{
				if (client_sock == SOCKET_ERROR)
				{
					handle_listen(server_sock, client_sock, client_ip);

					static bool once = false;
					if (once == false)
					{
						once = true;
					}
				}
				else
				{
					handle_accepted(client_sock, (char *)rbuffer, rsize);
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
			}
			else
			{
				while (squeue.size >= screen_size && connect_sock != SOCKET_ERROR)
				{
					int size = dequeue(&squeue, sbuffer, screen_size);

					int ret = send(connect_sock, (char *)sbuffer, size, 0);
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

				static bool once = false;
				if (once == false)
				{
					once = true;
				}
			}
			else
			{
				handle_accepted(client_sock, (char *)rbuffer, rsize);
				enqueue(&rqueue, rbuffer, rsize);
			}


			while (rqueue.size >= screen_size)
			{
				dequeue(&rqueue, data, screen_size);
				// client draws from data buffer, so thats it
			}

			if (connect_sock != SOCKET_ERROR)
			{
				// really just reads the socket, bad name
				handle_accepted(connect_sock, (char *)sbuffer, rsize);
				enqueue(&squeue, sbuffer, rsize);
			}

			while (squeue.size >= screen_size && connect_sock != SOCKET_ERROR)
			{
				dequeue(&squeue, data, screen_size);
			}
		}
		break;
	}
	case WM_TIMER:
	{
		// Dont attempt anything until video is streaming
		if (init == false)
			return 0;

		if (server == TRUE)
		{
			GetScreenCapture(hwnd, data);

			// prevent duplicate frames
			if (memcmp(data, cap_image_last, screen_width * screen_height * 4) != 0)
			{
				int size = screen_width * screen_height * 4;

				// send to both connect and listen queues
				enqueue(&squeue, data, size);
				enqueue(&rqueue, data, size);
				memcpy(cap_image_last, data, size);
			}
			else
			{
				printf("Duplicate frame\r\n");
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

		init = true;

		break;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC		hdc;

		hdc = BeginPaint(hwnd, &ps);

		if (server == false)
		{
			draw_pixels(hdc, 0, 0,
				client_area.right, client_area.bottom,
				screen_width, screen_height,
				data);


			char state[MAX_PATH];

			sprintf(state, "Connect socket %d, connected=%d. client socket %d, connected=%d connect_enable=%d",
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


void handle_accepted(int &csock, char *buffer, int &size)
{
	if (csock == -1)
		return;

	while (1)
	{
		size = recv(csock, buffer, FRAME_SIZE, 0);
		if (size > 0)
		{
			// add data to circular queue
			//enqueue(&rqueue, rbuffer, size);
		}
		else if (size == 0)
		{
			break;
		}
		else if (size < 0)
		{
			int ret = WSAGetLastError();

			if (ret == WSAEWOULDBLOCK)
			{
				size = 0;
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
	StretchBlt(hdc, xoff, yoff, scalew, scaleh, hdcMem, 0, 0, width, height, SRCCOPY);
	SelectObject(hdcMem, hOldBitmap);
	DeleteDC(hdcMem);
	DeleteObject(hBitmap);
}





