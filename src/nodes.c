#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nodes.h"
#include "protocols.h"

#define CLIENT_IP       "192.168.1.100"
#define CLIENT_MAC      "AA:BB:CC:DD:EE:01"
#define ROUTER_MAC_ETH0 "AA:BB:CC:DD:EE:02" // Client side
#define ROUTER_MAC_ETH1 "11:22:33:44:55:01" // Server side
#define SERVER_IP       "10.0.0.50"
#define SERVER_MAC      "11:22:33:44:55:02"

// Helper to determine destination port
static uint16_t get_default_port(AppProtocol proto, MailSubProtocol subproto) {
    switch (proto) {
        case PROTO_HTTP: return 80;
        case PROTO_DNS:  return 53;
        case PROTO_CDN:  return 443;
        case PROTO_SMTP:
            if (subproto == MAIL_SUB_POP3) return 110;
            if (subproto == MAIL_SUB_IMAP) return 143;
            if (subproto == MAIL_SUB_WEBMAIL) return 80;
            return 25;
        case PROTO_P2P:  return 6881;
        default:         return 80;
    }
}

static const char* get_port_service_name(uint16_t port) {
    switch (port) {
        case 80:   return "HTTP (Web)";
        case 53:   return "DNS (Nameserver)";
        case 443:  return "HTTPS/CDN (Secure Web)";
        case 25:   return "SMTP (Mail Transfer)";
        case 110:  return "POP3 (Mail Retrieval)";
        case 143:  return "IMAP (Mailbox Sync)";
        case 6881: return "BitTorrent P2P";
        default:   return "Dynamic/Ephemeral";
    }
}

