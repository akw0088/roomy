#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

typedef enum
{
	CONNECTED,
	DISCONNECTED
} client_state_t;

typedef int socklen_t;

#pragma pack(push, 1)
typedef struct
{
	int size;
	int width;
	int height;
	short planes;
	short bpp;
	int compression;
	int image_size;
	int xres;
	int yres;
	int clr_used;
	int clr_important;
} dib_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	char type[2];
	int file_size;
	int reserved;
	int offset;
} bmpheader_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
	bmpheader_t	header;
	dib_t		dib;
} bitmap_t;
#pragma pack(pop)


typedef struct
{
	unsigned int magic;
	unsigned int size;
	unsigned int xres;
	unsigned int yres;
} header_t;



typedef struct
{
	unsigned int left : 1,
		middle : 1,
		right : 1,
		x1 : 1,
		x2 : 1,
		wheel : 1,
		hwheel : 1,
		reserved0 : 1,
		reserved1 : 8,
		wheel_amount : 16;
} button_bits_t;

typedef union
{
	button_bits_t bits;
	int word;
} button_t;


typedef struct
{
	int magic;
	float x;
	float y;
	button_t button;
	uint64_t keyboard;
} input_t;


#endif
