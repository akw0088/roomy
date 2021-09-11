#include "roomy.h"

char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

Roomy::Roomy()
{
	server_sock = -1;	// listen socket
	client_sock = -1;	// client socket from listen
	connect_sock = -1;	// outgoing connect sock
	connect_state = DISCONNECTED;
	client_state = DISCONNECTED;
	listen_port = 65535;
	connect_port = 65534;
	listen_mode = 0;
	debug = 0;
	update_rate = 500;
	enable_mouse = 1;
	enable_keyboard = 1;
	sprintf(connect_ip, "127.0.0.1");
	sprintf(listen_ip, "127.0.0.1");
	sprintf(client_ip, "");
}

void Roomy::init(void *param1, void *param2, int xres, int yres)
{
#ifdef WIN32
	hwnd = (HWND)param1;
#endif
	read_config();

	screen_width = xres;
	screen_height = yres;
	screen_size = xres * yres * 4;
	remote_size = FRAME_SIZE; // init to max size 4k
	remote_width = MAX_WIDTH;
	remote_height = MAX_HEIGHT;
	packet_size = MIN(FRAME_SIZE, remote_size) + sizeof(header_t);

	printf("listen_mode is %d\r\n", listen_mode);
	if (listen_mode == 1)
	{
		listen_socket(server_sock, listen_port);
		set_sock_options(server_sock);
	}
}