// -------------------------------------------------------------
// Client Node Thread
// -------------------------------------------------------------
static void* client_worker(void *arg) {
    NetworkTopology *net = (NetworkTopology*)arg;
    
    while (net->running) {
        // Wait for a simulation trigger packet
        Packet *trigger = queue_pop(&net->q_client_trigger);
        if (!trigger) break; // Shutdown
        
        int sim_id = trigger->simulation_id;
        
        // 1. Layer 7: Application
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_CLIENT, LAYER_L7_APP,
                      "L7 Composition", "Composed application request with method and headers", trigger);
        usleep(5000);
        
        // 2. Layer 4: Transport (TCP/UDP encapsulation)
        L7_Payload l7 = trigger->frame.packet.segment.payload;
        uint16_t dport = get_default_port(l7.protocol, l7.mail_subprotocol);
        trigger->frame.packet.segment.src_port = 52144;
        trigger->frame.packet.segment.dest_port = dport;
        trigger->frame.packet.segment.seq_num = 1000;
        trigger->frame.packet.segment.ack_num = 0;
        trigger->frame.packet.segment.flags = 0x02; // SYN or data
        trigger->frame.packet.segment.transport_proto = (l7.protocol == PROTO_DNS) ? 17 : 6;
        
        char desc[256];
        if (l7.protocol == PROTO_DNS) {
            snprintf(desc, sizeof(desc),
                     "L4 UDP Datagram: Ephemeral Src Port 52144 -> Well-Known Dest Port 53 (DNS). Connectionless mode for rapid lookup.");
        } else {
            snprintf(desc, sizeof(desc),
                     "L4 TCP Segment: Ephemeral Src Port 52144 -> Well-Known Dest Port %d (%s). Flags: SYN (Seq=1000) for reliable stream.",
                     dport, get_port_service_name(dport));
        }
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_CLIENT, LAYER_L4_TRANS,
                      (l7.protocol == PROTO_DNS) ? "L4 UDP Encap" : "L4 TCP Encap", desc, trigger);
        usleep(5000);
        
        // 3. Layer 3: Network (IP encapsulation)
        strncpy(trigger->frame.packet.src_ip, CLIENT_IP, IP_ADDR_LEN - 1);
        strncpy(trigger->frame.packet.dest_ip, SERVER_IP, IP_ADDR_LEN - 1);
        trigger->frame.packet.ttl = 64;
        trigger->frame.packet.protocol = trigger->frame.packet.segment.transport_proto;
        
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_CLIENT, LAYER_L3_NET,
                      "L3 Encapsulation", "Encapsulated into IPv4 packet (Src: " CLIENT_IP ", Dest: " SERVER_IP ", TTL: 64)", trigger);
        usleep(5000);
        
        // 4. Layer 2: Data Link (Ethernet frame with Gateway MAC)
        strncpy(trigger->frame.src_mac, CLIENT_MAC, MAC_ADDR_LEN - 1);
        strncpy(trigger->frame.dest_mac, ROUTER_MAC_ETH0, MAC_ADDR_LEN - 1);
        trigger->frame.ethertype = 0x0800; // IPv4
        
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_CLIENT, LAYER_L2_LINK,
                      "L2 Encapsulation", "Encapsulated into Ethernet frame (Next-Hop MAC: " ROUTER_MAC_ETH0 ")", trigger);
        usleep(5000);
        
        // 5. Layer 1: Physical transmission
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_CLIENT, LAYER_L1_PHYS,
                      "L1 Transmission", "Pushed raw frame onto link toward Repeater 1", trigger);
        
        queue_push(&net->q_client_to_rep1, trigger);
        
        // --- Now wait for response packet to travel back ---
        Packet *resp = queue_pop(&net->q_rep1_to_client);
        if (!resp) break;
        
        // Client receives response: Ascend stack
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_CLIENT, LAYER_L1_PHYS,
                      "L1 Reception", "Bits received on physical network interface", resp);
        usleep(5000);
        
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_CLIENT, LAYER_L2_LINK,
                      "L2 Decapsulation", "Ethernet frame MAC verified (" CLIENT_MAC "), frame stripped", resp);
        usleep(5000);
        
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_CLIENT, LAYER_L3_NET,
                      "L3 Decapsulation", "IPv4 packet destination verified, IP header stripped", resp);
        usleep(5000);
        
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_CLIENT, LAYER_L4_TRANS,
                      "L4 Decapsulation", "Transport segment matched to active connection socket (52144)", resp);
        usleep(5000);
        
        char final_msg[256];
        snprintf(final_msg, sizeof(final_msg), "Application received response (Code %d: %s)",
                 resp->frame.packet.segment.payload.status_code,
                 resp->frame.packet.segment.payload.status_message);
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_CLIENT, LAYER_L7_APP,
                      "L7 Delivery", final_msg, resp);
        
        free(resp);
        
        // Signal topology_run_simulation that this run has finished
        pthread_mutex_lock(&net->sim_lock);
        net->sim_in_progress = false;
        pthread_cond_signal(&net->sim_done_cond);
        pthread_mutex_unlock(&net->sim_lock);
    }
    
    return NULL;
}

// -------------------------------------------------------------
// Repeater 1 Node Thread (L1 only)
// -------------------------------------------------------------
static void* repeater1_worker(void *arg) {
    NetworkTopology *net = (NetworkTopology*)arg;
    
    while (net->running) {
        // Forward direction: Client -> Repeater1 -> Router
        Packet *pkt = queue_pop(&net->q_client_to_rep1);
        if (!pkt) break;
        
        event_log_add(&net->event_log, pkt->simulation_id, DIR_FORWARD, NODE_REPEATER1, LAYER_L1_PHYS,
                      "Signal Repeat", "Repeater 1 regenerated physical signal and forwarded to Router", pkt);
        usleep(5000);
        
        queue_push(&net->q_rep1_to_router, pkt);
        
        // Reverse direction: Router -> Repeater1 -> Client
        Packet *resp = queue_pop(&net->q_router_to_rep1);
        if (!resp) break;
        
        event_log_add(&net->event_log, resp->simulation_id, DIR_REVERSE, NODE_REPEATER1, LAYER_L1_PHYS,
                      "Signal Repeat", "Repeater 1 regenerated response signal and forwarded to Client", resp);
        usleep(5000);
        
        queue_push(&net->q_rep1_to_client, resp);
    }
    
    return NULL;
}

