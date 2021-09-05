#ifndef QUEUE_H
#define QUEUE_H
#define SIZE_QUEUE (512*1024*1024) // 512MB

#include <stdint.h>

typedef struct
{
	uint64_t size;
	uint64_t tail;
	uint64_t head;
	char buffer[SIZE_QUEUE];
} queue_t;

uint64_t enqueue(queue_t *queue, unsigned char *buffer, uint64_t size);
uint64_t dequeue(queue_t *queue, unsigned char *buffer, uint64_t size);
uint64_t enqueue_front(queue_t *queue, unsigned char *buffer, uint64_t size);
uint64_t dequeue_peek(queue_t *queue, unsigned char *buffer, uint64_t size);
#endif