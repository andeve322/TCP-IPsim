#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "event.h"

static const char* node_to_str(SimNode node) {
    switch (node) {
        case NODE_CLIENT:    return "CLIENT";
        case NODE_REPEATER1: return "REPEATER_1";
        case NODE_ROUTER:    return "ROUTER";
        case NODE_REPEATER2: return "REPEATER_2";
        case NODE_SERVER:    return "SERVER";
        default:             return "UNKNOWN";
    }
}

static const char* proto_to_str(AppProtocol p) {
    switch (p) {
        case PROTO_HTTP: return "HTTP";
        case PROTO_DNS:  return "DNS";
        case PROTO_CDN:  return "CDN";
        case PROTO_SMTP: return "SMTP";
        case PROTO_P2P:  return "P2P";
        default:         return "UNKNOWN";
    }
}

void event_log_init(EventLog *log) {
    if (!log) return;
    log->count = 0;
    pthread_mutex_init(&log->lock, NULL);
}

void event_log_clear(EventLog *log) {
    if (!log) return;
    pthread_mutex_lock(&log->lock);
    log->count = 0;
    pthread_mutex_unlock(&log->lock);
}

void event_log_add(EventLog *log, int sim_id, PacketDirection dir, SimNode node, SimLayer layer, const char *action, const char *details, const Packet *pkt) {
    if (!log) return;
    
    pthread_mutex_lock(&log->lock);
    if (log->count < MAX_EVENTS) {
        SimulationEvent *ev = &log->events[log->count];
        ev->step_id = log->count + 1;
        ev->simulation_id = sim_id;
        ev->direction = dir;
        ev->node = node;
        ev->layer = layer;
        strncpy(ev->action, action ? action : "", sizeof(ev->action) - 1);
        strncpy(ev->details, details ? details : "", sizeof(ev->details) - 1);
        
        if (pkt) {
            ev->packet_snapshot = *pkt;
        } else {
            memset(&ev->packet_snapshot, 0, sizeof(Packet));
        }
        log->count++;
    }
    pthread_mutex_unlock(&log->lock);
}

