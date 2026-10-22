#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "nodes.h"
#include "http_server.h"

static NetworkTopology g_topology;
static HttpServer g_server;

static void handle_sigint(int sig) {
    (void)sig;
    printf("\nShutting down TCP/IP Simulator...\n");
    http_server_stop(&g_server);
    topology_shutdown(&g_topology);
    exit(0);
}

int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) port = 8080;
    }
    
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    printf("Initializing network topology and node threads...\n");
    topology_init(&g_topology);
    topology_start(&g_topology);
    
    http_server_init(&g_server, port, &g_topology);
    http_server_start(&g_server);
    
    return 0;
}