// -------------------------------------------------------------
// Router Node Thread (L1 -> L2 -> L3 Routing)
// -------------------------------------------------------------
static void* router_worker(void *arg) {
    NetworkTopology *net = (NetworkTopology*)arg;
    
    while (net->running) {
        // Forward: from Repeater1 to Repeater2
        Packet *pkt = queue_pop(&net->q_rep1_to_router);
        if (!pkt) break;
        
        event_log_add(&net->event_log, pkt->simulation_id, DIR_FORWARD, NODE_ROUTER, LAYER_L1_PHYS,
                      "L1 Inbound", "Frame bits received on interface eth0", pkt);
        usleep(5000);
        
        event_log_add(&net->event_log, pkt->simulation_id, DIR_FORWARD, NODE_ROUTER, LAYER_L2_LINK,
                      "L2 Check", "Verified Destination MAC matches eth0 (" ROUTER_MAC_ETH0 ")", pkt);
        usleep(5000);
        
        // Decrement TTL
        if (pkt->frame.packet.ttl > 0) {
            pkt->frame.packet.ttl--;
        }
        
        char routing_desc[128];
        snprintf(routing_desc, sizeof(routing_desc),
                 "Routing table lookup: Dest IP " SERVER_IP " matches eth1; TTL decremented to %d",
                 pkt->frame.packet.ttl);
        event_log_add(&net->event_log, pkt->simulation_id, DIR_FORWARD, NODE_ROUTER, LAYER_L3_NET,
                      "L3 Routing", routing_desc, pkt);
        usleep(5000);
        
        // Rewrite MAC addresses for next hop
        strncpy(pkt->frame.src_mac, ROUTER_MAC_ETH1, MAC_ADDR_LEN - 1);
        strncpy(pkt->frame.dest_mac, SERVER_MAC, MAC_ADDR_LEN - 1);
        
        event_log_add(&net->event_log, pkt->simulation_id, DIR_FORWARD, NODE_ROUTER, LAYER_L2_LINK,
                      "L2 Rewrite", "Updated Next-Hop MAC to Server (" SERVER_MAC ")", pkt);
        usleep(5000);
        
        event_log_add(&net->event_log, pkt->simulation_id, DIR_FORWARD, NODE_ROUTER, LAYER_L1_PHYS,
                      "L1 Transmission", "Pushed frame bits onto wire (eth1) toward Repeater 2", pkt);
        usleep(5000);
        
        queue_push(&net->q_router_to_rep2, pkt);
        
        // Reverse: from Repeater2 to Repeater1
        Packet *resp = queue_pop(&net->q_rep2_to_router);
        if (!resp) break;
        
        event_log_add(&net->event_log, resp->simulation_id, DIR_REVERSE, NODE_ROUTER, LAYER_L1_PHYS,
                       "L1 Inbound", "Response frame bits received on eth1", resp);
        usleep(5000);
        
        event_log_add(&net->event_log, resp->simulation_id, DIR_REVERSE, NODE_ROUTER, LAYER_L2_LINK,
                      "L2 Check", "Verified Destination MAC matches eth1 (" ROUTER_MAC_ETH1 ")", resp);
        usleep(5000);
        
        if (resp->frame.packet.ttl > 0) {
            resp->frame.packet.ttl--;
        }
        
        snprintf(routing_desc, sizeof(routing_desc),
                 "Routing response: Dest IP " CLIENT_IP " matches eth0; TTL decremented to %d",
                 resp->frame.packet.ttl);
        event_log_add(&net->event_log, resp->simulation_id, DIR_REVERSE, NODE_ROUTER, LAYER_L3_NET,
                      "L3 Routing", routing_desc, resp);
        usleep(5000);
        
        strncpy(resp->frame.src_mac, ROUTER_MAC_ETH0, MAC_ADDR_LEN - 1);
        strncpy(resp->frame.dest_mac, CLIENT_MAC, MAC_ADDR_LEN - 1);
        
        event_log_add(&net->event_log, resp->simulation_id, DIR_REVERSE, NODE_ROUTER, LAYER_L2_LINK,
                      "L2 Rewrite", "Updated Next-Hop MAC to Client (" CLIENT_MAC ")", resp);
        usleep(5000);
        
        event_log_add(&net->event_log, resp->simulation_id, DIR_REVERSE, NODE_ROUTER, LAYER_L1_PHYS,
                      "L1 Transmission", "Pushed response bits onto wire (eth0) toward Repeater 1", resp);
        usleep(5000);
        
        queue_push(&net->q_router_to_rep1, resp);
    }
    
    return NULL;
}

