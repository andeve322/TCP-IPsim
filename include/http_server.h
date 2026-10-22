#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "nodes.h"

typedef struct {
    int port;
    int server_fd;
    bool running;
    NetworkTopology *topology;
} HttpServer;

void http_server_init(HttpServer *server, int port, NetworkTopology *topology);
void http_server_start(HttpServer *server);
void http_server_stop(HttpServer *server);

#endif // HTTP_SERVER_H