static void escape_json(const char *src, char *dst, size_t max_dst) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] != '\0' && j + 2 < max_dst; i++) {
        if (src[i] == '"' || src[i] == '\\') {
            dst[j++] = '\\';
            dst[j++] = src[i];
        } else if (src[i] == '\n') {
            dst[j++] = '\\';
            dst[j++] = 'n';
        } else if (src[i] == '\r') {
            dst[j++] = '\\';
            dst[j++] = 'r';
        } else if (src[i] == '\t') {
            dst[j++] = '\\';
            dst[j++] = 't';
        } else if ((unsigned char)src[i] >= 32) {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static const char* port_to_desc(uint16_t port) {
    switch (port) {
        case 80:   return "HTTP (Port 80: Well-Known Web Server)";
        case 53:   return "DNS (Port 53: Well-Known Nameserver)";
        case 443:  return "HTTPS / CDN Edge (Port 443: TLS Secure Web)";
        case 25:   return "SMTP (Port 25: Well-Known Mail Transfer)";
        case 110:  return "POP3 (Port 110: Well-Known Mail Retrieval)";
        case 143:  return "IMAP (Port 143: Well-Known Mailbox Sync)";
        case 6881: return "BitTorrent (Port 6881: Registered Peer Wire)";
        default:
            if (port >= 49152) return "Ephemeral Client Socket (Dynamic OS Range 49152-65535)";
            return "Registered Service Port";
    }
}

static const char* transport_flags_to_desc(uint8_t proto, uint8_t flags) {
    if (proto == 17) return "UDP Datagram: Connectionless (No Flags / No Handshake)";
    if (flags & 0x02) return "SYN [0x02]: Connection Request (Initiating 3-Way Handshake)";
    if ((flags & 0x10) && (flags & 0x08)) return "ACK + PSH [0x18]: Acknowledged Data (Push immediately to Socket)";
    if (flags & 0x10) return "ACK [0x10]: Acknowledgment of Received Bytes";
    if (flags & 0x01) return "FIN [0x01]: Graceful Connection Teardown";
    return "TCP Stream Data";
}

static const char* proto_to_transport_rationale(AppProtocol proto) {
    switch (proto) {
        case PROTO_HTTP:
            return "HTTP uses TCP (Port 80/443): Requires lossless, in-order delivery of HTML and API data, backed by TCP flow control and retransmission timers.";
        case PROTO_DNS:
            return "DNS uses UDP (Port 53): Optimized for lightweight single-packet query and response, avoiding the 3-way handshake round-trip latency.";
        case PROTO_CDN:
            return "CDN uses TCP (Port 443/80): High-throughput video/asset streaming requires byte-level reliability and TCP sliding window congestion control.";
        case PROTO_SMTP:
            return "SMTP uses TCP (Port 25/110/143): State machine-driven email exchange requires reliable stream integrity so messages cannot arrive truncated or corrupted.";
        case PROTO_P2P:
            return "BitTorrent P2P uses TCP (Port 6881): Peer-to-peer chunk transfers require persistent full-duplex byte streams with accurate piece verification.";
        default:
            return "Standard L4 Transport Layer.";
    }
}

char* event_log_to_json(EventLog *log) {
    if (!log) return strdup("[]");
    
    pthread_mutex_lock(&log->lock);
    
    size_t buf_size = 1024 * 1024; // 1 MB buffer for complete timeline
    char *json = malloc(buf_size);
    if (!json) {
        pthread_mutex_unlock(&log->lock);
        return strdup("[]");
    }
    
    size_t offset = 0;
    offset += snprintf(json + offset, buf_size - offset, "[\n");
    
    char esc_action[256];
    char esc_details[512];
    char esc_headers[1024];
    char esc_body[2048];
    char esc_resp_body[2048];
    char esc_rationale[512];
    
    for (int i = 0; i < log->count; i++) {
        SimulationEvent *e = &log->events[i];
        Packet *p = &e->packet_snapshot;
        L2_Frame *f = &p->frame;
        L3_Packet *l3 = &f->packet;
        L4_Segment *l4 = &l3->segment;
        L7_Payload *l7 = &l4->payload;
        
        escape_json(e->action, esc_action, sizeof(esc_action));
        escape_json(e->details, esc_details, sizeof(esc_details));
        escape_json(l7->headers, esc_headers, sizeof(esc_headers));
        escape_json(l7->body, esc_body, sizeof(esc_body));
        escape_json(l7->response_body, esc_resp_body, sizeof(esc_resp_body));
        escape_json(proto_to_transport_rationale(l7->protocol), esc_rationale, sizeof(esc_rationale));
        
        offset += snprintf(json + offset, buf_size - offset,
            "  {\n"
            "    \"step_id\": %d,\n"
            "    \"sim_id\": %d,\n"
            "    \"direction\": \"%s\",\n"
            "    \"node\": \"%s\",\n"
            "    \"layer\": %d,\n"
            "    \"action\": \"%s\",\n"
            "    \"details\": \"%s\",\n"
            "    \"pdu\": {\n"
            "      \"l2\": {\"src_mac\": \"%s\", \"dest_mac\": \"%s\", \"ethertype\": \"0x%04x\"},\n"
            "      \"l3\": {\"src_ip\": \"%s\", \"dest_ip\": \"%s\", \"ttl\": %d, \"proto\": %d},\n"
            "      \"l4\": {\n"
            "        \"src_port\": %d,\n"
            "        \"src_port_desc\": \"%s\",\n"
            "        \"dest_port\": %d,\n"
            "        \"dest_port_desc\": \"%s\",\n"
            "        \"transport_proto\": \"%s\",\n"
            "        \"flags_desc\": \"%s\",\n"
            "        \"rationale\": \"%s\",\n"
            "        \"seq\": %u,\n"
            "        \"ack\": %u,\n"
            "        \"flags\": %d\n"
            "      },\n"
            "      \"l7\": {\n"
            "        \"protocol\": \"%s\",\n"
            "        \"method\": \"%s\",\n"
            "        \"target\": \"%s\",\n"
            "        \"headers\": \"%s\",\n"
            "        \"body\": \"%s\",\n"
            "        \"status_code\": %d,\n"
            "        \"status_msg\": \"%s\",\n"
            "        \"response_body\": \"%s\"\n"
            "      }\n"
            "    }\n"
            "  }%s\n",
            e->step_id,
            e->simulation_id,
            e->direction == DIR_FORWARD ? "FORWARD" : "REVERSE",
            node_to_str(e->node),
            e->layer,
            esc_action,
            esc_details,
            f->src_mac, f->dest_mac, f->ethertype,
            l3->src_ip, l3->dest_ip, l3->ttl, l3->protocol,
            l4->src_port,
            port_to_desc(l4->src_port),
            l4->dest_port,
            port_to_desc(l4->dest_port),
            (l4->transport_proto == 17) ? "UDP (Protocol 17)" : "TCP (Protocol 6)",
            transport_flags_to_desc(l4->transport_proto, l4->flags),
            esc_rationale,
            l4->seq_num, l4->ack_num, l4->flags,
            proto_to_str(l7->protocol),
            l7->method_or_cmd,
            l7->target,
            esc_headers,
            esc_body,
            l7->status_code,
            l7->status_message,
            esc_resp_body,
            (i < log->count - 1) ? "," : ""
        );
        
        if (offset >= buf_size - 4096) break;
    }
    
    snprintf(json + offset, buf_size - offset, "]\n");
    pthread_mutex_unlock(&log->lock);
    return json;
}