// -------------------------------------------------------------
// Repeater 2 Node Thread (L1 only)
// -------------------------------------------------------------
static void* repeater2_worker(void *arg) {
    NetworkTopology *net = (NetworkTopology*)arg;
    
    while (net->running) {
        // Forward: Router -> Repeater2 -> Server
        Packet *pkt = queue_pop(&net->q_router_to_rep2);
        if (!pkt) break;
        
        event_log_add(&net->event_log, pkt->simulation_id, DIR_FORWARD, NODE_REPEATER2, LAYER_L1_PHYS,
                      "Signal Repeat", "Repeater 2 regenerated physical signal and forwarded to Server", pkt);
        usleep(5000);
        
        queue_push(&net->q_rep2_to_server, pkt);
        
        // Reverse: Server -> Repeater2 -> Router
        Packet *resp = queue_pop(&net->q_server_to_rep2);
        if (!resp) break;
        
        event_log_add(&net->event_log, resp->simulation_id, DIR_REVERSE, NODE_REPEATER2, LAYER_L1_PHYS,
                      "Signal Repeat", "Repeater 2 regenerated response signal and forwarded to Router", resp);
        usleep(5000);
        
        queue_push(&net->q_rep2_to_router, resp);
    }
    
    return NULL;
}

// -------------------------------------------------------------
// Server Node Thread (Ascends L2 -> L7, Processes Request, Descends L7 -> L1)
// -------------------------------------------------------------
static void* server_worker(void *arg) {
    NetworkTopology *net = (NetworkTopology*)arg;
    
    while (net->running) {
        Packet *pkt = queue_pop(&net->q_rep2_to_server);
        if (!pkt) break;
        
        int sim_id = pkt->simulation_id;
        
        // 1. L1 Physical
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_SERVER, LAYER_L1_PHYS,
                      "L1 Reception", "Bits received on server network interface", pkt);
        usleep(5000);
        
        // 2. L2 Data Link
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_SERVER, LAYER_L2_LINK,
                      "L2 Verification", "Ethernet MAC matched Server MAC (" SERVER_MAC "), stripped frame header", pkt);
        usleep(5000);
        
        // 3. L3 Network
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_SERVER, LAYER_L3_NET,
                      "L3 Verification", "IP matched Server IP (" SERVER_IP "), protocol parsed", pkt);
        usleep(5000);
        
        // 4. L4 Transport
        char l4_desc[256];
        if (pkt->frame.packet.segment.transport_proto == 17) {
            snprintf(l4_desc, sizeof(l4_desc),
                     "UDP Datagram Demux: Dest Port 53 matched listening DNS resolver daemon (0.0.0.0:53). Checksum validated.");
        } else {
            snprintf(l4_desc, sizeof(l4_desc),
                     "TCP Segment Demux: Dest Port %d matched listening %s daemon socket (0.0.0.0:%d). Seq=1000 verified against socket buffer.",
                     pkt->frame.packet.segment.dest_port, get_port_service_name(pkt->frame.packet.segment.dest_port), pkt->frame.packet.segment.dest_port);
        }
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_SERVER, LAYER_L4_TRANS,
                      (pkt->frame.packet.segment.transport_proto == 17) ? "L4 UDP Demux" : "L4 TCP Demux", l4_desc, pkt);
        usleep(5000);
        
        // 5. L7 Application Processing
        event_log_add(&net->event_log, sim_id, DIR_FORWARD, NODE_SERVER, LAYER_L7_APP,
                      "L7 Processing", "Evaluating application payload and generating coherent response", pkt);
        usleep(5000);
        
        // Business logic execution
        protocol_process_server_request(&pkt->frame.packet.segment.payload);
        
        // Build reverse packet
        pkt->direction = DIR_REVERSE;
        
        // Swap L4 ports
        uint16_t orig_src = pkt->frame.packet.segment.src_port;
        uint16_t orig_dst = pkt->frame.packet.segment.dest_port;
        pkt->frame.packet.segment.src_port = orig_dst;
        pkt->frame.packet.segment.dest_port = orig_src;
        pkt->frame.packet.segment.seq_num = 5000;
        pkt->frame.packet.segment.ack_num = 1001;
        pkt->frame.packet.segment.flags = 0x10 | 0x08; // ACK + PSH
        
        // Swap L3 IPs & reset TTL
        strncpy(pkt->frame.packet.src_ip, SERVER_IP, IP_ADDR_LEN - 1);
        strncpy(pkt->frame.packet.dest_ip, CLIENT_IP, IP_ADDR_LEN - 1);
        pkt->frame.packet.ttl = 64;
        
        // Set L2 Next Hop MAC (Router eth1)
        strncpy(pkt->frame.src_mac, SERVER_MAC, MAC_ADDR_LEN - 1);
        strncpy(pkt->frame.dest_mac, ROUTER_MAC_ETH1, MAC_ADDR_LEN - 1);
        
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_SERVER, LAYER_L7_APP,
                      "L7 Response Ready", "Application generated status and response payload", pkt);
        usleep(5000);
        
        char l4_resp_desc[256];
        if (pkt->frame.packet.segment.transport_proto == 17) {
            snprintf(l4_resp_desc, sizeof(l4_resp_desc),
                     "UDP Response Datagram: Swapped ports (Src: %d [DNS Server] -> Dest: %d [Client socket]). Single datagram reply.",
                     orig_dst, orig_src);
        } else {
            snprintf(l4_resp_desc, sizeof(l4_resp_desc),
                     "TCP Response Segment: Swapped ports (Src: %d [%s] -> Dest: %d [Client]). Set ACK+PSH, Seq=5000, Ack=1001.",
                     orig_dst, get_port_service_name(orig_dst), orig_src);
        }
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_SERVER, LAYER_L4_TRANS,
                      (pkt->frame.packet.segment.transport_proto == 17) ? "L4 UDP Response Encap" : "L4 TCP Response Encap", l4_resp_desc, pkt);
        usleep(5000);
        
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_SERVER, LAYER_L3_NET,
                      "L3 Response Encap", "Encapsulated into IPv4 packet (Dest: " CLIENT_IP ", TTL: 64)", pkt);
        usleep(5000);
        
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_SERVER, LAYER_L2_LINK,
                      "L2 Response Encap", "Encapsulated into Ethernet frame (Next-Hop: " ROUTER_MAC_ETH1 ")", pkt);
        usleep(5000);
        
        event_log_add(&net->event_log, sim_id, DIR_REVERSE, NODE_SERVER, LAYER_L1_PHYS,
                      "L1 Transmission", "Pushed response frame onto wire toward Repeater 2", pkt);
        
        queue_push(&net->q_server_to_rep2, pkt);
    }
    
    return NULL;
}

