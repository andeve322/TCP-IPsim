#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include "packet.h"

#define QUEUE_CAPACITY 32

typedef struct {
    Packet *items[QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    bool active;
    
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} PacketQueue;

void queue_init(PacketQueue *q);
void queue_destroy(PacketQueue *q);
void queue_push(PacketQueue *q, Packet *pkt);
Packet* queue_pop(PacketQueue *q);
void queue_shutdown(PacketQueue *q);

#endif // QUEUE_H
