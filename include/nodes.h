#ifndef NODES_H
#define NODES_H

#include <pthread.h>
#include "packet.h"
#include "queue.h"
#include "event.h"

typedef struct {
    // Forward Queues (Client -> Server)
    PacketQueue q_client_to_rep1;
    PacketQueue q_rep1_to_router;
    PacketQueue q_router_to_rep2;
    PacketQueue q_rep2_to_server;
    
    // Reverse Queues (Server -> Client)
    PacketQueue q_server_to_rep2;
    PacketQueue q_rep2_to_router;
    PacketQueue q_router_to_rep1;
    PacketQueue q_rep1_to_client;
    
    // Trigger and completion queues/synchronization
    PacketQueue q_client_trigger;
    
    // Event logging
    EventLog event_log;
    
    // Thread handles
    pthread_t th_client;
    pthread_t th_repeater1;
    pthread_t th_router;
    pthread_t th_repeater2;
    pthread_t th_server;
    
    bool running;
    pthread_mutex_t sim_lock;
    pthread_cond_t sim_done_cond;
    bool sim_in_progress;
} NetworkTopology;

void topology_init(NetworkTopology *net);
void topology_start(NetworkTopology *net);
void topology_shutdown(NetworkTopology *net);

// Run a single simulation synchronously and return JSON log
char* topology_run_simulation(NetworkTopology *net, AppProtocol proto, MailSubProtocol subproto, const char *method, const char *target, const char *headers, const char *body);

#endif // NODES_H