void Roomy::step(int data_size, int xres, int yres)
{
	tick++;

	// resolution can change
	screen_width = xres;
	screen_height = yres;

	if (server)
	{
		if (((listen_mode != 0 && client_sock != -1) ||
			(listen_mode == 0 && connect_sock != -1 && connect_state == CONNECTED))
			)
		{
			// prevent duplicate frames
			if (memcmp(data, cap_image_last, data_size) != 0)
			{
				header_t header;

				static int seq = 0;
				header.magic = 0xDEAFB4B3;
				header.xres = screen_width;
				header.yres = screen_height;
				header.size = data_size;


				// save last image raw form
				memcpy(cap_image_last, data, header.size);

				int ret = encode(cap_image_last, header.xres, header.yres, data, header.size);
				if (ret != 0)
				{
					printf("Encode failed\r\n");
				}
				else
				{
					// send to both connect and listen queues
					printf("Adding frame to queue %d size %d\r\n", seq++, header.size);

					packet_size = header.size + sizeof(header_t);
					enqueue(&squeue, (unsigned char *)&header, sizeof(header_t));
					enqueue(&squeue, data, header.size);
				}
			}
			else
			{
				printf("Duplicate frame\r\n");
			}
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


	InvalidateRect(hwnd, NULL, 0);
}



void Roomy::handle_mouse(input_t *input)
{
	RECT rect;
	INPUT in = { 0 };
	static button_t last_button = { 0 };
	static int last_rate = 500;
	rect.left = 0;
	rect.top = 0;
	rect.right = screen_width;
	rect.bottom = screen_height;


	if (last_rate != input->rate)
	{
		SetTimer(hwnd, 1337, input->rate, NULL);
	}

	// set mouse position
	in.type = INPUT_MOUSE;
	in.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
	in.mi.dx = (LONG)(input->x * 65535);
	in.mi.dy = (LONG)(input->y * 65535);

	if (input->button.word != last_button.word)
	{
		printf("Button = 0x%08X\r\n", input->button.word);
		printf("LastButton = 0x%08X\r\n", input->button.word);

		if (input->button.bits.wheel)
		{
			printf("Mouse wheel %d\r\n", (short)input->button.bits.wheel_amount);

			mouse_event(MOUSEEVENTF_WHEEL, 0, 0, (short)input->button.bits.wheel_amount, 0);
			last_button.word = 0;
			return;
		}
		else if (input->button.bits.hwheel)
		{
			printf("Mouse hwheel %d\r\n", (short)input->button.bits.wheel_amount);
			mouse_event(MOUSEEVENTF_HWHEEL, 0, 0, (short)input->button.bits.wheel_amount, 0);
			last_button.word = 0;
			return;
		}
		else if (input->button.bits.x1 == 1)
		{
//			in.mi.mouseData = XBUTTON1;
//			in.mi.dwFlags |= MOUSEEVENTF_XDOWN;
			printf("Mouse X1 Down\r\n");
			mouse_event(MOUSEEVENTF_XDOWN, in.mi.dx, in.mi.dy, XBUTTON1, 0);
			return;
		}
		else if (input->button.bits.x2 == 1)
		{
//			in.mi.mouseData = XBUTTON2;
//			in.mi.dwFlags |= MOUSEEVENTF_XDOWN;
			printf("Mouse X2 Down\r\n");
			mouse_event(MOUSEEVENTF_XDOWN, in.mi.dx, in.mi.dy, XBUTTON2, 0);
			return;
		}
		else if (input->button.bits.x1 == 2)
		{
//			in.mi.mouseData = XBUTTON1;
//			in.mi.dwFlags |= MOUSEEVENTF_XUP;
			printf("Mouse X1 Up\r\n");
			mouse_event(MOUSEEVENTF_XUP, in.mi.dx, in.mi.dy, XBUTTON1, 0);
			return;
		}
		else if (input->button.bits.x2 == 2)
		{
//			in.mi.mouseData = XBUTTON2;
//			in.mi.dwFlags |= MOUSEEVENTF_XUP;
			printf("Mouse X2 Up\r\n");
			mouse_event(MOUSEEVENTF_XUP, in.mi.dx, in.mi.dy, XBUTTON2, 0);
			return;
		}


		// check for clicks
		if (input->button.bits.left == 1)
		{
			in.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
			printf("Mouse Left down\r\n");
		}

		if (input->button.bits.middle == 1)
		{
			in.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			printf("Mouse Mid down\r\n");
		}

		if (input->button.bits.right == 1)
		{
			in.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			printf("Mouse Right down\r\n");
		}


		// check for releases
		if (input->button.bits.left == 2)
		{
			in.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
			printf("Mouse Left up\r\n");
		}

		if (input->button.bits.middle == 2)
		{
			in.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
			printf("Mouse Mid up\r\n");
		}

		if (input->button.bits.right == 2)
		{
			in.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
			printf("Mouse right up\r\n");
		}
	}

	SendInput(1, &in, sizeof(INPUT));
}


void Roomy::handle_server(int &sock, client_state_t &state)
{
	unsigned int rsize = 0;

	if (sock != SOCKET_ERROR && state == CONNECTED)
	{
		int ret = 0;

		if (ret = read_socket(sock, (char *)rbuffer, rsize) != 0)
		{
			state = DISCONNECTED;
		}
	}

	while (rqueue.size >= sizeof(input_t))
	{
		dequeue_peek(&rqueue, rbuffer, sizeof(input_t));

		input_t *input = (input_t *)rbuffer;

		if (input->magic == 0xDEADBEEF || input->magic == 0x531F1355)
		{
			dequeue(&rqueue, rbuffer, sizeof(input_t));

			if (input->magic == 0xDEADBEEF)
				handle_mouse(input);
			else
				handle_keyboard(input);
		}
		else
		{
			// drop bytes and try again
			dequeue(&rqueue, rbuffer, 1);
			continue;
		}
	}

	while (squeue.size >= packet_size && sock != SOCKET_ERROR)
	{
		header_t *header = (header_t *)sbuffer;

		dequeue_peek(&squeue, sbuffer, sizeof(header_t));

		if (header->magic == 0xDEAFB4B3)
		{
			dequeue(&squeue, sbuffer, header->size + sizeof(header_t));
		}
		else
		{
			// drop 4 bytes and try again
			dequeue(&squeue, sbuffer, 1);
			continue;
		}

		//		printf("Atempting to send %d bytes\r\n", header->size + sizeof(header_t));
		int ret = send(sock, (char *)sbuffer, header->size + sizeof(header_t), 0);
		if (ret == -1)
		{
			int err = WSAGetLastError();

			if (err != WSAEWOULDBLOCK)
			{
				printf("send returned -1 error %d\r\n", err);
				state = DISCONNECTED;
				closesocket(sock);
				sock = -1;
			}
			break;
		}
		else if (ret > 0 && (unsigned int)ret < header->size)
		{
			// partial send occurred (full buffer?)
			enqueue_front(&squeue, &sbuffer[ret], header->size - ret);
		}
	}
}


void Roomy::handle_client(int &sock, client_state_t &state)
{
	unsigned int rsize = 0;

	if (sock != SOCKET_ERROR && state == CONNECTED)
	{
		int ret = 0;

		if (ret = read_socket(sock, (char *)rbuffer, rsize) != 0)
		{
			state = DISCONNECTED;
		}
	}

	while (rqueue.size >= packet_size)
	{
		header_t *header = (header_t *)rbuffer;

		dequeue_peek(&rqueue, rbuffer, sizeof(header_t));
		if (header->magic == 0xDEAFB4B3)
		{
			remote_size = header->size;
			remote_width = header->xres;
			remote_height = header->yres;
			packet_size = MIN(FRAME_SIZE, remote_size) + sizeof(header_t);
			dequeue(&rqueue, rbuffer, header->size + sizeof(header_t));


			printf("Adding to view buffer\r\n");

			unsigned char *png = rbuffer + sizeof(header_t);
			int ret = decode(data, remote_width, remote_height, png, remote_size);
			if (ret != 0)
			{
				printf("Decode failed\r\n");
			}

		}
		else
		{
			// drop 4 bytes and try again
			dequeue(&rqueue, rbuffer, 4);
			continue;
		}

		//			InvalidateRect(hwnd, NULL, 0);
		// client draws from data buffer, so thats it
	}

	while (squeue.size >= sizeof(input_t) && sock != SOCKET_ERROR)
	{
		dequeue_peek(&squeue, sbuffer, sizeof(input_t));

		input_t *input = (input_t *)sbuffer;

		if (input->magic == 0xDEADBEEF || input->magic == 0x531F1355)
		{
			dequeue(&squeue, sbuffer, sizeof(input_t));

			int ret = send(sock, (char *)sbuffer, sizeof(input_t), 0);
			if (ret < 0)
			{
				int err = WSAGetLastError();

				if (err != WSAEWOULDBLOCK)
				{
					printf("send returned -1 error %d\r\n", err);
					state = DISCONNECTED;
					closesocket(sock);
					sock = -1;
				}
				break;
			}
			else
			{
				printf("Sent %d bytes of mouse input\r\n", ret);
				break;
			}
		}
		else
		{
			// drop 4 bytes and try again
			dequeue(&squeue, sbuffer, 1);
			continue;
		}

	}

}

void Roomy::capture()
{
	unsigned int rsize = 0;
	
	if (server)
	{
		if (listen_mode)
		{
			if (client_sock == SOCKET_ERROR)
			{
				client_state = DISCONNECTED;
				handle_listen(server_sock, client_sock, client_ip);
			}
			else
			{
				client_state = CONNECTED;
				handle_server(client_sock, client_state);
			}
		}
		else
		{
			handle_server(connect_sock, connect_state);
		}
	}
	else
	{
		if (listen_mode)
		{
			if (client_sock == SOCKET_ERROR)
			{
				client_state = DISCONNECTED;
				handle_listen(server_sock, client_sock, client_ip);
			}
			else
			{
				client_state = CONNECTED;
				handle_client(client_sock, client_state);
			}
		}
		else
		{
			handle_client(connect_sock, connect_state);
		}
	}

	Sleep(0); // save CPU
}

unsigned char *Roomy::get_data()
{
	return &data[0];
}

void Roomy::destroy()
{

}

int Roomy::listen_socket(int &sock, unsigned short port)
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

int Roomy::set_sock_options(int sock)
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


//	int flag = 1;
//	int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

	return 0;
}


int Roomy::connect_socket(char *ip_addr, unsigned short port, int &sock)
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
			int err = WSAGetLastError();

			switch (err)
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
				printf("Fatal Error: %d Line %d\n", err, __LINE__);
				sock = -1;
				closesocket(sock);
				connect_state = DISCONNECTED;
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
		timeout.tv_usec = 5;

		int ret = select(sock + 1, &read_set, &write_set, NULL, &timeout);
		if (ret < 0)
		{
			printf("select() failed\r\n");
			return -1;
		}
		else if (ret == 0)
		{
			static int count = 0;
			printf("select() timed out %d of 10\r\n", count);
			count++;

			if (count >= 10)
			{
				count = 0;
				printf("Resetting socket\r\n");
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
				printf("SO_ERROR was not zero\r\n");
				closesocket(sock);
				sock = -1;
			}

		}
	}

	return -1;
}

