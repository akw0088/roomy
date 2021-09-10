#ifndef ROOMY_H
#define ROOMY_H

#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock.h>
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "queue.h"



#define FRAME_SIZE (3840 * 2160 * 4)
#define MAX_WIDTH 3840
#define MAX_HEIGHT 2160

#define MAX(x,y) (x) > (y) ? (x) : (y)
#define MIN(x,y) (x) < (y) ? (x) : (y)


class Roomy
{
public:
	Roomy();
	void init(void *param1, void *param2, int xres, int yres);
	void step(int data_size);
	void mouse(float x, float y, button_t button);
	void handle_mouse(input_t *input);
	void handle_keyboard(input_t *input);
	void keycode(unsigned int keycode, int up);

	void handle_server(int &sock, client_state_t &state);
	void handle_client(int &sock, client_state_t &state);
	int encode(unsigned char* image, unsigned width, unsigned height, unsigned char* png, size_t &pngsize);
	int decode(unsigned char *image, unsigned int width, unsigned int height, unsigned char *png, size_t &pngsize);

	void capture();
	void destroy();

	unsigned char *get_data();

	int server;
	unsigned int remote_width;
	unsigned int remote_height;

private:
	int listen_socket(int &sock, unsigned short port);
	int set_sock_options(int sock);
	int connect_socket(char *ip_addr, unsigned short port, int &sock);
	void handle_listen(int &sock, int &csock, char *ipstr);
	int read_socket(int &csock, char *buffer, unsigned int &size);
	void read_config();


	int server_sock;	// listen socket
	int client_sock;	// client socket from listen
	int connect_sock;	// outgoing connect sock
	int listen_port;
	int connect_port;
	int listen_mode;
	int debug;
	char connect_ip[512];
	char listen_ip[512];
	char client_ip[512];

	client_state_t connect_state;
	client_state_t client_state;

	unsigned int screen_width;
	unsigned int screen_height;
	unsigned int screen_size;


	unsigned int remote_size;
	unsigned int packet_size;

	unsigned int tick;

	queue_t squeue;
	queue_t rqueue;

	unsigned char rbuffer[2 * FRAME_SIZE];
	unsigned char sbuffer[2 * FRAME_SIZE];

	unsigned char cap_image_last[FRAME_SIZE];
	unsigned char data[FRAME_SIZE];

#ifdef WIN32
	HWND hwnd;
#endif
};
#endif