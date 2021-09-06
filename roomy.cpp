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
	sprintf(connect_ip, "127.0.0.1");
	sprintf(listen_ip, "127.0.0.1");
	sprintf(client_ip, "");
}

void Roomy::init(void *param1, void *param2, int xres, int yres)
{
#ifdef WIN32
	HWND hwnd = (HWND)param1;
#endif
	read_config();

	screen_width = xres;
	screen_height = yres;
	screen_size = xres * yres * 4;


	printf("listen_mode is %d\r\n", listen_mode);
	if (listen_mode == 1)
	{
		listen_socket(server_sock, listen_port);
		set_sock_options(server_sock);
	}
}

void Roomy::step(int data_size)
{
	if (server && client_sock != -1)
	{

		// prevent duplicate frames
		if (memcmp(data, cap_image_last, data_size) != 0)
		{
			header_t header;

			static int seq = 0;
			header.magic = 0xDEAFB4B3;
			header.seq = seq++;
			header.size = data_size;
			// send to both connect and listen queues
			printf("Adding frame to queue\r\n");

			enqueue(&squeue, (unsigned char *)&header, sizeof(header_t));
			enqueue(&squeue, data, header.size);

			enqueue(&rqueue, (unsigned char *)&header, sizeof(header_t));
			enqueue(&rqueue, data, header.size);
			memcpy(cap_image_last, data, header.size);
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
				handle_listen(server_sock, client_sock, client_ip);
			}

			while (rqueue.size >= screen_size + sizeof(header_t))
			{
				header_t *header = (header_t *)rbuffer;

				dequeue_peek(&rqueue, rbuffer, sizeof(header_t));

				if (header->magic == 0xDEAFB4B3 && header->size == screen_size)
				{
					dequeue(&rqueue, rbuffer, screen_size + sizeof(header_t));
				}
				else
				{
					// drop 4 bytes and try again
					dequeue(&rqueue, rbuffer, 4);
					continue;
				}


				if (client_sock != -1)
				{
					int ret = send(client_sock, (char *)rbuffer, screen_size + sizeof(header_t), 0);
					if (ret == -1)
					{
						int err = WSAGetLastError();

						if (err != WSAEWOULDBLOCK)
						{
							printf("send returned -1 error %d\r\n", err);
							connect_state = DISCONNECTED;
							closesocket(client_sock);
							client_sock = -1;
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

			while (squeue.size >= screen_size + sizeof(header_t) && connect_sock != SOCKET_ERROR)
			{
				header_t *header = (header_t *)sbuffer;

				dequeue_peek(&squeue, sbuffer, sizeof(header_t));

				if (header->magic == 0xDEAFB4B3 && header->size == screen_size)
				{
					dequeue(&squeue, sbuffer, screen_size + sizeof(header_t));
				}
				else
				{
					// drop 4 bytes and try again
					dequeue(&squeue, sbuffer, 4);
					continue;
				}

				printf("Atempting to send %d bytes\r\n", screen_size + sizeof(header_t));
				int ret = send(connect_sock, (char *)sbuffer, screen_size + sizeof(header_t), 0);
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


		while (rqueue.size >= screen_size + sizeof(header_t))
		{
			header_t *header = (header_t *)rbuffer;

			dequeue_peek(&rqueue, rbuffer, sizeof(header_t));

			if (header->magic == 0xDEAFB4B3 && header->size == screen_size)
			{
				dequeue(&rqueue, rbuffer, screen_size + sizeof(header_t));
			}
			else
			{
				// drop 4 bytes and try again
				dequeue(&rqueue, rbuffer, 4);
				continue;
			}

			printf("Adding to view buffer\r\n");
			memcpy(data, rbuffer + sizeof(header_t), header->size);
//			InvalidateRect(hwnd, NULL, 0);
			// client draws from data buffer, so thats it
		}

		if (connect_sock != SOCKET_ERROR && connect_state == CONNECTED)
		{
			read_socket(connect_sock, (char *)sbuffer, rsize);
			enqueue(&squeue, sbuffer, rsize);
		}

		while (squeue.size >= screen_size + sizeof(header_t) && connect_sock != SOCKET_ERROR)
		{
			header_t *header = (header_t *)sbuffer;

			dequeue_peek(&squeue, sbuffer, sizeof(header_t));

			if (header->magic == 0xDEAFB4B3 && header->size == screen_size)
			{
				dequeue(&squeue, sbuffer, screen_size + sizeof(header_t));
			}
			else
			{
				// drop 4 bytes and try again
				dequeue(&squeue, sbuffer, 4);
				continue;
			}

			printf("Adding to view buffer\r\n");
			memcpy(data, sbuffer + sizeof(header_t), header->size);
//			InvalidateRect(hwnd, NULL, 0);
		}
	}

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


	int flag = 1;
	int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

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
}


void Roomy::read_socket(int &csock, char *buffer, unsigned int &size)
{
	if (csock == -1)
		return;

	size = 0;
	while (1)
	{
		int ret = 0;
		ret = recv(csock, &buffer[size], FRAME_SIZE + sizeof(header_t), 0);
		if (ret > 0)
		{
			printf("Read %d bytes from socket\r\n", ret);
			size += ret;
			if (size > FRAME_SIZE + sizeof(header_t))
			{
				printf("Read at least one frame\r\n");
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
#endif
}