void Roomy::handle_listen(int &sock, int &csock, char *ipstr)
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
	else
	{
		int err = WSAGetLastError();

		switch (err)
		{
		case WSAETIMEDOUT:
			printf("Fatal Error: timed out.\n");
			break;
		case WSAECONNREFUSED:
			printf("Fatal Error: refused connection.\n(Server program is not running)\n");
			break;
		case WSAEHOSTUNREACH:
			printf("Fatal Error: router sent ICMP packet (destination unreachable)\n");
			break;
		case WSAEWOULDBLOCK:
			return;
		default:
			printf("Fatal Error: %d Line %d\n", err, __LINE__);
			sock = -1;
			closesocket(sock);
			break;
		}

		return;

	}
}


int Roomy::read_socket(int &csock, char *buffer, unsigned int &size)
{
	if (csock == -1)
		return -1;

	size = 0;
	while (1)
	{
		int ret = 0;
		ret = recv(csock, &buffer[size], packet_size, 0);
		if (ret > 0)
		{
//			printf("Read %d bytes from socket\r\n", ret);
			enqueue(&rqueue, (unsigned char *)&buffer[size], ret);
			size += ret;
			if (size > packet_size)
			{
//				printf("Read at least one frame\r\n");
				break;
			}
		}
		else if (ret == 0)
		{
			break;
		}
		else if (ret < 0)
		{
			int err = WSAGetLastError();

			switch (err)
			{
			case WSAEWOULDBLOCK:
				return 0;
			case WSAETIMEDOUT:
				csock = SOCKET_ERROR;
				break;
			case WSAECONNREFUSED:
				csock = SOCKET_ERROR;
				break;
			case WSAEHOSTUNREACH:
				csock = SOCKET_ERROR;
				break;
			default:
				printf("Fatal Error: %d Line %d\n", err, __LINE__);
				csock = SOCKET_ERROR;
				closesocket(csock);
				break;
			}
			return -1;
		}
	}

	return 0;
}



