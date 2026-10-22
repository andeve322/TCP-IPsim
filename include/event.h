#ifndef EVENT_H
#define EVENT_H

#include <pthread.h>
#include "packet.h"

#define MAX_EVENTS 256

typedef enum {
    NODE_CLIENT = 0,
    NODE_REPEATER1,
    NODE_ROUTER,
    NODE_REPEATER2,
    NODE_SERVER
} SimNode;

typedef enum {
    LAYER_L7_APP = 7,
    LAYER_L4_TRANS = 4,
    LAYER_L3_NET = 3,
    LAYER_L2_LINK = 2,
    LAYER_L1_PHYS = 1
} SimLayer;

typedef struct {
    int step_id;
    int simulation_id;
    PacketDirection direction;
    SimNode node;
    SimLayer layer;
    char action[128];
    char details[256];
    Packet packet_snapshot;
} SimulationEvent;

typedef struct {
    SimulationEvent events[MAX_EVENTS];
    int count;
    pthread_mutex_t lock;
} EventLog;

void event_log_init(EventLog *log);
void event_log_clear(EventLog *log);
void event_log_add(EventLog *log, int sim_id, PacketDirection dir, SimNode node, SimLayer layer, const char *action, const char *details, const Packet *pkt);
char* event_log_to_json(EventLog *log);

#endif // EVENT_H