// -------------------------------------------------------------
// Topology Management
// -------------------------------------------------------------
void topology_init(NetworkTopology *net) {
    if (!net) return;
    memset(net, 0, sizeof(NetworkTopology));
    
    queue_init(&net->q_client_to_rep1);
    queue_init(&net->q_rep1_to_router);
    queue_init(&net->q_router_to_rep2);
    queue_init(&net->q_rep2_to_server);
    
    queue_init(&net->q_server_to_rep2);
    queue_init(&net->q_rep2_to_router);
    queue_init(&net->q_router_to_rep1);
    queue_init(&net->q_rep1_to_client);
    
    queue_init(&net->q_client_trigger);
    
    event_log_init(&net->event_log);
    
    pthread_mutex_init(&net->sim_lock, NULL);
    pthread_cond_init(&net->sim_done_cond, NULL);
    
    net->running = true;
    net->sim_in_progress = false;
}

void topology_start(NetworkTopology *net) {
    if (!net) return;
    
    pthread_create(&net->th_client, NULL, client_worker, net);
    pthread_create(&net->th_repeater1, NULL, repeater1_worker, net);
    pthread_create(&net->th_router, NULL, router_worker, net);
    pthread_create(&net->th_repeater2, NULL, repeater2_worker, net);
    pthread_create(&net->th_server, NULL, server_worker, net);
}

