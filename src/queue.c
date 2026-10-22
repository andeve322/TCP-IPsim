#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void queue_init(PacketQueue *q) {
    if (!q) return;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->active = true;
    
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_destroy(PacketQueue *q) {
    if (!q) return;
    pthread_mutex_lock(&q->lock);
    q->active = false;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

void queue_shutdown(PacketQueue *q) {
    if (!q) return;
    pthread_mutex_lock(&q->lock);
    q->active = false;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
}

void queue_push(PacketQueue *q, Packet *pkt) {
    if (!q || !pkt) return;
    
    pthread_mutex_lock(&q->lock);
    
    // Wait while buffer is full and queue is active
    while (q->count == QUEUE_CAPACITY && q->active) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    
    if (!q->active) {
        pthread_mutex_unlock(&q->lock);
        return;
    }
    
    q->items[q->tail] = pkt;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    
    // Signal waiting consumers that a packet is ready
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

Packet* queue_pop(PacketQueue *q) {
    if (!q) return NULL;
    
    pthread_mutex_lock(&q->lock);
    
    // Wait while buffer is empty and queue is active
    while (q->count == 0 && q->active) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    
    if (q->count == 0 && !q->active) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }
    
    Packet *pkt = q->items[q->head];
    q->items[q->head] = NULL;
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;
    
    // Signal waiting producers that space is available
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    
    return pkt;
}