void Roomy::read_config()
{
	// This is very OS dependent, isolating to here for now
#ifdef WIN32
	char path[MAX_PATH] = { 0 };
	GetCurrentDirectory(MAX_PATH, path);
	lstrcat(path, TEXT("\\roomy.ini"));

	server = GetPrivateProfileInt(TEXT("roomy"), TEXT("server"), 1, path);
	debug = GetPrivateProfileInt(TEXT("roomy"), TEXT("debug"), 0, path);
	listen_port = GetPrivateProfileInt(TEXT("roomy"), TEXT("listen"), 65535, path);
	connect_port = GetPrivateProfileInt(TEXT("roomy"), TEXT("connect"), 65535, path);
	GetPrivateProfileString(TEXT("roomy"), TEXT("ip"), "127.0.0.1", connect_ip, MAX_PATH, path);
	listen_mode = GetPrivateProfileInt(TEXT("roomy"), TEXT("listen_mode"), 1, path);
	update_rate = GetPrivateProfileInt(TEXT("roomy"), TEXT("rate"), 500, path);
	enable_mouse = GetPrivateProfileInt(TEXT("roomy"), TEXT("enable_mouse"), 1, path);
	enable_keyboard = GetPrivateProfileInt(TEXT("roomy"), TEXT("enable_keyboard"), 1, path);
#endif
}


int Roomy::encode(unsigned char *image, unsigned width, unsigned height, unsigned char *compressed, size_t &compressed_size)
{
//	return lodepng_encode32(compressed, &compressed_size, image, width, height);
	memcpy(compressed, image, width * height * 4);
	return 0;
}


int Roomy::decode(unsigned char *image, unsigned int width, unsigned int height, unsigned char *compressed, size_t &compressed_size)
{
//	return lodepng_decode32(image, &width, &height, compressed, compressed_size);
	memcpy(image, compressed, compressed_size);
	return 0;
}


void Roomy::mouse(float x, float y, button_t button)
{
	static button_t last_button = { 0 };
	static int last_tick = 0;

	if (server)
		return;


	// prevent mouse move flooding, but if a button change occurred, go for it
	if (tick == last_tick && button.word == last_button.word)
		return;


	if (enable_mouse)
	{
		input_t input = { 0 };

		printf("x %f y %f\r\n", x, y);
		input.magic = 0xDEADBEEF;
		input.x = x;
		input.y = y;
		input.button.word = button.word;
		input.rate = update_rate;
		enqueue(&squeue, (unsigned char *)&input, sizeof(input_t));
	}
}

void Roomy::keycode(unsigned int kc, int up)
{
	if (enable_keyboard)
	{
		input_t input = { 0 };

		input.magic = 0x531F1355;
		input.keycode = kc;
		input.keyup = up;
		input.rate = update_rate;
		enqueue(&squeue, (unsigned char *)&input, sizeof(input_t));
	}
}

void Roomy::handle_keyboard(input_t *input)
{
	INPUT in = { 0 };
	static int last_rate = 500;

	printf("keycode %d\r\n", input->keycode);
	printf("keyup %d\r\n", input->keyup);

	in.type = INPUT_KEYBOARD;
	in.ki.wVk = input->keycode;
	if (input->keyup)
	{
		in.ki.dwFlags = KEYEVENTF_KEYUP;
	}

	if (last_rate != input->rate)
	{
		SetTimer(hwnd, 1337, input->rate, NULL);
	}


	SendInput(1, &in, sizeof(INPUT));
}