void topology_shutdown(NetworkTopology *net) {
    if (!net || !net->running) return;
    
    net->running = false;
    
    queue_shutdown(&net->q_client_to_rep1);
    queue_shutdown(&net->q_rep1_to_router);
    queue_shutdown(&net->q_router_to_rep2);
    queue_shutdown(&net->q_rep2_to_server);
    
    queue_shutdown(&net->q_server_to_rep2);
    queue_shutdown(&net->q_rep2_to_router);
    queue_shutdown(&net->q_router_to_rep1);
    queue_shutdown(&net->q_rep1_to_client);
    
    queue_shutdown(&net->q_client_trigger);
    
    pthread_join(net->th_client, NULL);
    pthread_join(net->th_repeater1, NULL);
    pthread_join(net->th_router, NULL);
    pthread_join(net->th_repeater2, NULL);
    pthread_join(net->th_server, NULL);
    
    queue_destroy(&net->q_client_to_rep1);
    queue_destroy(&net->q_rep1_to_router);
    queue_destroy(&net->q_router_to_rep2);
    queue_destroy(&net->q_rep2_to_server);
    
    queue_destroy(&net->q_server_to_rep2);
    queue_destroy(&net->q_rep2_to_router);
    queue_destroy(&net->q_router_to_rep1);
    queue_destroy(&net->q_rep1_to_client);
    
    queue_destroy(&net->q_client_trigger);
    
    pthread_mutex_destroy(&net->sim_lock);
    pthread_cond_destroy(&net->sim_done_cond);
}

char* topology_run_simulation(NetworkTopology *net, AppProtocol proto, MailSubProtocol subproto, const char *method, const char *target, const char *headers, const char *body) {
    if (!net) return strdup("[]");
    
    pthread_mutex_lock(&net->sim_lock);
    while (net->sim_in_progress) {
        pthread_cond_wait(&net->sim_done_cond, &net->sim_lock);
    }
    net->sim_in_progress = true;
    
    event_log_clear(&net->event_log);
    
    static int sim_counter = 1;
    int current_sim_id = sim_counter++;
    
    Packet *pkt = malloc(sizeof(Packet));
    memset(pkt, 0, sizeof(Packet));
    pkt->simulation_id = current_sim_id;
    pkt->direction = DIR_FORWARD;
    
    protocol_build_request(&pkt->frame.packet.segment.payload, proto, subproto, method, target, headers, body);
    
    // Inject packet into client's trigger queue
    queue_push(&net->q_client_trigger, pkt);
    
    // Wait until simulation completes (client signals sim_done_cond)
    while (net->sim_in_progress) {
        pthread_cond_wait(&net->sim_done_cond, &net->sim_lock);
    }
    
    char *json = event_log_to_json(&net->event_log);
    pthread_mutex_unlock(&net->sim_lock);
    
    return json;
}
